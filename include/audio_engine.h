#pragma once
#include <Arduino.h>
namespace AudioEngine { void begin(); void setEnabled(bool enabled); uint8_t nextSample(); }
