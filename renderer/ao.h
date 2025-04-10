#ifndef RA_AO_H_
#define RA_AO_H_

#include "bvh.h"
#include "random.h"

RA_FUNCTION vec3 pathtrace_ao(RNGState* rng, BVH& bvh, Ray ray);
#endif