#include "bsdf.h"
#include "sampling.h"
#include "microfacet.h"
#include "fresnel.h"

namespace shading {

// ----------------------------- Diffuse (doublesided)
RA_FUNCTION vec3 eval_diffuse(vec3 in_dir, vec3 out_dir, vec3 albedo) {
    if (!is_same_hemisphere(in_dir, out_dir)) return vec3(0); // No tranmission
    return albedo * fabs(in_dir.z) / M_PI;
}

RA_FUNCTION float pdf_diffuse(vec3 in_dir, vec3 out_dir) {
    if (!is_same_hemisphere(in_dir, out_dir)) return 0; // No tranmission
    return cosine_hemisphere_pdf(fabs(in_dir.z));
}

RA_FUNCTION BsdfSample sample_diffuse(RNGState* rng, vec3 out_dir, vec3 albedo) {
    auto sample = sample_cosine_hemisphere(randf(rng), randf(rng));

    // Ensure it is the same hemisphere
    if (out_dir.z < 0)
        sample.dir = -sample.dir;

    return BsdfSample {
        .dir = sample.dir,
        .pdf = sample.pdf,
        .color = albedo,
        .eta = 1
    };
}

// ----------------------------- Conductor (doublesided)
RA_FUNCTION vec3 eval_conductor(vec3 in_dir, vec3 out_dir, vec3 albedo, vec3 specular, float ior, float kappa, float alpha) {
    if (!is_same_hemisphere(in_dir, out_dir)) return vec3(0); // No tranmission

    float cos_o = fabs(out_dir.z);
    float cos_i = fabs(in_dir.z);

    if (cos_o <= __FLT_EPSILON__ || cos_i <= __FLT_EPSILON__) return vec3(0);

    vec3 h  = normalize(in_dir + out_dir);
    float d = microfacet::eval_D(h, alpha, alpha);
    float g = microfacet::eval_G(in_dir, out_dir, h, alpha, alpha);
    float f = fresnel::conductor_factor(ior, kappa, cos_i);
    float a = d * g * microfacet::reflective_jacobian(cos_o); // Not cos_h_o

    return ((1-f) * albedo + f * specular) * a;
}

RA_FUNCTION float pdf_conductor(vec3 in_dir, vec3 out_dir, float ior, float kappa, float alpha) {
    if (!is_same_hemisphere(in_dir, out_dir)) return 0; // No tranmission

    float cos_o = fabs(out_dir.z);
    float cos_i = fabs(in_dir.z);

    if (cos_o <= __FLT_EPSILON__ || cos_i <= __FLT_EPSILON__) return 0;

    vec3 h        = normalize(in_dir + out_dir);
    float cos_h_o = fabs(out_dir.dot(h));
    if (cos_h_o <= __FLT_EPSILON__) return 0;

    float jacob = microfacet::reflective_jacobian(cos_h_o);
    if (jacob <= __FLT_EPSILON__) return 0;

    float pdf = microfacet::pdf_vndf_ggx(in_dir, h, alpha, alpha);
    return pdf * jacob;
}

RA_FUNCTION BsdfSample sample_conductor(RNGState* rng, vec3 out_dir, vec3 albedo, vec3 specular, float ior, float kappa, float alpha) {
    float cos_o = fabs(out_dir.z);

    if (cos_o <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    vec3 s = microfacet::sample_vndf_ggx(rng, out_dir, alpha, alpha);
    if (lengthSquared(s) <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    vec3 h = normalize(s);
    if (h.dot(out_dir) < 0)
        h = -h;

    vec3 in_dir = reflect(out_dir, h);

    float cos_i = fabs(in_dir.z);
    if (cos_i <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    float cos_h_o = fabs(out_dir.dot(h)); // = cos_h_i
    float jacob = microfacet::reflective_jacobian(cos_h_o); // Jacobian of the half-direction mapping
    float pdf   = microfacet::pdf_vndf_ggx(out_dir, h, alpha, alpha);

    if (pdf <= __FLT_EPSILON__ || jacob <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    return BsdfSample {
        .dir   = in_dir,
        .pdf   = pdf * jacob,
        .color = eval_conductor(in_dir, out_dir, albedo, specular, ior, kappa, alpha) / (pdf * jacob), // TODO: Not really efficient
        .eta   = 1
    };
}

// ----------------------------- Perfect dielectric
RA_FUNCTION vec3 eval_perfect_dielectric(vec3 in_dir, vec3 out_dir, vec3 specular, vec3 transmission, float ior) {
    return vec3(0);
}

RA_FUNCTION float pdf_perfect_dielectric(vec3 in_dir, vec3 out_dir, float ior) {
    return 0;
}

RA_FUNCTION BsdfSample sample_perfect_dielectric(RNGState* rng, vec3 out_dir, vec3 specular, vec3 transmission, float ior) {
    float cos_o = out_dir.z;

    if (fabs(cos_o) <= __FLT_EPSILON__) return BsdfSample {.pdf=0};

    bool flip = cos_o < 0;
    float eta = flip ? ior : 1 / ior;
    if (flip) {
        cos_o   = -cos_o;
        out_dir = -out_dir;
    }

    auto fterm = fresnel::fresnel(eta, cos_o);

    vec3 in_dir;
    bool is_transmission;
    if (randf(rng) > fterm.factor) {
        // Refraction
        in_dir = refract(out_dir, vec3(0,0,1), eta, cos_o, fterm.cos_t);
        is_transmission = true;
    } else {
        // Reflection
        in_dir = reflect(out_dir, vec3(0,0,1));
        is_transmission = false;
    }

    return BsdfSample {
        .dir   = flip ? -in_dir : in_dir,
        .pdf   = 1,
        .color = is_transmission ? transmission : specular,
        .eta   = is_transmission ? eta : 1
    };
}

// ----------------------------- Dielectric
RA_FUNCTION vec3 eval_dielectric(vec3 in_dir, vec3 out_dir, vec3 specular, vec3 transmission, float ior, float alpha) {
    float cos_o = out_dir.z;
    float cos_i = in_dir.z;

    if (fabs(cos_o * cos_i) <= __FLT_EPSILON__) return vec3(0);

    bool flip = cos_o < 0;
    float eta = flip ? ior : 1 / ior;
    if (flip) {
        cos_o   = -cos_o;
        out_dir = -out_dir;

        cos_i  = -cos_i;
        in_dir = -in_dir;
    }

    const bool is_transmission = !is_same_hemisphere(in_dir, out_dir);
    vec3 h = is_transmission ? normalize(in_dir + eta * out_dir) : normalize(in_dir + out_dir);

    float cos_h_o = h.dot(out_dir);
    float cos_h_i = h.dot(in_dir);
    if (cos_h_o <= __FLT_EPSILON__ || fabs(cos_h_i) <= __FLT_EPSILON__) return vec3(0);

    float f = fresnel::fresnel(eta, cos_h_o).factor;
    float d = microfacet::eval_D(h, alpha, alpha);
    float g = microfacet::eval_G(in_dir, out_dir, h, alpha, alpha);

    if (is_transmission) {
        float jacob = microfacet::refractive_jacobian(eta, cos_h_i, cos_h_o);
        float a = d * g * fabs(cos_h_o * jacob / cos_o);
        return (1 - f) * transmission * a;
    } else {
        float a = d * g * microfacet::reflective_jacobian(cos_o); // Not cos_h_o
        return f * specular * a;
    }
}

RA_FUNCTION float pdf_dielectric(vec3 in_dir, vec3 out_dir, float ior, float alpha) {
    float cos_o = out_dir.z;
    float cos_i = in_dir.z;

    if (fabs(cos_o * cos_i) <= __FLT_EPSILON__) return 0;

    bool flip = cos_o < 0;
    float eta = flip ? ior : 1 / ior;
    if (flip) {
        cos_o   = -cos_o;
        out_dir = -out_dir;

        cos_i  = -cos_i;
        in_dir = -in_dir;
    }

    const bool is_transmission = !is_same_hemisphere(in_dir, out_dir);
    vec3 h = is_transmission ? normalize(in_dir + eta * out_dir) : normalize(in_dir + out_dir);

    float cos_h_o = h.dot(out_dir);
    float cos_h_i = h.dot(in_dir);
    if (cos_h_o <= __FLT_EPSILON__ || fabs(cos_h_i) <= __FLT_EPSILON__) return 0;

    float f = fresnel::fresnel(eta, cos_h_o).factor;

    float pdf = microfacet::pdf_vndf_ggx(out_dir, h, alpha, alpha);
    if (pdf <= __FLT_EPSILON__) return 0;

    if (is_transmission) {
        float jacob = microfacet::refractive_jacobian(eta, cos_h_i, cos_h_o);
        return (1 - f) * pdf * fabs(jacob);
    } else {
        return f * pdf * microfacet::reflective_jacobian(cos_h_o);
    }
}

RA_FUNCTION BsdfSample sample_dielectric(RNGState* rng, vec3 out_dir, vec3 specular, vec3 transmission, float ior, float alpha) {
    float cos_o = out_dir.z;
    if (fabs(cos_o) <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    // Flip such that N and wo facing forward
    bool flip = cos_o < 0;
    float eta = flip ? ior : 1 / ior;
    if (flip) {
        out_dir = -out_dir;
        cos_o   = -cos_o;
    }

    vec3 s = microfacet::sample_vndf_ggx(rng, out_dir, alpha, alpha);
    // Reject invalid normals
    if (lengthSquared(s) <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    vec3 h = normalize(s);
    float cos_h_o = out_dir.dot(h);
    // Reject if on the wrong hemisphere
    if (cos_h_o <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    auto fterm = fresnel::fresnel(eta, cos_h_o);
    // float d = microfacet::eval_D(h, alpha, alpha);

    vec3 in_dir;
    float pdf;
    bool is_transmission;
    vec3 weight;
    if (randf(rng) > fterm.factor) {
        // Refraction
        in_dir = refract(out_dir, h, eta, cos_h_o, fterm.cos_t);
        float cos_h_i = in_dir.dot(h);

        float jacob = microfacet::refractive_jacobian(eta, cos_h_i, cos_h_o);
        pdf = (1 - fterm.factor) * fabs(jacob);
        is_transmission = true;
        
        float g1 = microfacet::g_1_smith(in_dir, alpha, alpha);
        float a = g1 * (cos_h_o / cos_o) * (cos_h_o / cos_o);
        weight = transmission * a;        
        
        // Reject if wrong hemisphere
        if (in_dir.z > 0)
            return BsdfSample{.pdf = 0};
    } else {
        // Reflection
        in_dir = reflect(out_dir, h);
        float jacob = microfacet::reflective_jacobian(cos_h_o);
        pdf = fterm.factor * jacob;
        is_transmission = false;

        float g1 = microfacet::g_1_smith(in_dir, alpha, alpha);
        float a = g1 * cos_h_o / cos_o;
        weight = specular * a;

        // Reject if wrong hemisphere
        if (in_dir.z < 0)
            return BsdfSample{.pdf = 0};
    }

    pdf *= microfacet::pdf_vndf_ggx(out_dir, h, alpha, alpha);
    if (pdf <= __FLT_EPSILON__) return BsdfSample{.pdf = 0};

    // Note: no adjoint 
    return BsdfSample {
        .dir   = flip ? -in_dir : in_dir,
        .pdf   = pdf,
        .color = weight,
        .eta   = is_transmission ? eta : 1
    };
}

// ----------------------------- Material ~ Principled
RA_CONSTANT float PrincipledConductorIOR   = 1.0f;
RA_CONSTANT float PrincipledConductorKappa = 0.0f;

RA_FUNCTION vec3 eval_material(vec3 in_dir, vec3 out_dir, vec2 uv, const Material& mat, const Texture* textures) {
    vec3 base_color = texture::lookup_color_property(uv, mat.base_color, mat.base_color_tex, textures);

    vec3 color = vec3(0);
    if (mat.metallic > 0)
        color = color + mat.metallic * eval_conductor(in_dir, out_dir, base_color, base_color, PrincipledConductorIOR, PrincipledConductorKappa, mat.roughness);
    
    if ((1 - mat.metallic) * mat.transmission > 0)
        color = color + (1 - mat.metallic) * mat.transmission * eval_dielectric(in_dir, out_dir, base_color, base_color, mat.ior, mat.roughness);

    if ((1 - mat.metallic) * (1 - mat.transmission) > 0)
        color = color + (1 - mat.metallic) * (1 - mat.transmission) * eval_diffuse(in_dir, out_dir, base_color);
    return color;
}

RA_FUNCTION float pdf_material(vec3 in_dir, vec3 out_dir, vec2 uv, const Material& mat, const Texture* textures) {
    float pdf = 0;
    if (mat.metallic > 0)
        pdf += mat.metallic * pdf_conductor(in_dir, out_dir, PrincipledConductorIOR, PrincipledConductorKappa, mat.roughness);

    if ((1 - mat.metallic) * mat.transmission > 0)
        pdf += (1 - mat.metallic) * mat.transmission * pdf_dielectric(in_dir, out_dir, mat.ior, mat.roughness);
    
    if ((1 - mat.metallic) * (1 - mat.transmission) > 0)
        pdf += (1 - mat.metallic) * (1 - mat.transmission) * pdf_diffuse(in_dir, out_dir);
    return pdf;
}

RA_FUNCTION BsdfSample sample_material(RNGState* rng, vec3 out_dir, vec2 uv, const Material& mat, const Texture* textures) {
    vec3 base_color = texture::lookup_color_property(uv, mat.base_color, mat.base_color_tex, textures);

    BsdfSample sample;
    float new_pdf;
    if (randf(rng) < mat.metallic) {
        sample = sample_conductor(rng, out_dir, base_color, base_color, PrincipledConductorIOR, PrincipledConductorKappa, mat.roughness);
        new_pdf = mat.metallic * sample.pdf 
                + (1 - mat.metallic) * mat.transmission * pdf_dielectric(sample.dir, out_dir, mat.ior, mat.roughness) 
                + (1 - mat.metallic) * (1 - mat.transmission) * pdf_diffuse(sample.dir, out_dir);
    } else if (randf(rng) < mat.transmission) {
        sample = sample_dielectric(rng, out_dir, base_color, base_color, mat.ior, mat.roughness);
        new_pdf = mat.metallic * pdf_conductor(sample.dir, out_dir, PrincipledConductorIOR, PrincipledConductorKappa, mat.roughness) 
                + (1 - mat.metallic) * mat.transmission * sample.pdf 
                + (1 - mat.metallic) * (1 - mat.transmission) * pdf_diffuse(sample.dir, out_dir);
    } else {
        sample = sample_diffuse(rng, out_dir, base_color);
        new_pdf = mat.metallic * pdf_conductor(sample.dir, out_dir, PrincipledConductorIOR, PrincipledConductorKappa, mat.roughness) 
                + (1 - mat.metallic) * mat.transmission * pdf_dielectric(sample.dir, out_dir, mat.ior, mat.roughness) 
                + (1 - mat.metallic) * (1 - mat.transmission) * sample.pdf;
    }
    
    sample.color = sample.color * sample.pdf;
    sample.pdf   = new_pdf;
    sample.color = sample.color / sample.pdf;
    return sample;
}
}