#ifndef RA_MICROFACET_H_
#define RA_MICROFACET_H_

#include "random.h"
#include "sampling.h"
#include "shading.h"

namespace microfacet {
// Everything below assumes and returns in shading space!

inline RA_FUNCTION auto g_1_smith(vec3 w, float alpha_u, float alpha_v) -> float {
    float cosX = w.x; // cosPhi * sinTheta
    float cosY = w.y; // sinPhi * sinTheta
    float cosZ = w.z; // cosTheta

    float kx = alpha_u * cosX;
    float ky = alpha_v * cosY;
    float a2 = kx * kx + ky * ky;
    if (cosZ * cosZ <= __FLT_EPSILON__) {
        return 0;
    } else {
        float k2 = a2 / (cosZ * cosZ);
        float denom = 1 + sqrtf(1 + k2);
        return 2 / denom;
    }
}

inline RA_FUNCTION auto ndf_ggx(vec3 m, float alpha_u, float alpha_v) -> float {
    float cosX = m.x; // cosPhi * sinTheta
    float cosY = m.y; // sinPhi * sinTheta
    float cosZ = m.z; // cosTheta

    float kx = cosX / alpha_u;
    float ky = cosY / alpha_v;
    float k  = kx * kx + ky * ky + cosZ * cosZ;
    
    float denom = float(M_PI) * alpha_u * alpha_v * k * k;
    if (denom <= __FLT_EPSILON__)
        return 0;
    else
        return 1 / denom;
}

// Based on:
// Sampling Visible GGX Normals with Spherical Caps
// by Jonathan Dupuy and Anis Benyoub 
// https://arxiv.org/pdf/2306.05044
inline RA_FUNCTION auto sample_vndf_ggx(RNGState* rng, vec3 lN, float alpha_u, float alpha_v) -> vec3 {
    // Stretch
    vec3 sL = normalize(vec3(alpha_u * lN.x, alpha_v * lN.y, lN.z));
    
    // Compute
    float u0 = randf(rng);
    float u1 = randf(rng);

    float phi = 2 * float(M_PI) * u0;
    float z = (1 - u1) * (1 + sL.z) - sL.z;
    float sinTheta = sqrtf(clampf(1 - z * z, 0, 1));
    float x = sinTheta * cosf(phi);
    float y = sinTheta * sinf(phi);

    vec3 h = vec3(x,y,z) + lN;

    // Unstretch    
    return normalize(vec3(h.x * alpha_u, h.y * alpha_v, h.z));
}

inline RA_FUNCTION auto pdf_vndf_ggx(vec3 w, vec3 h, float alpha_u, float alpha_v) -> float {
    float cosZ = fabs(w.z);
    return cosZ <= __FLT_EPSILON__ ? 0 : (g_1_smith(w, alpha_u, alpha_v) * fabs(w.dot(h)) * ndf_ggx(h, alpha_u, alpha_v) / cosZ);
}

inline RA_FUNCTION auto eval_D(vec3 m, float alpha_u, float alpha_v) -> float {
    return ndf_ggx(m, alpha_u, alpha_v);
}

inline RA_FUNCTION auto eval_G(vec3 wi, vec3 wo, vec3 m, float alpha_u, float alpha_v) -> float {
    return g_1_smith(wi, alpha_u, alpha_v) * g_1_smith(wo, alpha_u, alpha_v);
}

inline RA_FUNCTION auto reflective_jacobian(float cos_i) -> float {
    return 1 / (4 * cos_i);
}

inline RA_FUNCTION auto refractive_jacobian(float eta, float cos_i, float cos_o) -> float {
    float denom = cos_i + cos_o * eta;
    if (denom * denom <= __FLT_EPSILON__) 
        return 0;
    else
        return eta * eta * cos_i / (denom * denom);
}
}

#endif