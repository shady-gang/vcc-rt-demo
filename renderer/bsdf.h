#ifndef RA_BSDF_H_
#define RA_BSDF_H_

#include "shading.h"
#include "random.h"
#include "material.h"

// All following functions assume shading space
namespace shading {

struct BsdfSample {
    // Sampled direction
    vec3 dir;
    // Sample pdf
    float pdf;
    // Color of the sample with cosine and pdf already applied
    vec3 color;
    // Eta (ior)
    float eta;
};

RA_FUNCTION vec3 eval_diffuse(vec3 in_dir, vec3 out_dir, vec3 albedo);
RA_FUNCTION float pdf_diffuse(vec3 in_dir, vec3 out_dir);
RA_FUNCTION BsdfSample sample_diffuse(RNGState* rng, vec3 out_dir, vec3 albedo);

RA_FUNCTION vec3 eval_material(vec3 in_dir, vec3 out_dir, const Material& mat);
RA_FUNCTION float pdf_material(vec3 in_dir, vec3 out_dir, const Material& mat);
RA_FUNCTION BsdfSample sample_material(RNGState* rng, vec3 out_dir, const Material& mat);

}
#endif