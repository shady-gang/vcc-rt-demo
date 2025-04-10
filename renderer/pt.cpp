#include "pt.h"

#include "sampling.h"
#include "shading.h"
#include "emitter.h"
#include "bsdf.h"
#include "rendercontext.h"

RA_FUNCTION inline float compute_rr_factor(vec3 color, int depth) {
    return depth < 2 ? 1.0f : clampf(2 * color_luminance(color), 0.05f, 0.95f);
}

RA_FUNCTION vec3 pathtrace(RNGState* rng, Ray ray, int depth, vec3 throughput, float prev_pdf, const RenderContext& ctx) {
    const float offset = 0.001f;

    vec3 lo = 0.0f;

    if (depth > ctx.max_depth)
        return vec3(0);

    int dc;
    Hit hit { .t = ray.tmax };
    if (ctx.bvh->intersect(ray, hit, &dc)) {
        Triangle tri = ctx.primitives[hit.prim_id];
        Material mat = ctx.materials[tri.mat_id];

        // TODO: NEE
        vec3 emission = throughput * mat.emission;

        vec3 n = tri.get_vertex_normal(hit.primary);
        vec3 fn = ray.dir.dot(n) > 0 ? -n : n; // Ensure normal is facing forward
        vec3 p = tri.get_position(hit.primary);
        auto frame = shading::make_shading_frame(fn);

        const auto sample = shading::sample_material(rng, shading::to_local(-ray.dir, frame), mat);

        // Handle rr
        float rr = compute_rr_factor(sample.color * throughput, depth);
        if (randf(rng) > rr)
            return emission;

        Ray bounced_ray = { 
            .origin = p,
            .dir = shading::to_world(sample.dir, frame),
            .tmin = 0,
            .tmax = 1.0e+17f,
        };
        bounced_ray.origin = bounced_ray.origin + bounced_ray.dir * offset;

        return emission + pathtrace(rng, bounced_ray, depth + 1, sample.color * throughput / rr, sample.pdf * rr, ctx);
    } else {
        return throughput * ctx.emitters[0].emission;
    }
}