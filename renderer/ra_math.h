#ifndef RA_MATH
#define RA_MATH

// static float min(float a, float b) { if (a < b) return a; return b; }
// static float max(float a, float b) { if (a > b) return a; return b; }

#ifdef __SHADY__
#include "shady.h"
static float M_PI = 3.14159f;
static float sinf(float) __asm__("shady::pure_op::GLSL.std.450::13::Invocation");
static float cosf(float) __asm__("shady::pure_op::GLSL.std.450::14::Invocation");
static float tanf(float) __asm__("shady::pure_op::GLSL.std.450::15::Invocation");
static float fmaf(float, float, float) __asm__("shady::pure_op::GLSL.std.450::50::Invocation");
static float fminf(float, float) __asm__("shady::pure_op::GLSL.std.450::37::Invocation");
static float fmaxf(float, float) __asm__("shady::pure_op::GLSL.std.450::40::Invocation");
static float sqrtf(float) __asm__("shady::pure_op::GLSL.std.450::31::Invocation");
static float sign(float) __asm__("shady::pure_op::GLSL.std.450::6::Invocation");

typedef long int size_t;

#else
#include <cmath>
#include <cstddef>
static inline float sign(float f) {
    return copysignf(1.0f, f);
}
#endif

#include "nasl.h"

using namespace nasl;

static constexpr float epsilon = 1e-4f;

/// @brief Barycentric interpolation ([0,0] returns a, [1,0] returns b, and
/// [0,1] returns c).
template <typename T>
static T interpolateBarycentric(const vec2 &bary, const T &a, const T &b,
                                const T &c) {
    return a * (1 - bary.x - bary.y) + b * bary.x + c * bary.y;
}

/*static Vertex interpolate(const vec2 &bary, const Vertex &a,
                          const Vertex &b, const Vertex &c) {
    return {
        .position = interpolateBarycentric(
            bary, a.position, b.position, c.position),
        .uv = interpolateBarycentric(bary, a.uv, b.uv, c.uv),
        .normal =
        interpolateBarycentric(bary, a.normal, b.normal, c.normal),
    };
}*/

#endif
