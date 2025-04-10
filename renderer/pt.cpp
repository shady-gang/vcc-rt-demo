#include "pt.h"

#include "sampling.h"
#include "shading.h"
#include "bsdf.h"

RA_CONSTANT float PTEnvMap = 1/(4*pi);

RA_FUNCTION inline float compute_rr_factor(vec3 color, int depth) {
    return depth < 2 ? 1.0f : clampf(2 * color_luminance(color), 0.05f, 0.95f);
}

RA_FUNCTION vec3 pathtrace1(RNGState* rng, BVH& bvh, Ray ray, int path_len, vec3 li, float pdf) {
    vec3 lo = 0.0f;

    if (path_len > 1)
        return vec3(0);

    int dc;
    Hit hit { .t = ray.tmax };
    if (bvh.intersect(ray, hit, &dc)) {
        /*vec3 p = ray.origin + ray.dir * hit.t + hit.n * epsilon;
        vec3 basis[3];
        orthoBasis(basis, hit.n);

        Ray bounced_ray = { .origin = p };
        bounced_ray.origin = p;
        // local -> global
        float u = randf(rng);
        auto sample_dir = sample_cosine_hemisphere(u, randf(rng));
        float rx = sample_dir.dir.x * basis[0].x + sample_dir.dir.y * basis[1].x + sample_dir.dir.z * basis[2].x;
        float ry = sample_dir.dir.x * basis[0].y + sample_dir.dir.y * basis[1].y + sample_dir.dir.z * basis[2].y;
        float rz = sample_dir.dir.x * basis[0].z + sample_dir.dir.y * basis[1].z + sample_dir.dir.z * basis[2].z;
        bounced_ray.dir.x = rx;
        bounced_ray.dir.y = ry;
        bounced_ray.dir.z = rz;

        bounced_ray.origin = bounced_ray.origin + bounced_ray.dir * epsilon;
        bounced_ray.tmax = 1.0e+17f;

        lo = lo + pathtrace(rng, bvh, bounced_ray, path_len + 1, vec3(0), pdf * sample_dir.pdf);*/
    } else
        lo = lo + vec3(PTEnvMap) / pdf;

    return lo;
}

RA_FUNCTION vec3 pathtrace(RNGState* rng, BVH& bvh, Ray ray, int depth, vec3 throughput, float prev_pdf, int max_depth, Material* materials) {
    const float offset = 0.001f;

    vec3 lo = 0.0f;

    if (depth > max_depth)
        return vec3(0);

    int dc;
    Hit hit { .t = ray.tmax };
    if (bvh.intersect(ray, hit, &dc)) {
        Triangle tri = bvh.tris[hit.prim_id];

        // TODO: Check emission

        vec3 n = tri.get_vertex_normal(hit.primary);
        vec3 fn = ray.dir.dot(n) > 0 ? -n : n; // Ensure normal is facing forward
        vec3 p = tri.get_position(hit.primary);
        auto frame = shading::make_shading_frame(fn);

        const auto sample = shading::sample_material(rng, shading::to_local(-ray.dir, frame), &materials[tri.mat_id]);

        // Handle rr
        float rr = compute_rr_factor(sample.color * throughput, depth);
        if (randf(rng) > rr)
            return vec3(0);

        Ray bounced_ray = { 
            .origin = p,
            .dir = shading::to_world(sample.dir, frame),
            .tmin = 0,
            .tmax = 1.0e+17f,
        };
        bounced_ray.origin = bounced_ray.origin + bounced_ray.dir * offset;

        // TODO: Fix recursion bug
        return pathtrace(rng, bvh, bounced_ray, depth + 1, sample.color * throughput / rr, sample.pdf * rr, max_depth, materials);
    } else {
        return throughput * PTEnvMap;
    }
}