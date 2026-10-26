#include <Arduino.h>
#include "audio_engine.h"
#include "noise.h"
namespace { int16_t wind=128; uint16_t phase=0; uint16_t lfo=0; }
void AudioEngine::begin(){}
uint8_t AudioEngine::nextSample(){ int16_t s=128; wind += (((int16_t)noise8()-wind)>>4); s += (wind-128)/2; lfo+=2; phase += 50 + ((lfo>>8)>>2); s += (phase & 0x8000)?30:-30; if(s<0)s=0; if(s>255)s=255; return (uint8_t)s; }
