#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint8_t motionSensorPin = 2;
constexpr uint8_t pwmOutputPin = 6;
constexpr uint8_t pwmCenter = 128;
constexpr uint16_t audioSampleRate = 8000;
// Timer1 runs without a prescaler, so this produces one interrupt per audio sample.
constexpr uint16_t timer1Compare = F_CPU / audioSampleRate - 1;

constexpr uint8_t soundDurationSeconds = 4;
constexpr uint16_t samplesPerSound = audioSampleRate * soundDurationSeconds;
constexpr uint16_t soundFadeSamples = 160;
constexpr uint8_t outputGainStep = 4;
// This phase step is tuned for the configured 8 kHz sample rate.
constexpr uint8_t phaseIncrementShift = 3;
constexpr uint8_t filteredNoiseShift = 4;
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

namespace Wind {
constexpr uint16_t gustPhaseStep = 1;
constexpr int16_t noiseScale = 64;
constexpr uint8_t filteredNoiseDivisor = 2;
constexpr uint8_t deepNoiseDivisor = 2;
constexpr uint8_t rawNoiseDivisor = 16;
constexpr uint8_t gustDepthShift = 1;
constexpr uint16_t mixDivisor = 192;
}

namespace Ghost {
constexpr uint16_t vibratoPhaseStep = 14;
constexpr uint16_t baseFrequency = 145;
constexpr uint8_t noiseScale = 12;
constexpr uint8_t wobbleShift = 1;
constexpr uint8_t toneNumerator = 3;
constexpr uint8_t toneDivisor = 4;
}

namespace Thunder {
constexpr uint16_t rumblePhaseStep = 3;
constexpr uint16_t rumbleDecaySamples = 30000;
constexpr uint16_t rumbleDecayDivisor = 118;
constexpr uint16_t crackSamples = 1200;
constexpr uint16_t crackScale = 150;
constexpr uint8_t rumbleToneDivisor = 2;
constexpr uint8_t rumbleOutputDivisor = 128;
constexpr uint8_t crackLevelDivisor = 8;
}

namespace Door {
constexpr uint16_t firstCreakStart = 1800;
constexpr uint16_t firstCreakEnd = 7800;
constexpr uint16_t secondCreakStart = 12000;
constexpr uint16_t secondCreakEnd = 20000;
constexpr uint16_t attackSamples = 640;
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
constexpr uint16_t fundamentalDecaySamples = 28000;
constexpr uint16_t fundamentalDecayDivisor = 110;
constexpr uint16_t partialDecaySamples = 18000;
constexpr uint16_t partialDecayDivisor = 100;
constexpr uint16_t highDecaySamples = 9000;
constexpr uint16_t highDecayDivisor = 60;
constexpr uint16_t strikeSamples = 120;
constexpr uint8_t partialDecayBase = 190;
constexpr uint8_t partialDecayFloor = 10;
constexpr uint8_t highDecayBase = 160;
constexpr uint8_t highDecayFloor = 10;
constexpr uint16_t fundamentalMixDivisor = 420;
constexpr uint16_t partialMixDivisor = 600;
constexpr uint16_t highPartialMixDivisor = 700;
}

namespace Owl {
constexpr uint16_t cycleSamples = 9000;
constexpr uint16_t firstHootStart = 300;
constexpr uint16_t firstHootEnd = 2900;
constexpr uint16_t secondHootStart = 3400;
constexpr uint16_t secondHootEnd = 6000;
constexpr uint16_t pitchPeakSample = 1300;
constexpr uint16_t baseFrequency = 360;
constexpr uint8_t pitchStepShift = 3;
constexpr uint16_t attackSamples = 500;
constexpr uint16_t envelopeRampSamples = 800;
constexpr uint8_t noiseScale = 18;
constexpr uint8_t envelopeShift = 2;
constexpr uint8_t envelopeLevel = 200;
constexpr uint16_t toneMixDivisor = 300;
}
}
