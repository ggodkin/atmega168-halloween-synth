#include <stdint.h>
static uint32_t lfsr=0xACE1u;
uint8_t noise8(){ lfsr^=lfsr<<13; lfsr^=lfsr>>17; lfsr^=lfsr<<5; return lfsr&0xFF; }
