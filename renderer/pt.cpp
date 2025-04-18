#include "pt.h"

#include "sampling.h"
#include "shading.h"
#include "emitter.h"
#include "bsdf.h"

RA_FUNCTION inline int sample_emitter(RNGState* rng, int num_lights) {
    return (int)randi_max(rng, num_lights);
}

RA_FUNCTION inline float compute_rr_factor(vec3 color, int depth) {
    return depth < 2 ? 1.0f : clampf(2 * color_luminance(color), 0.05f, 0.95f);
}

RA_FUNCTION vec3 pt_handle_nee(RNGState* rng, float prev_pdf, vec3 out_dir, vec3 pos_surface, vec2 uv_surface, const Material& mat, const shading::ShadingFrame& frame, const RenderContext& ctx) {
    const float offset = 0.001f;

    int picked_light = sample_emitter(rng, ctx.num_lights-1) + 1; // Skip environment map (id == 0)
    Emitter emitter  = ctx.emitters[picked_light];
    Triangle tri     = ctx.primitives[emitter.prim_id];
    vec2 bary        = tri.sample_point_on_surface(rng);
    vec3 pos_light   = tri.get_position(bary);
    vec3 fn          = tri.get_face_normal();
    float area       = tri.get_area();

    vec3 in_dir = (pos_light - pos_surface);    
    float dist2 = lengthSquared(in_dir);
    if (dist2 <= __FLT_EPSILON__)
        return vec3(0);

    float dist = sqrtf(dist2);
    in_dir     = in_dir / dist;

    Ray shadow_ray = { 
        .origin = pos_surface,
        .dir    = in_dir,
        .tmin   = offset,
        .tmax   = dist - offset,
    };

    if (ctx.bvh->intersect_shadow(shadow_ray))
        return vec3(0);

    float dot     = fmaxf(fn.dot(-in_dir), 0);
    float geom    = dot <= __FLT_EPSILON__ ? 0 : dist2 / dot;
    float pdf_nee = geom / ( area * (ctx.num_lights - 1));

    if (pdf_nee <= __FLT_EPSILON__)
        return vec3(0);

    vec3 out_dir_s  = shading::to_local(out_dir, frame);
    vec3 in_dir_s   = shading::to_local(in_dir, frame);
    float pdf_bsdf  = shading::pdf_material(in_dir_s, out_dir_s, uv_surface, mat, ctx.textures);
    vec3 bsdfFactor = shading::eval_material(in_dir_s, out_dir_s, uv_surface, mat, ctx.textures);                                                                                                                                                                                 

    float mis = 1 / (1 + pdf_bsdf / pdf_nee);
    return mis * bsdfFactor * emitter.emission / pdf_nee;
}

// Note: This is a basic pathtracer with NEE but only for area lights
RA_FUNCTION vec3 pathtrace(RNGState* rng, Ray ray, int depth, vec3 throughput, float prev_pdf, const RenderContext& ctx) {
    const float offset = 0.001f;

    vec3 lo = 0.0f;

    if (depth > ctx.max_depth)
        return vec3(0);
    
    Hit hit { .t = ray.tmax };
    if (ctx.bvh->intersect(ray, hit)) {
        Triangle tri = ctx.primitives[hit.prim_id];
        Material mat = ctx.materials[tri.mat_id];

        vec3 contrib = vec3(0);

        vec3 n     = tri.get_vertex_normal(hit.primary);
        vec3 p     = tri.get_position(hit.primary);
        vec2 uv    = tri.get_texcoords(hit.primary);
        vec3 fn    = tri.get_face_normal();
        auto frame = shading::make_shading_frame(n);

        // Handle NEE if enabled and there is enough room
        if (ctx.enable_nee && depth + 1 <= ctx.max_depth)
            contrib = contrib + throughput * pt_handle_nee(rng, prev_pdf, -ray.dir, p, uv, mat, frame, ctx);

        // Handle emissive hits only when hit from the front
        float fn_dot = fmaxf(fn.dot(-ray.dir), 0);
        if (fn_dot > __FLT_EPSILON__) {
            vec3 emission = throughput * mat.emission;
            float mis = 1;
            if (ctx.enable_nee && depth > 0) {
                float area = tri.get_area();
                float dist2 = lengthSquared(p - ray.origin);
                float geom = dist2 / fn_dot;
                float pdf_nee = geom / ( area * (ctx.num_lights - 1));
                mis = 1 / (1 + pdf_nee / prev_pdf);
            }
            contrib = contrib + emission * mis;
        }

        // Next bounce
        const auto sample = shading::sample_material(rng, shading::to_local(-ray.dir, frame), uv, mat, ctx.textures);
        if (sample.pdf <= __FLT_EPSILON__)
            return contrib;

        // - Handle rr
        float rr = compute_rr_factor(sample.color * throughput, depth);
        if (randf(rng) > rr)
            return contrib;

        Ray bounced_ray = { 
            .origin = p,
            .dir  = shading::to_world(sample.dir, frame),
            .tmin = offset,
            .tmax = __FLT_MAX__,
        };

        return contrib + pathtrace(rng, bounced_ray, depth + 1, throughput * sample.color / rr, sample.pdf, ctx);
    } else {
        return throughput * ctx.emitters[0].emission;
    }
}