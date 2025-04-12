#ifndef RA_BSDF_H_
#define RA_BSDF_H_

#include "shading.h"
#include "random.h"
#include "material.h"
#include "texture.h"

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

RA_FUNCTION vec3 eval_conductor(vec3 in_dir, vec3 out_dir, vec3 albedo, vec3 specular, float ior, float kappa, float alpha);
RA_FUNCTION float pdf_conductor(vec3 in_dir, vec3 out_dir, float ior, float kappa, float alpha);
RA_FUNCTION BsdfSample sample_conductor(RNGState* rng, vec3 out_dir, vec3 albedo, vec3 specular, float ior, float kappa, float alpha);

RA_FUNCTION vec3 eval_perfect_dielectric(vec3 in_dir, vec3 out_dir, vec3 specular, vec3 transmission, float ior);
RA_FUNCTION float pdf_perfect_dielectric(vec3 in_dir, vec3 out_dir, float ior);
RA_FUNCTION BsdfSample sample_perfect_dielectric(RNGState* rng, vec3 out_dir, vec3 specular, vec3 transmission, float ior);

RA_FUNCTION vec3 eval_dielectric(vec3 in_dir, vec3 out_dir, vec3 specular, vec3 transmission, float ior, float alpha);
RA_FUNCTION float pdf_dielectric(vec3 in_dir, vec3 out_dir, float ior, float alpha);
RA_FUNCTION BsdfSample sample_dielectric(RNGState* rng, vec3 out_dir, vec3 specular, vec3 transmission, float ior, float alpha);

RA_FUNCTION vec3 eval_material(vec3 in_dir, vec3 out_dir, vec2 uv, const Material& mat, const TextureSystem& textures);
RA_FUNCTION float pdf_material(vec3 in_dir, vec3 out_dir, vec2 uv, const Material& mat, const TextureSystem& textures);
RA_FUNCTION BsdfSample sample_material(RNGState* rng, vec3 out_dir, vec2 uv, const Material& mat, const TextureSystem& textures);

}
#endif