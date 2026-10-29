#include <avr/interrupt.h>
#include <avr/io.h>
#include "audio_engine.h"
#include "config.h"

ISR(TIMER1_COMPA_vect) {
    OCR0A = AudioEngine::nextSample();
}

void setupPWM() {
    DDRD |= _BV(Config::pwmOutputBit) | _BV(Config::ledOutputBit);
    OCR0A = Config::pwmCenter;
    OCR0B = 255;
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM00) | (1 << WGM01);
    TCCR0B = (1 << CS00);
}

void setupMotionSensor() {
    DDRD &= ~_BV(Config::motionSensorBit);
    PORTD &= ~_BV(Config::motionSensorBit);
}

void setupAudioTimer() {
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1B = _BV(WGM12) | _BV(CS10);
    OCR1A = Config::timer1Compare;
    TIFR1 = _BV(OCF1A);
    TIMSK1 = _BV(OCIE1A);
}

int main() {
    AudioEngine::begin();
    setupMotionSensor();
    setupPWM();
    setupAudioTimer();
    sei();

    // Treat an already-active sensor at startup as the first rising-edge trigger.
    uint8_t previousSensorState = 0;
    for (;;) {
        uint8_t sensorState = PIND & _BV(Config::motionSensorBit);
        // Only a new low-to-high edge starts a request; a held-high sensor does not retrigger.
        if (sensorState && !previousSensorState) {
            AudioEngine::trigger();
        }
        previousSensorState = sensorState;
    }
}
