#include <avr/pgmspace.h>
#include "audio_engine.h"
#include "config.h"
#include "door_squeak_samples.h"
#include "noise.h"
#include "owl_samples.h"

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
const uint8_t bellAttackEnvelope[32] PROGMEM = {
    0, 1, 3, 7, 12, 18, 25, 33, 42, 52, 62, 74, 85, 97, 109, 121,
    134, 146, 158, 170, 181, 193, 203, 213, 222, 230, 237, 243, 248, 252, 254, 255
};
const int8_t owlWaveTable[64] PROGMEM = {
    0, 12, 24, 36, 48, 60, 71, 81, 91, 99, 107, 113, 117, 119, 120, 118,
    115, 110, 104, 96, 87, 77, 67, 58, 48, 39, 31, 24, 18, 13, 8, 4,
    0, -4, -8, -13, -18, -24, -31, -39, -48, -58, -67, -77, -87, -96, -104, -110,
    -115, -118, -120, -119, -117, -113, -107, -99, -91, -81, -71, -60, -48, -36, -24, -12
};
const int8_t owlPitchContour[Config::Owl::contourEntryCount] PROGMEM = {
    32, 28, 24, 20, 16, 12, 8, 4, 0, -4, -8, -12, -16, -20, -24, -28,
    -32, -36, -40, -44, -48, -52, -56, -60, -64, -68, -72, -76, -80, -84, -88, -92,
    -96, -100, -104, -108, -110, -110, -110, -110
};
const int8_t owlLevelContour[Config::Owl::contourEntryCount] PROGMEM = {
    0, 8, 20, 40, 65, 88, 108, 122, 127, 127, 126, 124, 121, 117, 112, 106,
    99, 91, 82, 72, 61, 50, 40, 32, 25, 19, 14, 10, 7, 5, 3, 2,
    1, 1, 1, 1, 1, 0, 0, 0
};

int16_t filteredNoise = 0;
int16_t deepNoise = 0;
int16_t pinkFast = 0;
int16_t pinkMid = 0;
int16_t pinkSlow = 0;
int16_t windAirNoise = 0;
int16_t windHissNoise = 0;
uint16_t soundSample = 0;
uint16_t ledFadePhase = 0;
uint8_t sound = Config::windSound;
uint16_t modulationPhase = 0;
uint16_t slowGustPhase = 0;
uint32_t doorSamplePhase = 0;
uint16_t bellFundamentalPhase = 0;
uint16_t bellPartialPhase = 0;
uint16_t bellHighPartialPhase = 0;
uint16_t bellFourthPartialPhase = 0;
int8_t bellFundamentalDetune = 0;
int8_t bellPartialDetune = 0;
int8_t bellHighPartialDetune = 0;
int8_t bellFourthPartialDetune = 0;
uint16_t owlRecordingPosition = 0;
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

int16_t sineSignal(uint16_t phase) {
    uint8_t quadrant = phase >> 14;
    uint8_t index = (phase >> 8) & 0x3F;
    if (quadrant & 1) {
        index = 63 - index;
    }

    int16_t value = pgm_read_byte(&sineQuarterWave[index]);
    return (quadrant & 2) ? -value : value;
}

uint8_t ledFadeCurve(uint8_t progress) {
    if (progress < 128) {
        uint16_t value = progress;
        return (value * value * 2 + 127) / 255;
    }

    uint16_t remaining = 255 - progress;
    uint16_t fade = (remaining * remaining * 2 + 254) / 255;
    return 255 - fade;
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

uint8_t bellAttack(uint16_t position) {
    uint8_t index = position >> Config::Bell::attackTableStepShift;
    if (index >= Config::Bell::attackTableSize - 1) return 255;
    uint8_t fraction = position & ((1 << Config::Bell::attackTableStepShift) - 1);
    uint8_t first = pgm_read_byte(&bellAttackEnvelope[index]);
    uint8_t second = pgm_read_byte(&bellAttackEnvelope[index + 1]);
    return first + ((uint16_t)(second - first) * fraction
        >> Config::Bell::attackTableStepShift);
}

int8_t randomBellDetune() {
    return (int8_t)(noise8() >> 5) - Config::Bell::randomDetuneRange;
}

int16_t renderGhostCall(uint16_t position) {
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

    if (!hooting) return 0;

    uint16_t contourPosition = (uint32_t)hootPosition * 21 / 10;
    int16_t frequency = Config::Owl::baseFrequency
        + interpolateOwlContour(owlPitchContour, contourPosition);
    if (secondHoot) frequency += Config::Owl::secondHootOffset;
    owlVibratoPhase += Config::Owl::vibratoPhaseStep;
    frequency += sineSignal(owlVibratoPhase) / Config::Owl::vibratoDivisor;
    if (frequency < 200) frequency = 200;
    owlPhase += frequency * Config::phaseIncrementScale;
    owlChorusPhase += (frequency + Config::Owl::chorusDetuneHz)
        * Config::phaseIncrementScale;
    int16_t envelope = interpolateOwlContour(owlLevelContour, contourPosition);
    envelope = envelope * (secondHoot
        ? Config::Owl::secondHootLevelPercent
        : Config::Owl::firstHootLevelPercent) / 100;
    int16_t waveform = (int8_t)pgm_read_byte(&owlWaveTable[owlPhase >> 10]);
    int16_t tone = (waveform * Config::Owl::chorusMainWeight
        + sineSignal(owlChorusPhase))
        / (Config::Owl::chorusMainWeight + 1);
    uint8_t tremolo = 224 + sineSignal(owlVibratoPhase)
        / Config::Owl::tremoloDepthDivisor;
    int16_t sample = (tone * tremolo >> 8) * envelope
        >> Config::Owl::levelScaleShift;
    if (hootPosition < Config::Owl::breathSamples) {
        uint16_t remainingBreath = Config::Owl::breathSamples - hootPosition;
        int16_t breathEnvelope = envelope * remainingBreath
            >> Config::Owl::breathEnvelopeShift;
        sample += (filteredNoise * breathEnvelope)
            >> Config::Owl::breathMixShift;
    }
    return sample;
}

int16_t renderOwlRecording() {
    uint16_t sourceIndex = owlRecordingPosition
        / Config::Owl::sampleUpsampleFactor;
    uint16_t fraction = ((uint32_t)(owlRecordingPosition
        % Config::Owl::sampleUpsampleFactor) << 16)
        / Config::Owl::sampleUpsampleFactor;
    int16_t sample = 0;
    if (sourceIndex >= OWL_SAMPLE_START
            && sourceIndex < OWL_SAMPLE_START + OWL_SAMPLE_COUNT) {
        uint16_t index = sourceIndex - OWL_SAMPLE_START;
        uint8_t first = pgm_read_byte(&owlSamples[index]);
        uint8_t second = index + 1 < OWL_SAMPLE_COUNT
            ? pgm_read_byte(&owlSamples[index + 1])
            : 128;
        sample = (int16_t)first - 128
            + ((int32_t)((int16_t)second - first) * fraction >> 16);
    }

    if (++owlRecordingPosition == Config::audioSampleRate) {
        owlRecordingPosition = 0;
    }
    return sample;
}

int16_t renderDoorSqueak() {
    uint16_t index = doorSamplePhase >> 16;
    if (index >= DOOR_SQUEAK_SAMPLE_COUNT) {
        return 0;
    }

    uint8_t first = pgm_read_byte(&doorSqueakSamples[index]);
    uint8_t second = index + 1 < DOOR_SQUEAK_SAMPLE_COUNT
        ? pgm_read_byte(&doorSqueakSamples[index + 1])
        : first;
    uint16_t fraction = doorSamplePhase;
    int16_t sample = (int16_t)first - 128
        + ((int32_t)((int16_t)second - first) * fraction >> 16);
    doorSamplePhase += Config::Door::phaseIncrement;
    return sample;
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
    ledFadePhase = 0;
    sound = Config::windSound;
    modulationPhase = 0;
    slowGustPhase = 0;
    doorSamplePhase = 0;
    bellFundamentalPhase = 0;
    bellPartialPhase = 0;
    bellHighPartialPhase = 0;
    bellFourthPartialPhase = 0;
    bellFundamentalDetune = 0;
    bellPartialDetune = 0;
    bellHighPartialDetune = 0;
    bellFourthPartialDetune = 0;
    owlRecordingPosition = 0;
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
        OCR0B = 255;
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
    uint16_t activeSoundSamples = Config::samplesPerSound;
    uint16_t fadePosition;
    if (ledFadePhase < Config::ledFadeInSamples) {
        fadePosition = ledFadePhase;
    } else if (ledFadePhase
            < Config::ledFadeInSamples + Config::ledFadeOutSamples) {
        fadePosition = Config::ledFadeOutSamples - 1
            - (ledFadePhase - Config::ledFadeInSamples);
    } else {
        fadePosition = 0;
    }
    uint8_t fadeProgress = fadePosition / 125;
    uint8_t ledBrightness = ledFadeCurve(fadeProgress);
    OCR0B = 255 - ledBrightness;
    if (++ledFadePhase >= Config::ledFadeCycleSamples) {
        ledFadePhase = 0;
    }
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
        sample = (int32_t)windNoise * gustGain >> Config::Wind::gustMixShift;
        break;
    }
    case Config::ghostSound: { // Owl-style pitched calls give the ghost its wavering voice.
        sample = renderGhostCall(position);
        break;
    }
    case Config::doorSound: { // Play the recorded, flash-resident door squeak.
        sample = renderDoorSqueak();
        break;
    }
    case Config::bellSound: { // A smooth attack excites inharmonic partials with exponential ring-down.
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
        uint8_t attack = bellAttack(position);
        uint8_t fundamentalDecay = bellDecay(position, Config::Bell::fundamentalDecayShift);
        uint8_t partialDecay = bellDecay(position, Config::Bell::partialDecayShift);
        uint8_t highDecay = bellDecay(position, Config::Bell::highDecayShift);
        uint8_t fourthDecay = bellDecay(position, Config::Bell::fourthDecayShift);
        fundamentalDecay = (uint16_t)fundamentalDecay * attack >> 8;
        partialDecay = (uint16_t)partialDecay * attack >> 8;
        highDecay = (uint16_t)highDecay * attack >> 8;
        fourthDecay = (uint16_t)fourthDecay * attack >> 8;
        sample = (sineSignal(bellFundamentalPhase) * fundamentalDecay
                >> Config::Bell::fundamentalMixShift)
            + (sineSignal(bellPartialPhase) * partialDecay
                >> Config::Bell::partialMixShift)
            + (sineSignal(bellHighPartialPhase) * highDecay
                >> Config::Bell::highPartialMixShift)
            + (sineSignal(bellFourthPartialPhase) * fourthDecay
                >> Config::Bell::fourthPartialMixShift);
        if (position < Config::Bell::strikeNoiseSamples) {
            uint8_t noiseDecay = Config::Bell::strikeNoiseSamples - position;
            uint8_t noiseEnvelope = (uint16_t)attack * noiseDecay
                / Config::Bell::strikeNoiseSamples;
            sample += (pinkFast * noiseEnvelope) >> Config::Bell::strikeNoiseMixShift;
        }
        break;
    }
    case Config::owlSound: { // Flash wavetable and contours shape shorter paired owl calls.
        sample = renderOwlRecording();
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
    sample = ((int32_t)sample * fade) >> 8;

    if (++soundSample == activeSoundSamples) {
        soundSample = 0;
        if (sound + 1 == Config::soundCount) {
            sequencePlaying = 0;
            outputGain = 0;
        } else {
            ++sound;
            modulationPhase = 0;
            slowGustPhase = 0;
            doorSamplePhase = 0;
            bellFundamentalPhase = 0;
            bellPartialPhase = 0;
            bellHighPartialPhase = 0;
            bellFourthPartialPhase = 0;
            bellFundamentalDetune = 0;
            bellPartialDetune = 0;
            bellHighPartialDetune = 0;
            bellFourthPartialDetune = 0;
            owlRecordingPosition = 0;
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
