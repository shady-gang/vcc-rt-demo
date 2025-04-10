#ifndef RA_MATH
#define RA_MATH

// static float min(float a, float b) { if (a < b) return a; return b; }
// static float max(float a, float b) { if (a > b) return a; return b; }

#ifdef __SHADY__
#include "shady.h"
static float M_PI = 3.14159265f;
static float fabs(float) __asm__("shady::pure_op::GLSL.std.450::4::Invocation");
static float sign(float) __asm__("shady::pure_op::GLSL.std.450::6::Invocation");
static float floorf(float) __asm__("shady::pure_op::GLSL.std.450::8::Invocation");
static float sinf(float) __asm__("shady::pure_op::GLSL.std.450::13::Invocation");
static float cosf(float) __asm__("shady::pure_op::GLSL.std.450::14::Invocation");
static float tanf(float) __asm__("shady::pure_op::GLSL.std.450::15::Invocation");
static float powf(float, float) __asm__("shady::pure_op::GLSL.std.450::26::Invocation");
static float expf(float) __asm__("shady::pure_op::GLSL.std.450::27::Invocation");
static float logf(float) __asm__("shady::pure_op::GLSL.std.450::28::Invocation");
static float exp2f(float) __asm__("shady::pure_op::GLSL.std.450::29::Invocation");
static float log2f(float) __asm__("shady::pure_op::GLSL.std.450::30::Invocation");
static float sqrtf(float) __asm__("shady::pure_op::GLSL.std.450::31::Invocation");
static float fminf(float, float) __asm__("shady::pure_op::GLSL.std.450::37::Invocation");
static float fmaxf(float, float) __asm__("shady::pure_op::GLSL.std.450::40::Invocation");
static float fmaf(float, float, float) __asm__("shady::pure_op::GLSL.std.450::50::Invocation");

typedef long int size_t;

#define RA_FUNCTION static
#define RA_METHOD
#define RA_CONSTANT static
RA_FUNCTION float copysignf(float a, float b) {
    return (b < 0.0f) ? -fabs(a) : fabs(a);
}

#elif __CUDACC__
//static float M_PI = 3.14159265f;

#define RA_FUNCTION __device__
#define RA_METHOD __device__
#define RA_CONSTANT __constant__

RA_FUNCTION float sign(float f) {
    return copysignf(1.0f, f);
}
#else
#include <cmath>
#include <cstddef>

#define RA_FUNCTION
#define RA_METHOD
#define RA_CONSTANT static inline

RA_FUNCTION static float sign(float f) {
    return copysignf(1.0f, f);
}
#endif

#include "nasl.h"

using namespace nasl;

RA_CONSTANT float epsilon = 1e-4f;
// RA_CONSTANT float pi = 3.141592f;

/// @brief Barycentric interpolation ([0,0] returns a, [1,0] returns b, and
/// [0,1] returns c).
template <typename T>
RA_FUNCTION T interpolateBarycentric(const vec2 &bary, const T &a, const T &b,
                                const T &c) {
    return a * (1 - bary.x - bary.y) + b * bary.x + c * bary.y;
}

inline RA_FUNCTION float clampf(float v, float min, float max) {
    return fminf(max, fmaxf(v, min));
}

inline RA_FUNCTION vec3 clamp(vec3 v, vec3 min, vec3 max) {
    v.x = fminf(max.x, fmaxf(v.x, min.x));
    v.y = fminf(max.y, fmaxf(v.y, min.y));
    v.z = fminf(max.z, fmaxf(v.z, min.z));
    return v;
}

inline RA_FUNCTION float color_average(vec3 color) {
    return (color[0] + color[1] + color[2]) / 3;
}

inline RA_FUNCTION float color_luminance(vec3 color) {
    return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
}

#endif
