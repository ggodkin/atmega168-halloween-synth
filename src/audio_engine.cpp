#include "audio_engine.h"
#include "config.h"
#include "noise.h"

namespace {
int16_t filteredNoise = 0;
int16_t deepNoise = 0;
uint16_t soundSample = 0;
uint8_t sound = Config::windSound;
uint16_t modulationPhase = 0;
uint16_t ghostPhase = 0;
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
    int16_t value = triangleSignal(phase);
    int16_t magnitude = value < 0 ? -value : value;
    return (value * (256 - magnitude)) >> 7;
}

void resetSequence() {
    filteredNoise = 0;
    deepNoise = 0;
    soundSample = 0;
    sound = Config::windSound;
    modulationPhase = 0;
    ghostPhase = 0;
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
    filteredNoise += (noise - filteredNoise) >> Config::filteredNoiseShift;
    deepNoise += (filteredNoise - deepNoise) >> Config::deepNoiseShift;

    uint16_t position = soundSample;
    int16_t sample = 0;

    switch (sound) {
    case Config::windSound: { // Slow gust modulation and layered low-passed noise make wind, not hiss.
        modulationPhase += Config::Wind::gustPhaseStep;
        uint8_t gust = triangleWave(modulationPhase);
        int16_t windNoise = (filteredNoise - deepNoise)
                / Config::Wind::filteredNoiseDivisor
            + deepNoise / Config::Wind::deepNoiseDivisor
            + noise / Config::Wind::rawNoiseDivisor;
        sample = windNoise
            * (Config::Wind::noiseScale + (gust >> Config::Wind::gustDepthShift))
            / Config::Wind::mixDivisor;
        break;
    }
    case Config::ghostSound: { // Pitch-vibrato on a strong near-sine tone gives the ghost a clear howl.
        modulationPhase += Config::Ghost::vibratoPhaseStep;
        uint8_t wobble = triangleWave(modulationPhase);
        uint16_t frequency = Config::Ghost::baseFrequency
            + (wobble >> Config::Ghost::wobbleShift);
        ghostPhase += frequency << Config::phaseIncrementShift;
        sample = sineSignal(ghostPhase) * Config::Ghost::toneNumerator
                / Config::Ghost::toneDivisor
            + noise / Config::Ghost::noiseScale;
        break;
    }
    case Config::thunderSound: { // A short crack leads into a separately decaying low rumble.
        modulationPhase += Config::Thunder::rumblePhaseStep;
        uint8_t rumbleDecay = position < Config::Thunder::rumbleDecaySamples
            ? 255 - position / Config::Thunder::rumbleDecayDivisor
            : 0;
        int16_t rumble = deepNoise
            + triangleSignal(modulationPhase) / Config::Thunder::rumbleToneDivisor;
        int16_t crack = position < Config::Thunder::crackSamples
            ? noise * ((Config::Thunder::crackSamples - position)
                >> Config::Thunder::crackLevelDivisor)
                / Config::Thunder::crackScale
            : 0;
        sample = crack
            + (rumble / Config::Thunder::rumbleToneDivisor) * rumbleDecay
                / Config::Thunder::rumbleOutputDivisor;
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
            squeakPhase += frequency << Config::phaseIncrementShift;
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
            << Config::phaseIncrementShift;
        bellPartialPhase += Config::Bell::partialFrequency
            << Config::phaseIncrementShift;
        bellHighPartialPhase += Config::Bell::highPartialFrequency
            << Config::phaseIncrementShift;
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
            owlPhase += frequency << Config::phaseIncrementShift;
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
            ghostPhase = 0;
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
