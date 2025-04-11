#include "bsdf.h"
#include "sampling.h"

namespace shading {

RA_FUNCTION vec3 eval_diffuse(vec3 in_dir, vec3 out_dir, vec3 albedo) {
    return albedo * fmaxf(in_dir[2],0) / pi;
}

RA_FUNCTION BsdfSample sample_diffuse(RNGState* rng, vec3 out_dir, vec3 albedo) {
    auto sample = sample_cosine_hemisphere(randf(rng), randf(rng));

    return BsdfSample {
        .dir = sample.dir,
        .pdf = sample.pdf,
        .color = albedo,
        .eta = 1
    };
}

RA_FUNCTION vec3 eval_material(vec3 in_dir, vec3 out_dir, const Material& mat) {
    return eval_diffuse(in_dir, out_dir, mat.base_color);
}

RA_FUNCTION BsdfSample sample_material(RNGState* rng, vec3 out_dir, const Material& mat) {
    return sample_diffuse(rng, out_dir, mat.base_color);
}
}