#include <stdint.h>

namespace {
uint32_t xorshiftState = 0x6D2B79F5u;
uint16_t lfsrState = 0xACE1u;
}

uint8_t noise8() {
    // Independent xorshift32 and maximal-period 16-bit Galois LFSR decorrelate the noise.
    xorshiftState ^= xorshiftState << 13;
    xorshiftState ^= xorshiftState >> 17;
    xorshiftState ^= xorshiftState << 5;

    uint8_t feedback = lfsrState & 1;
    lfsrState >>= 1;
    if (feedback) {
        lfsrState ^= 0xB400u;
    }

    uint32_t mixed = xorshiftState ^ (xorshiftState >> 8)
        ^ (xorshiftState >> 16) ^ (xorshiftState >> 24);
    return (uint8_t)(mixed ^ lfsrState ^ (lfsrState >> 8));
}
