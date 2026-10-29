#pragma once

#include <avr/io.h>
#include <stdint.h>

namespace Config {
constexpr uint8_t motionSensorBit = PD2;
constexpr uint8_t pwmOutputBit = PD6;
constexpr uint8_t pwmCenter = 128;
constexpr uint16_t audioSampleRate = 8000;
// Timer1 runs without a prescaler, so this produces one interrupt per audio sample.
constexpr uint16_t timer1Compare = F_CPU / audioSampleRate - 1;
constexpr uint16_t referenceSampleRate = 8000;

constexpr uint8_t soundDurationSeconds = 4;
constexpr uint16_t samplesPerSound = audioSampleRate * soundDurationSeconds;
constexpr uint16_t samplesFromReferenceRate(uint16_t samples) {
    return ((uint32_t)samples * audioSampleRate + referenceSampleRate / 2)
        / referenceSampleRate;
}
constexpr uint16_t soundFadeSamples = samplesFromReferenceRate(160);
constexpr uint8_t outputGainStep = 4;
// Eight phase units per hertz approximates 65536 / the 8 kHz sample rate.
constexpr uint8_t phaseIncrementScale = 8;
constexpr uint8_t deepNoiseShift = 6;

enum Sound : uint8_t {
    windSound,
    ghostSound,
    doorSound,
    bellSound,
    owlSound
};
constexpr uint8_t soundCount = owlSound + 1;

namespace PinkNoise {
constexpr uint8_t fastFilterShift = 2;
constexpr uint8_t midFilterShift = 4;
constexpr uint8_t slowFilterShift = 6;
constexpr uint8_t whiteWeightShift = 1;
constexpr uint8_t fastWeightShift = 1;
constexpr uint8_t midWeightShift = 2;
constexpr uint8_t slowWeightShift = 3;
}

namespace Wind {
constexpr uint16_t gustPhaseStep = samplesFromReferenceRate(4);
constexpr uint16_t slowGustPhaseStep = samplesFromReferenceRate(1);
constexpr uint8_t airFilterShift = 7;
constexpr uint8_t hissFilterShift = 2;
constexpr int16_t baseGain = 48;
constexpr uint8_t filteredNoiseDivisor = 2;
constexpr uint8_t deepNoiseDivisor = 2;
constexpr uint8_t rawNoiseDivisor = 16;
constexpr uint8_t airMovementDivisor = 4;
constexpr uint8_t hissDivisor = 8;
constexpr uint8_t fastGustDepthShift = 1;
constexpr uint8_t slowGustDepthShift = 2;
constexpr uint8_t gustMixShift = 7;
}

namespace Door {
constexpr uint16_t sampleRate = 4000;
constexpr uint32_t phaseIncrement =
    ((uint32_t)sampleRate << 16) / audioSampleRate;
}

namespace Bell {
constexpr uint16_t fundamentalFrequency = 587;
constexpr uint16_t partialFrequency = 987;
constexpr uint16_t highPartialFrequency = 1571;
constexpr uint16_t fourthPartialFrequency = 2137;
constexpr uint8_t fundamentalDecayShift = 11;
constexpr uint8_t partialDecayShift = 10;
constexpr uint8_t highDecayShift = 9;
constexpr uint8_t fourthDecayShift = 8;
constexpr uint16_t strikeNoiseSamples = samplesFromReferenceRate(256);
constexpr uint8_t strikeNoiseMixShift = 10;
constexpr uint8_t attackTableSize = 32;
constexpr uint8_t attackTableStepShift = 5;
constexpr uint8_t randomDetuneRange = 4;
constexpr uint16_t fadeSamples = samplesFromReferenceRate(32);
constexpr uint8_t fundamentalMixShift = 9;
constexpr uint8_t partialMixShift = 9;
constexpr uint8_t highPartialMixShift = 10;
constexpr uint8_t fourthPartialMixShift = 10;
}

namespace Owl {
constexpr uint16_t cycleSamples = samplesFromReferenceRate(24000);
constexpr uint16_t firstHootStart = samplesFromReferenceRate(250);
constexpr uint16_t firstHootEnd = samplesFromReferenceRate(5250);
constexpr uint16_t secondHootStart = samplesFromReferenceRate(11250);
constexpr uint16_t secondHootEnd = samplesFromReferenceRate(16250);
constexpr uint8_t contourEntryCount = 40;
constexpr uint8_t contourStepShift = 7;
constexpr uint16_t baseFrequency = 520;
constexpr uint8_t firstHootLevelPercent = 65;
constexpr uint8_t secondHootOffset = 160;
constexpr uint8_t secondHootLevelPercent = 100;
constexpr uint8_t chorusDetuneHz = 3;
constexpr uint8_t chorusMainWeight = 3;
constexpr uint16_t vibratoPhaseStep =
    samplesFromReferenceRate(6) * phaseIncrementScale;
constexpr uint8_t vibratoDivisor = 40;
constexpr uint8_t tremoloDepthDivisor = 8;
constexpr uint16_t breathSamples = samplesFromReferenceRate(512);
constexpr uint8_t breathEnvelopeShift = 9;
constexpr uint8_t levelScaleShift = 7;
constexpr uint8_t breathMixShift = 12;
}
}
