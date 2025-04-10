#ifndef RA_PT_H_
#define RA_PT_H_

#include "bvh.h"
#include "random.h"

RA_FUNCTION vec3 pathtrace(RNGState* rng, BVH& bvh, Ray ray, int path_len, vec3 li, float pdf, int max_depth);

#endif