#ifndef RA_RENDERCONTEXT_H_
#define RA_RENDERCONTEXT_H_

#include "ra_math.h"

struct Triangle;
struct Material;
struct Emitter;
struct BVH;

struct RenderContext {
    const Triangle* primitives;
    const Material* materials;
    int num_lights;
    const Emitter* emitters;
    BVH* bvh;

    int max_depth;
    bool enable_nee;
};

#endif