#ifndef RA_RENDERER_H
#define RA_RENDERER_H

#include "ra_math.h"
using namespace nasl;

#include "camera.h"
#include "primitives.h"
#include "material.h"
#include "emitter.h"
#include "bvh.h"

enum RenderMode {
    FACENORMAL,
    VERTEXNORMAL,
    TEXCOORDS,
    PRIM_IDS,
    PRIMARY_HEATMAP,
    AO,
    PT,

    MAX_RENDER_MODE = PT,
    DEFAULT_RENDER_MODE = PT,
};

#define RA_RENDERER_SIGNATURE void render_a_pixel(Camera cam, int width, int height, uint32_t* fb, float* film, int ntris, Triangle* triangles, Material* materials, Emitter* emitters, BVH bvh, unsigned frame, unsigned accum, RenderMode mode, int max_depth)

#endif
