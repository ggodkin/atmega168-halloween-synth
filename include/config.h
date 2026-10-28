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
constexpr uint16_t gustPhaseStep = samplesFromReferenceRate(1);
constexpr int16_t noiseScale = 64;
constexpr uint8_t filteredNoiseDivisor = 2;
constexpr uint8_t deepNoiseDivisor = 2;
constexpr uint8_t rawNoiseDivisor = 16;
constexpr uint8_t gustDepthShift = 1;
constexpr uint16_t mixDivisor = 192;
}

namespace Ghost {
constexpr uint16_t vibratoPhaseStep = samplesFromReferenceRate(14);
constexpr uint16_t baseFrequency = 145;
constexpr uint8_t noiseScale = 12;
constexpr uint8_t wobbleShift = 1;
constexpr uint8_t toneNumerator = 3;
constexpr uint8_t toneDivisor = 4;
}

namespace Thunder {
constexpr uint16_t rumblePhaseStep = samplesFromReferenceRate(3);
constexpr uint16_t rumbleDecaySamples = samplesFromReferenceRate(30000);
constexpr uint16_t rumbleDecayDivisor = samplesFromReferenceRate(118);
constexpr uint16_t crackSamples = samplesFromReferenceRate(320);
constexpr uint16_t crackScale = 80;
constexpr uint8_t crackLevelDivisor = 2;
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
constexpr uint16_t thirdBoomStart = samplesFromReferenceRate(22000);
}

namespace Door {
constexpr uint16_t firstCreakStart = samplesFromReferenceRate(1800);
constexpr uint16_t firstCreakEnd = samplesFromReferenceRate(7800);
constexpr uint16_t secondCreakStart = samplesFromReferenceRate(12000);
constexpr uint16_t secondCreakEnd = samplesFromReferenceRate(20000);
constexpr uint16_t attackSamples = samplesFromReferenceRate(640);
constexpr uint8_t creakEnvelope = 20;
constexpr uint16_t baseFrequency = 620;
constexpr uint8_t pitchStepShift = 6;
constexpr uint8_t pitchJitterDivisor = 8;
constexpr uint8_t attackEnvelopeShift = 5;
constexpr int16_t waveformScale = 48;
constexpr uint8_t idleNoiseDivisor = 24;
}

namespace Bell {
constexpr uint16_t fundamentalFrequency = 587;
constexpr uint16_t partialFrequency = 987;
constexpr uint16_t highPartialFrequency = 1571;
constexpr uint16_t fundamentalDecaySamples = samplesFromReferenceRate(28000);
constexpr uint16_t fundamentalDecayDivisor = samplesFromReferenceRate(110);
constexpr uint16_t partialDecaySamples = samplesFromReferenceRate(18000);
constexpr uint16_t partialDecayDivisor = samplesFromReferenceRate(100);
constexpr uint16_t highDecaySamples = samplesFromReferenceRate(9000);
constexpr uint16_t highDecayDivisor = samplesFromReferenceRate(60);
constexpr uint16_t strikeSamples = samplesFromReferenceRate(120);
constexpr uint8_t partialDecayBase = 190;
constexpr uint8_t partialDecayFloor = 10;
constexpr uint8_t highDecayBase = 160;
constexpr uint8_t highDecayFloor = 10;
constexpr uint16_t fundamentalMixDivisor = 420;
constexpr uint16_t partialMixDivisor = 600;
constexpr uint16_t highPartialMixDivisor = 700;
}

namespace Owl {
constexpr uint16_t cycleSamples = samplesFromReferenceRate(9000);
constexpr uint16_t firstHootStart = samplesFromReferenceRate(300);
constexpr uint16_t firstHootEnd = samplesFromReferenceRate(2900);
constexpr uint16_t secondHootStart = samplesFromReferenceRate(3400);
constexpr uint16_t secondHootEnd = samplesFromReferenceRate(6000);
constexpr uint16_t pitchPeakSample = samplesFromReferenceRate(1300);
constexpr uint16_t baseFrequency = 360;
constexpr uint8_t pitchStepShift = 3;
constexpr uint16_t attackSamples = samplesFromReferenceRate(500);
constexpr uint16_t envelopeRampSamples = samplesFromReferenceRate(800);
constexpr uint8_t noiseScale = 18;
constexpr uint8_t envelopeShift = 2;
constexpr uint8_t envelopeLevel = 200;
constexpr uint16_t toneMixDivisor = 300;
}
}
