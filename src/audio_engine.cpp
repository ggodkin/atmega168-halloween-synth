#include <Arduino.h>
#include "audio_engine.h"
#include "noise.h"

namespace {
const uint16_t samplesPerSound = 32000;
int16_t filteredNoise = 0;
int16_t deepNoise = 0;
uint16_t soundSample = 0;
uint8_t sound = 0;
uint16_t modulationPhase = 0;
uint16_t ghostPhase = 0;
uint16_t squeakPhase = 0;
uint16_t bellFundamentalPhase = 0;
uint16_t bellPartialPhase = 0;
uint16_t bellHighPartialPhase = 0;
uint16_t owlPhase = 0;
// An 8-bit request is atomic to read between the main loop and audio ISR.
volatile uint8_t requestedEnabled = 0;
uint8_t playingEnabled = 0;
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
    sound = 0;
    modulationPhase = 0;
    ghostPhase = 0;
    squeakPhase = 0;
    bellFundamentalPhase = 0;
    bellPartialPhase = 0;
    bellHighPartialPhase = 0;
    owlPhase = 0;
}
}

void AudioEngine::begin() {
    requestedEnabled = 0;
    playingEnabled = 0;
    outputGain = 0;
    resetSequence();
}

void AudioEngine::setEnabled(bool enabled) {
    requestedEnabled = enabled ? 1 : 0;
}

uint8_t AudioEngine::nextSample() {
    uint8_t enableRequest = requestedEnabled;
    if (enableRequest != playingEnabled) {
        playingEnabled = enableRequest;
        if (playingEnabled) {
            resetSequence();
        }
    }

    if (!playingEnabled && outputGain == 0) {
        return 128;
    }
    // Fade on state changes to avoid clicks when the motion sensor toggles.
    if (playingEnabled) {
        outputGain = outputGain > 251 ? 255 : outputGain + 4;
    } else {
        outputGain = outputGain < 4 ? 0 : outputGain - 4;
    }

    int16_t noise = (int16_t)noise8() - 128;
    filteredNoise += (noise - filteredNoise) >> 4;
    deepNoise += (filteredNoise - deepNoise) >> 6;

    uint16_t position = soundSample;
    int16_t sample = 0;

    switch (sound) {
    case 0: { // Slow gust modulation and layered low-passed noise make wind, not hiss.
        modulationPhase += 1;
        uint8_t gust = triangleWave(modulationPhase);
        int16_t windNoise = (filteredNoise - deepNoise) / 2
            + deepNoise / 2 + noise / 16;
        sample = windNoise * (64 + (gust >> 1)) / 192;
        break;
    }
    case 1: { // Pitch-vibrato on a strong near-sine tone gives the ghost a clear howl.
        modulationPhase += 14;
        uint8_t wobble = triangleWave(modulationPhase);
        uint16_t frequency = 145 + (wobble >> 1);
        ghostPhase += frequency << 3;
        sample = sineSignal(ghostPhase) * 3 / 4 + noise / 12;
        break;
    }
    case 2: { // A short crack leads into a separately decaying low rumble.
        modulationPhase += 3;
        uint8_t rumbleDecay = position < 30000
            ? 255 - position / 118
            : 0;
        int16_t rumble = deepNoise + triangleSignal(modulationPhase) / 2;
        int16_t crack = position < 1200
            ? noise * ((1200 - position) >> 3) / 150
            : 0;
        sample = crack + (rumble / 2) * rumbleDecay / 128;
        break;
    }
    case 3: { // Narrow pitch movement plus friction noise makes a hinge creak, not a siren.
        uint16_t squeakPosition = 0;
        uint16_t squeakDuration = 0;
        if (position >= 1800 && position < 7800) {
            squeakPosition = position - 1800;
            squeakDuration = 6000;
        } else if (position >= 12000 && position < 20000) {
            squeakPosition = position - 12000;
            squeakDuration = 8000;
        }

        if (squeakDuration) {
            uint16_t edge = squeakPosition;
            uint16_t release = squeakDuration - squeakPosition;
            if (release < edge) edge = release;
            uint8_t envelope = edge < 640 ? edge >> 5 : 20;
            int16_t pitchJitter = filteredNoise / 8;
            uint16_t frequency = 620 + (squeakPosition >> 6) + pitchJitter;
            squeakPhase += frequency << 3;
            int16_t friction = filteredNoise - deepNoise;
            sample = triangleSignal(squeakPhase) * envelope / 48
                + friction * envelope / 48;
        } else {
            sample = noise / 24;
        }
        break;
    }
    case 4: { // Inharmonic partials ring down at different rates after a noisy strike.
        bellFundamentalPhase += 587 << 3;
        bellPartialPhase += 987 << 3;
        bellHighPartialPhase += 1571 << 3;
        uint8_t fundamentalDecay = position < 28000
            ? 255 - position / 110
            : 0;
        uint8_t partialDecay = position < 18000
            ? 190 - position / 100
            : 10;
        uint8_t highDecay = position < 9000
            ? 160 - position / 60
            : 10;
        sample = sineSignal(bellFundamentalPhase) * fundamentalDecay / 420
            + sineSignal(bellPartialPhase) * partialDecay / 600
            + sineSignal(bellHighPartialPhase) * highDecay / 700;
        if (position < 120) {
            sample += noise * (120 - position) / 120;
        }
        break;
    }
    default: { // Paired owl hoots use a breathy attack and a rounded falling contour.
        uint16_t cycle = position % 9000;
        uint16_t hootPosition = 0;
        bool hooting = false;
        if (cycle >= 300 && cycle < 2900) {
            hootPosition = cycle - 300;
            hooting = true;
        } else if (cycle >= 3400 && cycle < 6000) {
            hootPosition = cycle - 3400;
            hooting = true;
        }

        if (hooting) {
            uint16_t contour = hootPosition < 1300
                ? hootPosition
                : 2600 - hootPosition;
            uint16_t frequency = 360 + (contour >> 3);
            owlPhase += frequency << 3;
            uint16_t edge = hootPosition < 500
                ? hootPosition
                : 2600 - hootPosition;
            uint8_t envelope = edge < 800 ? edge >> 2 : 200;
            sample = sineSignal(owlPhase) * envelope / 300 + noise / 18;
        }
        break;
    }
    }

    uint8_t fade = 255;
    if (position < 160) {
        fade = position;
    } else if (position >= samplesPerSound - 160) {
        fade = samplesPerSound - 1 - position;
    }
    sample = (sample * fade) >> 8;

    if (playingEnabled && ++soundSample == samplesPerSound) {
        soundSample = 0;
        sound = (sound + 1) % 6;
        modulationPhase = 0;
        ghostPhase = 0;
        squeakPhase = 0;
        bellFundamentalPhase = 0;
        bellPartialPhase = 0;
        bellHighPartialPhase = 0;
        owlPhase = 0;
    }

    if (sample < -128) sample = -128;
    if (sample > 127) sample = 127;
    sample = sample * outputGain / 255;
    sample += 128;
    if (sample < 0) sample = 0;
    if (sample > 255) sample = 255;
    return (uint8_t)sample;
}
