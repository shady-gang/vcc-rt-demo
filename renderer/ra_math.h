#ifndef RA_MATH
#define RA_MATH

#ifdef __SHADY__
#include "shady.h"
static float M_PI = 3.14159f;
static float sinf(float) __asm__("shady::pure_op::GLSL.std.450::13::Invocation");
static float cosf(float) __asm__("shady::pure_op::GLSL.std.450::14::Invocation");
static float tanf(float) __asm__("shady::pure_op::GLSL.std.450::15::Invocation");
static float fmaf(float, float, float) __asm__("shady::pure_op::GLSL.std.450::50::Invocation");
static float fminf(float, float) __asm__("shady::pure_op::GLSL.std.450::37::Invocation");
static float fmaxf(float, float) __asm__("shady::pure_op::GLSL.std.450::40::Invocation");
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

static float dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

#endif
