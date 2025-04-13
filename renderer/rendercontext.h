#ifndef RA_RENDERCONTEXT_H_
#define RA_RENDERCONTEXT_H_

#include "ra_math.h"
#include "texture.h"

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
    TextureSystem textures;

    int max_depth;
    bool enable_nee;
};

#endif