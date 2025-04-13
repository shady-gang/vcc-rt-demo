#ifndef RA_FRESNEL_H_
#define RA_FRESNEL_H_

#include "ra_math.h"

namespace fresnel {
inline RA_FUNCTION float snell(float eta, float cos_i) {
    return 1 - (1 - cos_i * cos_i) * eta * eta;
}

struct FresnelTerm {
    float cos_t;
    float factor;
};
inline RA_FUNCTION FresnelTerm fresnel(float eta, float cos_i) {
    float cos2_t = snell(eta, cos_i);

    if (cos2_t <= 0) {
        // Total reflection
        return FresnelTerm {
            .cos_t  = -1,
            .factor = 1
        };
    } else {
        float cos_t   = sqrtf(cos2_t);
        float R_s     = (eta * cos_i - cos_t) / (eta * cos_i + cos_t);
        float R_p     = (cos_i - eta * cos_t) / (cos_i + eta * cos_t);
        float factor  = clampf((R_s * R_s + R_p * R_p) / 2, 0, 1);
        return FresnelTerm {
            .cos_t  = cos_t,
            .factor = factor
        };
    }
}

inline RA_FUNCTION float conductor_factor(float n, float k, float cos_i) {
    float f	  = n*n + k*k;
    float d1  = f * cos_i * cos_i;
	float d2  = 2.0f * n * cos_i;
	float R_s = (d1 - d2) / (d1 + d2);
	float R_p = (f - d2 + cos_i * cos_i) / (f + d2 + cos_i * cos_i);
    return clampf((R_s * R_s + R_p * R_p) / 2, 0, 1);
}

inline RA_FUNCTION float schlick(float eta) {
    float s = clampf(1 - eta, 0, 1);
    return (s * s) * (s * s) * s;
}
}
#endif