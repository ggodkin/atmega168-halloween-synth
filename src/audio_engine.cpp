#include <avr/pgmspace.h>
#include "audio_engine.h"
#include "config.h"
#include "noise.h"

namespace {
// A 64-entry quarter wave reconstructs a smooth, signed cycle by symmetry.
const uint8_t sineQuarterWave[64] PROGMEM = {
    0, 3, 6, 9, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
    49, 52, 55, 58, 61, 64, 66, 69, 72, 74, 77, 79, 82, 84, 86, 89,
    91, 93, 95, 97, 99, 101, 103, 105, 107, 108, 110, 112, 113, 114, 116, 117,
    118, 119, 120, 121, 122, 123, 124, 124, 125, 126, 126, 126, 127, 127, 127, 127
};

int16_t filteredNoise = 0;
int16_t deepNoise = 0;
int16_t pinkFast = 0;
int16_t pinkMid = 0;
int16_t pinkSlow = 0;
int16_t windAirNoise = 0;
int16_t windHissNoise = 0;
uint16_t soundSample = 0;
uint8_t sound = Config::windSound;
uint16_t modulationPhase = 0;
uint16_t slowGustPhase = 0;
uint16_t ghostPhase = 0;
uint16_t ghostChorusPhase = 0;
uint16_t squeakPhase = 0;
uint16_t bellFundamentalPhase = 0;
uint16_t bellPartialPhase = 0;
uint16_t bellHighPartialPhase = 0;
uint16_t owlPhase = 0;
// An 8-bit event request is atomic to consume from the audio ISR.
volatile uint8_t triggerRequested = 0;
uint8_t sequencePlaying = 0;
uint8_t outputGain = 0;

uint8_t triangleWave(uint16_t phase) {
    uint8_t value = (phase & 0x7FFF) >> 7;
    return (phase & 0x8000) ? 255 - value : value;
}

int16_t triangleSignal(uint16_t phase) {
    return (int16_t)triangleWave(phase) - 128;
}

int16_t sineSignal(uint16_t phase) {
    uint8_t quadrant = phase >> 14;
    uint8_t index = (phase >> 8) & 0x3F;
    if (quadrant & 1) {
        index = 63 - index;
    }

    int16_t value = pgm_read_byte(&sineQuarterWave[index]);
    return (quadrant & 2) ? -value : value;
}

uint8_t thunderBoomGain(uint16_t position, uint16_t start) {
    uint16_t age = position - start;
    if (position < start || age >= Config::Thunder::boomAttackSamples
            + Config::Thunder::boomDecaySamples) {
        return 0;
    }
    if (age < Config::Thunder::boomAttackSamples) {
        return age >> Config::Thunder::boomAttackShift;
    }
    return Config::Thunder::boomPeakGain
        - ((age - Config::Thunder::boomAttackSamples)
            >> Config::Thunder::boomDecayShift);
}

void resetSequence() {
    filteredNoise = 0;
    deepNoise = 0;
    pinkFast = 0;
    pinkMid = 0;
    pinkSlow = 0;
    windAirNoise = 0;
    windHissNoise = 0;
    soundSample = 0;
    sound = Config::windSound;
    modulationPhase = 0;
    slowGustPhase = 0;
    ghostPhase = 0;
    ghostChorusPhase = 0;
    squeakPhase = 0;
    bellFundamentalPhase = 0;
    bellPartialPhase = 0;
    bellHighPartialPhase = 0;
    owlPhase = 0;
    outputGain = 0;
}
}

void AudioEngine::begin() {
    triggerRequested = 0;
    sequencePlaying = 0;
    resetSequence();
}

void AudioEngine::trigger() {
    triggerRequested = 1;
}

uint8_t AudioEngine::nextSample() {
    if (triggerRequested) {
        triggerRequested = 0;
        // Triggers received while playing are consumed and intentionally discarded.
        if (!sequencePlaying) {
            resetSequence();
            sequencePlaying = 1;
        }
    }

    if (!sequencePlaying) {
        return 128;
    }
    // Fade in at the start; the final sound's envelope fades the sequence out.
    outputGain = outputGain > 255 - Config::outputGainStep
        ? 255
        : outputGain + Config::outputGainStep;

    int16_t noise = (int16_t)noise8() - 128;
    // Weighted parallel low-pass bands approximate equal energy per octave (pink noise).
    pinkFast += (noise - pinkFast) >> Config::PinkNoise::fastFilterShift;
    pinkMid += (noise - pinkMid) >> Config::PinkNoise::midFilterShift;
    pinkSlow += (noise - pinkSlow) >> Config::PinkNoise::slowFilterShift;
    filteredNoise = (noise >> Config::PinkNoise::whiteWeightShift)
        + (pinkFast >> Config::PinkNoise::fastWeightShift)
        + (pinkMid >> Config::PinkNoise::midWeightShift)
        + (pinkSlow >> Config::PinkNoise::slowWeightShift);
    deepNoise += (filteredNoise - deepNoise) >> Config::deepNoiseShift;

    uint16_t position = soundSample;
    int16_t sample = 0;

    switch (sound) {
    case Config::windSound: { // Layered gusts shape slow airflow and faster filtered hiss.
        modulationPhase += Config::Wind::gustPhaseStep;
        slowGustPhase += Config::Wind::slowGustPhaseStep;
        uint8_t gust = triangleWave(modulationPhase);
        uint8_t slowGust = triangleWave(slowGustPhase);
        windAirNoise += (noise - windAirNoise) >> Config::Wind::airFilterShift;
        windHissNoise += (noise - windHissNoise) >> Config::Wind::hissFilterShift;
        int16_t windNoise = (filteredNoise - deepNoise)
                / Config::Wind::filteredNoiseDivisor
            + deepNoise / Config::Wind::deepNoiseDivisor
            + noise / Config::Wind::rawNoiseDivisor
            + windAirNoise / Config::Wind::airMovementDivisor
            + windHissNoise / Config::Wind::hissDivisor;
        int16_t gustGain = Config::Wind::baseGain
            + (gust >> Config::Wind::fastGustDepthShift)
            + (slowGust >> Config::Wind::slowGustDepthShift);
        sample = (windNoise / Config::Wind::windNoiseInputDivisor) * gustGain
            / Config::Wind::mixDivisor;
        break;
    }
    case Config::ghostSound: { // Slightly detuned, tremolo-modulated voices fade through the sound slot.
        modulationPhase += Config::Ghost::vibratoPhaseStep;
        uint8_t wobble = triangleWave(modulationPhase);
        uint16_t frequency = Config::Ghost::baseFrequency
            + (wobble >> Config::Ghost::wobbleShift);
        ghostPhase += frequency * Config::phaseIncrementScale;
        ghostChorusPhase += (frequency + Config::Ghost::chorusDetuneHz)
            * Config::phaseIncrementScale;
        int16_t chorusTone = (sineSignal(ghostPhase) * Config::Ghost::chorusMainWeight
            + sineSignal(ghostChorusPhase)) / (Config::Ghost::chorusMainWeight + 1);
        uint8_t tremolo = Config::Ghost::tremoloBase
            + (wobble >> Config::Ghost::tremoloDepthShift);
        int16_t ghostTone = chorusTone * tremolo >> 8;
        ghostTone = ghostTone * Config::Ghost::toneNumerator
            / Config::Ghost::toneDivisor;
        uint8_t decay = 255 - (position >> Config::Ghost::decayShift);
        sample = (ghostTone + noise / Config::Ghost::noiseScale) * decay >> 8;
        break;
    }
    case Config::thunderSound: { // A filtered crack gives way to slow, pulsing low-frequency rumble.
        modulationPhase += Config::Thunder::rumblePhaseStep;
        uint8_t rumbleDecay = position < Config::Thunder::rumbleDecaySamples
            ? 255 - position / Config::Thunder::rumbleDecayDivisor
            : 0;
        uint8_t boomGain = thunderBoomGain(position, Config::Thunder::firstBoomStart);
        uint8_t nextBoom = thunderBoomGain(position, Config::Thunder::secondBoomStart);
        if (nextBoom > boomGain) boomGain = nextBoom;
        nextBoom = thunderBoomGain(position, Config::Thunder::thirdBoomStart);
        if (nextBoom > boomGain) boomGain = nextBoom;
        uint8_t rumbleGain = Config::Thunder::rumbleBaseGain
            + (triangleWave(modulationPhase) >> Config::Thunder::rumbleModulationShift)
            + boomGain;
        int16_t decayedDeepNoise = (deepNoise / 2) * rumbleDecay
            / Config::Thunder::rumbleOutputDivisor;
        int16_t deepRumble = decayedDeepNoise * rumbleGain
            / Config::Thunder::rumbleGainDivisor;
        int16_t lowRumbleTone = triangleSignal(modulationPhase)
            / Config::Thunder::rumbleToneDivisor * rumbleDecay
            / Config::Thunder::rumbleOutputDivisor;
        int16_t crack = position < Config::Thunder::crackSamples
            ? filteredNoise * ((Config::Thunder::crackSamples - position)
                >> Config::Thunder::crackLevelDivisor)
                / Config::Thunder::crackScale
            : 0;
        sample = crack + deepRumble + lowRumbleTone;
        break;
    }
    case Config::doorSound: { // Narrow pitch movement plus friction noise makes a hinge creak, not a siren.
        uint16_t squeakPosition = 0;
        uint16_t squeakDuration = 0;
        if (position >= Config::Door::firstCreakStart
            && position < Config::Door::firstCreakEnd) {
            squeakPosition = position - Config::Door::firstCreakStart;
            squeakDuration = Config::Door::firstCreakEnd - Config::Door::firstCreakStart;
        } else if (position >= Config::Door::secondCreakStart
            && position < Config::Door::secondCreakEnd) {
            squeakPosition = position - Config::Door::secondCreakStart;
            squeakDuration = Config::Door::secondCreakEnd - Config::Door::secondCreakStart;
        }

        if (squeakDuration) {
            uint16_t edge = squeakPosition;
            uint16_t release = squeakDuration - squeakPosition;
            if (release < edge) edge = release;
            uint8_t envelope = edge < Config::Door::attackSamples
                ? edge >> Config::Door::attackEnvelopeShift
                : Config::Door::creakEnvelope;
            int16_t pitchJitter = filteredNoise / Config::Door::pitchJitterDivisor;
            uint16_t frequency = Config::Door::baseFrequency
                + (squeakPosition >> Config::Door::pitchStepShift)
                + pitchJitter;
            squeakPhase += frequency * Config::phaseIncrementScale;
            int16_t friction = filteredNoise - deepNoise;
            sample = triangleSignal(squeakPhase) * envelope / Config::Door::waveformScale
                + friction * envelope / Config::Door::waveformScale;
        } else {
            sample = noise / Config::Door::idleNoiseDivisor;
        }
        break;
    }
    case Config::bellSound: { // Inharmonic partials ring down at different rates after a noisy strike.
        bellFundamentalPhase += Config::Bell::fundamentalFrequency
            * Config::phaseIncrementScale;
        bellPartialPhase += Config::Bell::partialFrequency
            * Config::phaseIncrementScale;
        bellHighPartialPhase += Config::Bell::highPartialFrequency
            * Config::phaseIncrementScale;
        uint8_t fundamentalDecay = position < Config::Bell::fundamentalDecaySamples
            ? 255 - position / Config::Bell::fundamentalDecayDivisor
            : 0;
        uint8_t partialDecay = position < Config::Bell::partialDecaySamples
            ? Config::Bell::partialDecayBase - position / Config::Bell::partialDecayDivisor
            : Config::Bell::partialDecayFloor;
        uint8_t highDecay = position < Config::Bell::highDecaySamples
            ? Config::Bell::highDecayBase - position / Config::Bell::highDecayDivisor
            : Config::Bell::highDecayFloor;
        sample = sineSignal(bellFundamentalPhase) * fundamentalDecay
                / Config::Bell::fundamentalMixDivisor
            + sineSignal(bellPartialPhase) * partialDecay / Config::Bell::partialMixDivisor
            + sineSignal(bellHighPartialPhase) * highDecay
                / Config::Bell::highPartialMixDivisor;
        if (position < Config::Bell::strikeSamples) {
            sample += noise * (Config::Bell::strikeSamples - position)
                / Config::Bell::strikeSamples;
        }
        break;
    }
    case Config::owlSound: { // Paired owl hoots use a breathy attack and a rounded falling contour.
        uint16_t cycle = position % Config::Owl::cycleSamples;
        uint16_t hootPosition = 0;
        bool hooting = false;
        if (cycle >= Config::Owl::firstHootStart && cycle < Config::Owl::firstHootEnd) {
            hootPosition = cycle - Config::Owl::firstHootStart;
            hooting = true;
        } else if (cycle >= Config::Owl::secondHootStart
            && cycle < Config::Owl::secondHootEnd) {
            hootPosition = cycle - Config::Owl::secondHootStart;
            hooting = true;
        }

        if (hooting) {
            uint16_t contour = hootPosition < Config::Owl::pitchPeakSample
                ? hootPosition
                : (Config::Owl::firstHootEnd - Config::Owl::firstHootStart) - hootPosition;
            uint16_t frequency = Config::Owl::baseFrequency
                + (contour >> Config::Owl::pitchStepShift);
            owlPhase += frequency * Config::phaseIncrementScale;
            uint16_t hootLength = Config::Owl::firstHootEnd - Config::Owl::firstHootStart;
            uint16_t edge = hootPosition < Config::Owl::attackSamples
                ? hootPosition
                : hootLength - hootPosition;
            uint8_t envelope = edge < Config::Owl::envelopeRampSamples
                ? edge >> Config::Owl::envelopeShift
                : Config::Owl::envelopeLevel;
            sample = sineSignal(owlPhase) * envelope / Config::Owl::toneMixDivisor
                + noise / Config::Owl::noiseScale;
        }
        break;
    }
    }

    uint8_t fade = 255;
    if (position < Config::soundFadeSamples) {
        fade = position;
    } else if (position >= Config::samplesPerSound - Config::soundFadeSamples) {
        fade = Config::samplesPerSound - 1 - position;
    }
    sample = (sample * fade) >> 8;

    if (++soundSample == Config::samplesPerSound) {
        soundSample = 0;
        if (sound + 1 == Config::soundCount) {
            sequencePlaying = 0;
            outputGain = 0;
        } else {
            ++sound;
            modulationPhase = 0;
            slowGustPhase = 0;
            ghostPhase = 0;
            ghostChorusPhase = 0;
            squeakPhase = 0;
            bellFundamentalPhase = 0;
            bellPartialPhase = 0;
            bellHighPartialPhase = 0;
            owlPhase = 0;
        }
    }

    if (sample < -128) sample = -128;
    if (sample > 127) sample = 127;
    sample = sample * outputGain / 255;
    sample += 128;
    if (sample < 0) sample = 0;
    if (sample > 255) sample = 255;
    return (uint8_t)sample;
}
