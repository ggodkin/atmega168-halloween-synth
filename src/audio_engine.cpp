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
const uint8_t bellExponentialDecay[16] PROGMEM = {
    255, 218, 187, 160, 137, 117, 100, 86,
    74, 63, 54, 46, 39, 33, 28, 24
};
const int8_t owlWaveTable[64] PROGMEM = {
    0, 12, 24, 36, 48, 60, 71, 81, 91, 99, 107, 113, 117, 119, 120, 118,
    115, 110, 104, 96, 87, 77, 67, 58, 48, 39, 31, 24, 18, 13, 8, 4,
    0, -4, -8, -13, -18, -24, -31, -39, -48, -58, -67, -77, -87, -96, -104, -110,
    -115, -118, -120, -119, -117, -113, -107, -99, -91, -81, -71, -60, -48, -36, -24, -12
};
const int8_t owlPitchContour[Config::Owl::contourEntryCount] PROGMEM = {
    32, 28, 20, 10, 0, -10, -20, -30, -40, -50, -60, -70, -80, -90,
    -100, -105, -110, -108, -104, -98, -90, -80, -68, -54, -40, -28, -16, -6,
    -2, 0, 0, 0
};
const int8_t owlLevelContour[Config::Owl::contourEntryCount] PROGMEM = {
    0, 12, 32, 62, 90, 112, 124, 127, 127, 126, 124, 121, 117, 112, 106, 99,
    91, 82, 72, 61, 50, 39, 29, 20, 13, 8, 4, 2, 1, 0, 0, 0
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
uint16_t secondSqueakPhase = 0;
uint16_t thirdSqueakPhase = 0;
uint16_t doorResonancePhase = 0;
uint16_t bellFundamentalPhase = 0;
uint16_t bellPartialPhase = 0;
uint16_t bellHighPartialPhase = 0;
uint16_t bellFourthPartialPhase = 0;
uint16_t bellStrikePhase = 0;
int8_t bellFundamentalDetune = 0;
int8_t bellPartialDetune = 0;
int8_t bellHighPartialDetune = 0;
int8_t bellFourthPartialDetune = 0;
uint16_t owlPhase = 0;
uint16_t owlChorusPhase = 0;
uint16_t owlVibratoPhase = 0;
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

int16_t interpolateOwlContour(const int8_t *table, uint16_t position) {
    uint8_t index = position >> Config::Owl::contourStepShift;
    if (index >= Config::Owl::contourEntryCount - 1) {
        return (int8_t)pgm_read_byte(&table[Config::Owl::contourEntryCount - 1]);
    }

    uint8_t fraction = position & ((1 << Config::Owl::contourStepShift) - 1);
    int8_t first = (int8_t)pgm_read_byte(&table[index]);
    int8_t second = (int8_t)pgm_read_byte(&table[index + 1]);
    return first + ((int16_t)(second - first) * fraction
        >> Config::Owl::contourStepShift);
}

uint8_t bellDecay(uint16_t position, uint8_t intervalShift) {
    uint8_t index = position >> intervalShift;
    if (index > 15) return 0;
    return pgm_read_byte(&bellExponentialDecay[index]);
}

int8_t randomBellDetune() {
    return (int8_t)(noise8() >> 5) - Config::Bell::randomDetuneRange;
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

int16_t renderDoorCreak(
    uint16_t position,
    uint16_t start,
    uint16_t end,
    uint16_t baseFrequency,
    uint16_t catchStart,
    uint16_t catchEnd,
    uint16_t &phase,
    uint8_t &resonanceGain,
    int16_t noise,
    int16_t filteredNoise,
    int16_t deepNoise
) {
    if (position < start || position >= end) {
        return 0;
    }

    uint16_t age = position - start;
    uint16_t edge = age;
    uint16_t release = end - start - age;
    if (release < edge) edge = release;
    uint8_t envelope = edge < Config::Door::attackSamples
        ? edge >> Config::Door::attackEnvelopeShift
        : Config::Door::creakEnvelope;

    int16_t jitter = filteredNoise / Config::Door::pitchJitterDivisor
        + noise / Config::Door::rawPitchJitterDivisor;
    int16_t frequency = baseFrequency + (age >> Config::Door::pitchStepShift) + jitter;
    if (frequency < 100) frequency = 100;
    phase += (uint16_t)frequency * Config::phaseIncrementScale;

    int16_t friction = filteredNoise - deepNoise;
    int16_t creak = triangleSignal(phase) * envelope / Config::Door::waveformScale
        + friction * envelope / Config::Door::waveformScale;
    if (age >= catchStart && age < catchEnd) {
        creak >>= 3;
    }

    if (age < Config::Door::resonanceDecaySamples) {
        uint16_t decay = Config::Door::resonanceDecaySamples - age;
        uint8_t resonance = envelope * decay / Config::Door::resonanceDecaySamples;
        if (resonance > resonanceGain) resonanceGain = resonance;
    }
    return creak;
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
    secondSqueakPhase = 0;
    thirdSqueakPhase = 0;
    doorResonancePhase = 0;
    bellFundamentalPhase = 0;
    bellPartialPhase = 0;
    bellHighPartialPhase = 0;
    bellFourthPartialPhase = 0;
    bellStrikePhase = 0;
    bellFundamentalDetune = 0;
    bellPartialDetune = 0;
    bellHighPartialDetune = 0;
    bellFourthPartialDetune = 0;
    owlPhase = 0;
    owlChorusPhase = 0;
    owlVibratoPhase = 0;
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
    uint16_t activeSoundSamples = Config::samplesForSound(sound);
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
        int16_t highPassedNoise = noise - pinkFast;
        int16_t crack = position < Config::Thunder::crackSamples
            ? highPassedNoise * ((Config::Thunder::crackSamples - position)
                >> Config::Thunder::crackLevelShift)
                / Config::Thunder::crackScale
            : 0;
        int16_t click = position == 0 ? Config::Thunder::crackClickLevel : 0;
        sample = position == 0 ? click : crack + deepRumble + lowRumbleTone;
        break;
    }
    case Config::doorSound: { // Overlapping jittered creaks have brief catches and metallic ring.
        uint8_t resonanceGain = 0;
        sample = renderDoorCreak(position,
                Config::Door::firstCreakStart, Config::Door::firstCreakEnd,
                Config::Door::firstBaseFrequency,
                Config::Door::firstCatchStart, Config::Door::firstCatchEnd,
                squeakPhase, resonanceGain, noise, filteredNoise, deepNoise)
            + renderDoorCreak(position,
                Config::Door::secondCreakStart, Config::Door::secondCreakEnd,
                Config::Door::secondBaseFrequency,
                Config::Door::secondCatchStart, Config::Door::secondCatchEnd,
                secondSqueakPhase, resonanceGain, noise, filteredNoise, deepNoise)
            + renderDoorCreak(position,
                Config::Door::thirdCreakStart, Config::Door::thirdCreakEnd,
                Config::Door::thirdBaseFrequency,
                Config::Door::thirdCatchStart, Config::Door::thirdCatchEnd,
                thirdSqueakPhase, resonanceGain, noise, filteredNoise, deepNoise);
        doorResonancePhase += Config::Door::resonanceFrequency
            * Config::phaseIncrementScale;
        sample += sineSignal(doorResonancePhase) * resonanceGain
            / Config::Door::resonanceOutputDivisor;
        if (!resonanceGain) sample += noise / Config::Door::idleNoiseDivisor;
        break;
    }
    case Config::bellSound: { // A noisy strike excites inharmonic partials with exponential ring-down.
        if (position == 0) {
            bellFundamentalDetune = randomBellDetune();
            bellPartialDetune = randomBellDetune();
            bellHighPartialDetune = randomBellDetune();
            bellFourthPartialDetune = randomBellDetune();
        }
        bellFundamentalPhase += (Config::Bell::fundamentalFrequency
            + bellFundamentalDetune)
            * Config::phaseIncrementScale;
        bellPartialPhase += (Config::Bell::partialFrequency + bellPartialDetune)
            * Config::phaseIncrementScale;
        bellHighPartialPhase += (Config::Bell::highPartialFrequency
            + bellHighPartialDetune)
            * Config::phaseIncrementScale;
        bellFourthPartialPhase += (Config::Bell::fourthPartialFrequency
            + bellFourthPartialDetune) * Config::phaseIncrementScale;
        bellStrikePhase += Config::Bell::strikeToneFrequency
            * Config::phaseIncrementScale;
        uint8_t fundamentalDecay = bellDecay(position, Config::Bell::fundamentalDecayShift);
        uint8_t partialDecay = bellDecay(position, Config::Bell::partialDecayShift);
        uint8_t highDecay = bellDecay(position, Config::Bell::highDecayShift);
        uint8_t fourthDecay = bellDecay(position, Config::Bell::fourthDecayShift);
        sample = (sineSignal(bellFundamentalPhase) * fundamentalDecay
                >> Config::Bell::fundamentalMixShift)
            + (sineSignal(bellPartialPhase) * partialDecay
                >> Config::Bell::partialMixShift)
            + (sineSignal(bellHighPartialPhase) * highDecay
                >> Config::Bell::highPartialMixShift)
            + (sineSignal(bellFourthPartialPhase) * fourthDecay
                >> Config::Bell::fourthPartialMixShift);
        if (position < Config::Bell::strikeNoiseSamples) {
            sample += noise * (Config::Bell::strikeNoiseSamples - position) >> 9;
        }
        if (position < Config::Bell::strikeToneSamples) {
            uint8_t strikeEnvelope = (Config::Bell::strikeToneSamples - position) * 10;
            sample += sineSignal(bellStrikePhase) * strikeEnvelope
                >> 10;
        }
        break;
    }
    case Config::owlSound: { // Flash wavetable and interpolated contours shape soft, paired owl calls.
        uint16_t cycle = position % Config::Owl::cycleSamples;
        uint16_t hootPosition = 0;
        bool hooting = false;
        bool secondHoot = false;
        if (cycle >= Config::Owl::firstHootStart && cycle < Config::Owl::firstHootEnd) {
            hootPosition = cycle - Config::Owl::firstHootStart;
            hooting = true;
        } else if (cycle >= Config::Owl::secondHootStart
            && cycle < Config::Owl::secondHootEnd) {
            hootPosition = cycle - Config::Owl::secondHootStart;
            hooting = true;
            secondHoot = true;
        }

        if (hooting) {
            int16_t frequency = Config::Owl::baseFrequency
                + interpolateOwlContour(owlPitchContour, hootPosition);
            if (secondHoot) frequency -= Config::Owl::secondHootOffset;
            owlVibratoPhase += Config::Owl::vibratoPhaseStep;
            frequency += sineSignal(owlVibratoPhase) / Config::Owl::vibratoDivisor;
            if (frequency < 200) frequency = 200;
            owlPhase += frequency * Config::phaseIncrementScale;
            owlChorusPhase += (frequency + Config::Owl::chorusDetuneHz)
                * Config::phaseIncrementScale;
            int16_t envelope = interpolateOwlContour(owlLevelContour, hootPosition);
            int16_t waveform = (int8_t)pgm_read_byte(&owlWaveTable[owlPhase >> 10]);
            int16_t tone = (waveform * Config::Owl::chorusMainWeight
                + sineSignal(owlChorusPhase))
                / (Config::Owl::chorusMainWeight + 1);
            uint8_t tremolo = 224 + sineSignal(owlVibratoPhase)
                / Config::Owl::tremoloDepthDivisor;
            sample = (tone * tremolo >> 8) * envelope
                >> Config::Owl::levelScaleShift;
            if (hootPosition < Config::Owl::breathSamples) {
                uint16_t remainingBreath = Config::Owl::breathSamples - hootPosition;
                int16_t breathEnvelope = envelope * remainingBreath
                    >> Config::Owl::contourStepShift;
                sample += (filteredNoise * breathEnvelope)
                    >> Config::Owl::breathMixShift;
            }
        }
        break;
    }
    }

    uint16_t fadeSamples = sound == Config::bellSound
        ? Config::Bell::fadeSamples
        : Config::soundFadeSamples;
    uint8_t fade = 255;
    if (position < fadeSamples) {
        fade = position;
    } else if (position >= activeSoundSamples - fadeSamples) {
        fade = activeSoundSamples - 1 - position;
    }
    sample = (sample * fade) >> 8;

    if (++soundSample == activeSoundSamples) {
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
            secondSqueakPhase = 0;
            thirdSqueakPhase = 0;
            doorResonancePhase = 0;
            bellFundamentalPhase = 0;
            bellPartialPhase = 0;
            bellHighPartialPhase = 0;
            bellFourthPartialPhase = 0;
            bellStrikePhase = 0;
            bellFundamentalDetune = 0;
            bellPartialDetune = 0;
            bellHighPartialDetune = 0;
            bellFourthPartialDetune = 0;
            owlPhase = 0;
            owlChorusPhase = 0;
            owlVibratoPhase = 0;
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
