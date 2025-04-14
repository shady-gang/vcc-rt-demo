#ifndef RA_PT_H_
#define RA_PT_H_

#include "bvh.h"
#include "random.h"
#include "rendercontext.h"

RA_FUNCTION vec3 pathtrace(RNGState* rng, Ray ray, int depth, vec3 throughput, float prev_pdf, const RenderContext& ctx);

#endif