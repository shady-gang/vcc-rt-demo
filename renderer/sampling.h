#ifndef RA_SAMPLING_H_
#define RA_SAMPLING_H_

#include "random.h"

namespace shading {

struct DirSample {
    vec3 dir;
    float pdf;
};

// Probability density function for uniform sphere sampling
inline RA_FUNCTION auto uniform_sphere_pdf() -> float { return 1.0f / (4.0f * M_PI); }

inline RA_FUNCTION auto make_dir_sample(float c, float s, float phi, float pdf) -> DirSample {
    auto x = s * cosf(phi);
    auto y = s * sinf(phi);
    auto z = c;
    return DirSample {
        .dir = vec3(x, y, z),
        .pdf = pdf
    };
}

// Samples a direction uniformly on a sphere
inline RA_FUNCTION auto sample_uniform_sphere(float u, float v) -> DirSample {
    auto c = 2.0f * v - 1.0f;
    auto s = sqrtf(1.0f - c * c);
    auto phi = 2.0f * M_PI * u;
    return make_dir_sample(c, s, phi, uniform_sphere_pdf());
}

// Probability density function for cosine weighted hemisphere sampling
inline RA_FUNCTION auto cosine_hemisphere_pdf(float c) -> float { return c * (1.0f / M_PI); }

inline RA_FUNCTION auto sample_cosine_hemisphere(float u, float v) -> DirSample {
    auto c = sqrtf(1.0f - v);
    auto s = sqrtf(v);
    auto phi = 2.0f * M_PI * u;
    return make_dir_sample(c, s, phi, cosine_hemisphere_pdf(c));
}
}

#endif