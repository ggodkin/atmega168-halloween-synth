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
    thunderSound,
    doorSound,
    bellSound,
    owlSound
};
constexpr uint8_t soundCount = owlSound + 1;
constexpr uint16_t thunderDurationSamples = samplesFromReferenceRate(24000);
constexpr uint16_t samplesForSound(uint8_t sound) {
    return sound == thunderSound ? thunderDurationSamples : samplesPerSound;
}

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
constexpr int16_t baseGain = 32;
constexpr uint8_t filteredNoiseDivisor = 2;
constexpr uint8_t deepNoiseDivisor = 2;
constexpr uint8_t rawNoiseDivisor = 16;
constexpr uint8_t airMovementDivisor = 4;
constexpr uint8_t hissDivisor = 8;
constexpr uint8_t fastGustDepthShift = 1;
constexpr uint8_t slowGustDepthShift = 2;
constexpr uint8_t windNoiseInputDivisor = 3;
constexpr uint16_t mixDivisor = 128;
}

namespace Ghost {
constexpr uint16_t vibratoPhaseStep = samplesFromReferenceRate(14);
constexpr uint16_t baseFrequency = 145;
constexpr uint8_t chorusDetuneHz = 2;
constexpr uint8_t noiseScale = 12;
constexpr uint8_t wobbleShift = 1;
constexpr uint8_t chorusMainWeight = 3;
constexpr uint8_t tremoloBase = 192;
constexpr uint8_t tremoloDepthShift = 2;
constexpr uint8_t decayShift = 7;
constexpr uint8_t toneNumerator = 3;
constexpr uint8_t toneDivisor = 4;
}

namespace Thunder {
constexpr uint16_t rumblePhaseStep = samplesFromReferenceRate(3);
constexpr uint16_t rumbleDecaySamples = thunderDurationSamples;
constexpr uint16_t rumbleDecayDivisor = samplesFromReferenceRate(94);
constexpr uint16_t crackSamples = samplesFromReferenceRate(300);
constexpr uint16_t crackScale = 50;
constexpr uint8_t crackLevelShift = 2;
constexpr uint8_t crackClickLevel = 110;
constexpr uint8_t rumbleToneDivisor = 4;
constexpr uint8_t rumbleGainDivisor = 32;
constexpr int16_t rumbleOutputDivisor = 256;
constexpr uint8_t rumbleBaseGain = 48;
constexpr uint8_t rumbleModulationShift = 1;
constexpr uint8_t boomPeakGain = 64;
constexpr uint16_t boomAttackSamples = samplesFromReferenceRate(1024);
constexpr uint16_t boomDecaySamples = samplesFromReferenceRate(2048);
constexpr uint8_t boomAttackShift = 4;
constexpr uint8_t boomDecayShift = 5;
constexpr uint16_t firstBoomStart = samplesFromReferenceRate(2000);
constexpr uint16_t secondBoomStart = samplesFromReferenceRate(12000);
constexpr uint16_t thirdBoomStart = samplesFromReferenceRate(18000);
}

namespace Door {
constexpr uint16_t firstCreakStart = samplesFromReferenceRate(2200);
constexpr uint16_t firstCreakEnd = samplesFromReferenceRate(8700);
constexpr uint16_t secondCreakStart = samplesFromReferenceRate(6800);
constexpr uint16_t secondCreakEnd = samplesFromReferenceRate(14000);
constexpr uint16_t thirdCreakStart = samplesFromReferenceRate(13800);
constexpr uint16_t thirdCreakEnd = samplesFromReferenceRate(19800);
constexpr uint16_t attackSamples = samplesFromReferenceRate(640);
constexpr uint8_t creakEnvelope = 20;
constexpr uint16_t firstBaseFrequency = 570;
constexpr uint16_t secondBaseFrequency = 650;
constexpr uint16_t thirdBaseFrequency = 720;
constexpr uint8_t pitchStepShift = 6;
constexpr uint8_t pitchJitterDivisor = 4;
constexpr uint8_t rawPitchJitterDivisor = 10;
constexpr uint8_t attackEnvelopeShift = 5;
constexpr int16_t waveformScale = 48;
constexpr uint8_t idleNoiseDivisor = 24;
constexpr uint16_t firstCatchStart = samplesFromReferenceRate(2400);
constexpr uint16_t firstCatchEnd = samplesFromReferenceRate(2560);
constexpr uint16_t secondCatchStart = samplesFromReferenceRate(1800);
constexpr uint16_t secondCatchEnd = samplesFromReferenceRate(1940);
constexpr uint16_t thirdCatchStart = samplesFromReferenceRate(3100);
constexpr uint16_t thirdCatchEnd = samplesFromReferenceRate(3260);
constexpr uint16_t resonanceFrequency = 2350;
constexpr uint16_t resonanceDecaySamples = samplesFromReferenceRate(900);
constexpr uint8_t resonanceOutputDivisor = 200;
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
constexpr uint8_t strikeNoiseSamples = samplesFromReferenceRate(96);
constexpr uint8_t strikeToneSamples = samplesFromReferenceRate(24);
constexpr uint16_t strikeToneFrequency = 3200;
constexpr uint8_t randomDetuneRange = 4;
constexpr uint16_t fadeSamples = samplesFromReferenceRate(32);
constexpr uint8_t fundamentalMixShift = 9;
constexpr uint8_t partialMixShift = 9;
constexpr uint8_t highPartialMixShift = 10;
constexpr uint8_t fourthPartialMixShift = 10;
}

namespace Owl {
constexpr uint16_t cycleSamples = samplesFromReferenceRate(9500);
constexpr uint16_t firstHootStart = samplesFromReferenceRate(250);
constexpr uint16_t firstHootEnd = samplesFromReferenceRate(4250);
constexpr uint16_t secondHootStart = samplesFromReferenceRate(4700);
constexpr uint16_t secondHootEnd = samplesFromReferenceRate(8700);
constexpr uint8_t contourEntryCount = 32;
constexpr uint8_t contourStepShift = 7;
constexpr uint16_t baseFrequency = 520;
constexpr uint8_t secondHootOffset = 35;
constexpr uint8_t chorusDetuneHz = 3;
constexpr uint8_t chorusMainWeight = 3;
constexpr uint16_t vibratoPhaseStep =
    samplesFromReferenceRate(6) * phaseIncrementScale;
constexpr uint8_t vibratoDivisor = 40;
constexpr uint8_t tremoloDepthDivisor = 8;
constexpr uint16_t breathSamples = samplesFromReferenceRate(512);
constexpr uint8_t levelScaleShift = 7;
constexpr uint8_t breathMixShift = 11;
}
}
