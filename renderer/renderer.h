#ifndef RA_RENDERER_H
#define RA_RENDERER_H

#include "ra_math.h"
using namespace nasl;

//#include <stdint.h>
typedef unsigned int uint32_t;

#include "camera.h"
#include "primitives.h"
#include "bvh.h"

enum RenderMode {
    PRIMARY,
    PRIMARY_HEATMAP,
    AO,

    MAX_RENDER_MODE = AO,
};

#define RA_RENDERER_SIGNATURE void render_a_pixel(Camera cam, int width, int height, uint32_t* buf, int ntris, Triangle* triangles, BVH bvh, unsigned frame, unsigned accum, RenderMode mode)

#endif
