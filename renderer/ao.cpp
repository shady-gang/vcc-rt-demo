#include "ao.h"

#include "sampling.h"
#include "shading.h"

RA_FUNCTION vec3 pathtrace_ao(RNGState* rng, BVH& bvh, const Triangle* tris, Ray ray) {
    const float offset = 0.001f;

    vec3 contrib = vec3(0);
    
    Hit hit { .t = ray.tmax };
    if (bvh.intersect(ray, hit)) {
        Triangle tri = tris[hit.prim_id];

        vec3 n = tri.get_vertex_normal(hit.primary);
        vec3 fn = ray.dir.dot(n) > 0 ? -n : n; // Ensure normal is facing forward
        vec3 p = tri.get_position(hit.primary);
        auto frame = shading::make_shading_frame(fn);

        auto sample = shading::sample_cosine_hemisphere(randf(rng), randf(rng));

        Ray bounced_ray = { .origin = p };
        bounced_ray.origin = p;
        bounced_ray.dir = shading::to_world(sample.dir, frame);
        bounced_ray.origin = bounced_ray.origin + bounced_ray.dir * offset;
        bounced_ray.tmax = 1.0e+17f;

        // AO only bounces once
        hit = Hit{ .t = ray.tmax };
        if (bvh.intersect(bounced_ray, hit))
            return vec3(0);
        else
            return vec3(fabs(sample.dir.z/*cosine*/)) / sample.pdf;
    } else {
        return vec3(1);
    }
}