#ifndef RA_RANDOM_H_
#define RA_RANDOM_H_

#include "ra_math.h"
using namespace nasl;

typedef unsigned int uint32_t;

RA_FUNCTION unsigned int FNVHash(char* str, unsigned int length) {
    const unsigned int fnv_prime = 0x811C9DC5;
    unsigned int hash = 0;
    unsigned int i = 0;

    for (i = 0; i < length; str++, i++)
    {
        hash *= fnv_prime;
        hash ^= (*str);
    }

    return hash;
}

// FNV hash function
RA_FUNCTION auto fnv_hash(uint32_t h, uint32_t d) -> uint32_t {
    h = (h * 16777619u) ^ ( d           & 0xFFu);
    h = (h * 16777619u) ^ ((d >>  8u) & 0xFFu);
    h = (h * 16777619u) ^ ((d >> 16u) & 0xFFu);
    h = (h * 16777619u) ^ ((d >> 24u) & 0xFFu);
    return h;
}

RA_FUNCTION unsigned int nrand(unsigned int* rng) {
    unsigned int orand = *rng;
    *rng = FNVHash((char*) &orand, 4);
    return *rng;
}

RA_FUNCTION auto xorshift(uint32_t* seed) -> uint32_t {
    auto x = *seed;
    // x = select(x == 0u, 1u, x);
    x = (x == 0) ? 1u : x;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    *seed = x;
    return x;
}

//auto randi = xorshift;
#define randi xorshift

typedef uint32_t RNGState;
// [0.0, 1.0]
RA_FUNCTION auto randf(RNGState* rnd) -> float {
    // Assumes IEEE 754 floating point format
    auto x = randi(rnd);
    //return std::bit_cast<float>((127u << 23u) | (x & 0x7FFFFFu)) - 1.0f;
    unsigned i = (127u << 23u) | (x & 0x7FFFFFu);
    float f;
    f = *(float*) &i;
    return f - 1.0f;
    //return std::bit_cast<float>() - 1.0f;
}

/*RA_FUNCTION auto randf(RNGState* rnd) -> float {
    // Assumes IEEE 754 floating point format
    auto x = randi(rnd);
    //return std::bit_cast<float>((127u << 23u) | (x & 0x7FFFFFu)) - 1.0f;
    unsigned i = (127u << 23u) | (x & 0x7FFFFFu);
    float f;
    f = *(float*) &i;
    return f / 2.0f - 0.5f;
}*/

#endif