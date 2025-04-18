#ifndef RA_RANDOM_H_
#define RA_RANDOM_H_

#include "ra_math.h"
using namespace nasl;

typedef unsigned int uint32_t;

inline RA_FUNCTION unsigned int FNVHash(char* str, unsigned int length) {
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
inline RA_FUNCTION auto fnv_hash(uint32_t h, uint32_t d) -> uint32_t {
    h = (h * 16777619u) ^ ( d           & 0xFFu);
    h = (h * 16777619u) ^ ((d >>  8u) & 0xFFu);
    h = (h * 16777619u) ^ ((d >> 16u) & 0xFFu);
    h = (h * 16777619u) ^ ((d >> 24u) & 0xFFu);
    return h;
}

inline RA_FUNCTION unsigned int nrand(unsigned int* rng) {
    unsigned int orand = *rng;
    *rng = FNVHash((char*) &orand, 4);
    return *rng;
}

inline RA_FUNCTION auto xorshift(uint32_t* seed) -> uint32_t {
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
inline RA_FUNCTION auto randf(RNGState* rnd) -> float {
    // Assumes IEEE 754 floating point format
    auto x = randi(rnd);
    //return std::bit_cast<float>((127u << 23u) | (x & 0x7FFFFFu)) - 1.0f;
    unsigned i = (127u << 23u) | (x & 0x7FFFFFu);
    float f;
    f = *(float*) &i;
    return f - 1.0f;
    //return std::bit_cast<float>() - 1.0f;
}

// Sample [0, max] uniformly
inline RA_FUNCTION auto randi_max(RNGState* rnd, uint32_t max) -> uint32_t {
    const uint32_t rng_range = 0xFFFFFFFF;        
    if (rng_range == max) {
        return randi(rnd);
    } else {
        const uint32_t erange  = max + 1;
        const uint32_t scaling = rng_range / erange;
        const uint32_t past    = erange * scaling;
        
        uint32_t ret = randi(rnd);
        while (ret >= past) {
            ret = randi(rnd);
        }
    
        return ret / scaling;
    }
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