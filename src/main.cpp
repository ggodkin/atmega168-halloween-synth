#include <Arduino.h>
#include "audio_engine.h"

const uint8_t motionSensorPin = 2;

ISR(TIMER1_COMPA_vect) {
    OCR0A = AudioEngine::nextSample();
}

void setupPWM() {
    pinMode(6, OUTPUT);
    OCR0A = 128;
    TCCR0A = (1 << COM0A1) | (1 << WGM00) | (1 << WGM01);
    TCCR0B = (1 << CS00);
}

void setupAudioTimer() {
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS10);
    OCR1A = 999;
    TIMSK1 |= (1 << OCIE1A);
    sei();
}

void setup() {
    pinMode(motionSensorPin, INPUT);
    AudioEngine::begin();
    setupPWM();
    setupAudioTimer();
}

void loop() {
    AudioEngine::setEnabled(digitalRead(motionSensorPin) == HIGH);
}
