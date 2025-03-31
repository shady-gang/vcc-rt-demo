// #include <shady.h>
// using namespace vcc;
#include "ra_math.h"
using namespace nasl;

//#include <stdint.h>
typedef unsigned int uint32_t;

#include "camera.h"
#include "primitives.h"
#include "bvh.h"

static_assert(sizeof(Sphere) == sizeof(float) * 4);

RA_FUNCTION unsigned int FNVHash(char* str, unsigned int length) {
    const unsigned int fnv_prime = 0x811C9DC5;
    unsigned int hash = 0;
    unsigned int i = 0;

    for (i = 0; i < length; str++, i++)
    {
        hash *= fnv_prime;
        hash ^= (*str);
    }

    return hash;
}

RA_FUNCTION unsigned int nrand(unsigned int* rng) {
    unsigned int orand = *rng;
    *rng = FNVHash((char*) &orand, 4);
    return *rng;
}

RA_FUNCTION float drand48(unsigned int* rng) {
    float n = (nrand(rng) / 65536.0f);
    n = n - floorf(n);
    return n;
}

RA_FUNCTION void
orthoBasis(vec3 *basis, vec3 n)
{
    basis[2] = n;
    basis[1].x = 0.0f; basis[1].y = 0.0f; basis[1].z = 0.0f;

    if ((n.x < 0.6f) && (n.x > -0.6f)) {
        basis[1].x = 1.0f;
    } else if ((n.y < 0.6f) && (n.y > -0.6f)) {
        basis[1].y = 1.0f;
    } else if ((n.z < 0.6f) && (n.z > -0.6f)) {
        basis[1].z = 1.0f;
    } else {
        basis[1].x = 1.0f;
    }

    basis[0] = cross(basis[1], basis[2]);
    basis[0] = normalize(basis[0]);

    basis[1] = cross(basis[2], basis[0]);
    basis[1] = normalize(basis[1]);
}

#define NAO_SAMPLES 1

RA_FUNCTION vec3 ambient_occlusion(unsigned int* rng, BVH& bvh, const Hit *isect) {
    int    i, j;
    int    ntheta = NAO_SAMPLES;
    int    nphi   = NAO_SAMPLES;
    float eps = 0.0001f;

    vec3 p;

    p.x = isect->p.x + eps * isect->n.x;
    p.y = isect->p.y + eps * isect->n.y;
    p.z = isect->p.z + eps * isect->n.z;

    vec3 basis[3];
    orthoBasis(basis, isect->n);

    float occlusion = 0.0f;

    for (j = 0; j < ntheta; j++) {
        for (i = 0; i < nphi; i++) {
            float theta = sqrtf(drand48(rng));
            float phi   = 2.0f * M_PI * drand48(rng);

            float x = cosf(phi) * theta;
            float y = sinf(phi) * theta;
            float z = sqrtf(1.0f - theta * theta);

            // local -> global
            float rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
            float ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
            float rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

            Ray ray;

            ray.origin = p;
            ray.dir.x = rx;
            ray.dir.y = ry;
            ray.dir.z = rz;
            ray.tmax = 1.0e+17f;

            Hit occ_hit = { .t = ray.tmax };

            vec3 inv = vec3(1.0f) / ray.dir;
            int dc;
            if (bvh.intersect(ray, inv, occ_hit, &dc))
                occlusion += 1.0f;
        }
    }

    occlusion = (ntheta * nphi - occlusion) / (float)(ntheta * nphi);

    return vec3(occlusion);
}

RA_FUNCTION vec3 clamp(vec3 v, vec3 min, vec3 max) {
    v.x = fminf(max.x, fmaxf(v.x, min.x));
    v.y = fminf(max.y, fmaxf(v.y, min.y));
    v.z = fminf(max.z, fmaxf(v.z, min.z));
    return v;
}

RA_FUNCTION uint32_t pack_color(vec3 color) {
    color = clamp(color, vec3(0.0f), vec3(1.0f));
    return (((int) (color.z * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.x * 255) & 0xFF);
}

extern "C" {

#ifdef __SHADY__
#include "shady.h"
using namespace vcc;
#elif __CUDACC__
#define gl_GlobalInvocationID (uint3(threadIdx.x + blockDim.x * blockIdx.x, threadIdx.y + blockDim.y * blockIdx.y, threadIdx.z + blockDim.z * blockIdx.z))
#else
thread_local extern vec2 gl_GlobalInvocationID;
#endif

#ifdef __SHADY__
[[gnu::flatten]]
compute_shader local_size(16, 16, 1)
#elif __CUDACC__
__global__
#endif
void render_a_pixel(Camera cam, int width, int height, uint32_t* buf, int ntris, Triangle* triangles, BVH bvh, unsigned frame, bool heat) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = cam.position;
    Ray r = { origin, normalize(camera_get_forward_vec(&cam, vec3(dx, dy, -1.0f))), 0, 99999 };
    buf[(y * width + x)] = pack_color(vec3(0.0f, 0.5f, 1.0f));

    vec3 ray_inv_dir = vec3(1.0f) / r.dir;

    Hit nearest_hit = { r.tmin };
    int iter;

    if (ntris > 0) {
        for (int i = 0; i < ntris; i++) {
            Triangle& b = (triangles)[i];
            b.intersect(r, nearest_hit);
        }
    } else {
        // BVH shizzle
        bvh.intersect(r, ray_inv_dir, nearest_hit, &iter);
    }

    unsigned int rng = x * width + y + frame;
    if (nearest_hit.t > 0.0f)
        buf[(y * width + x)] = pack_color(ambient_occlusion(&rng, bvh, &nearest_hit));
    if (heat)
        buf[(y * width + x)] = pack_color(vec3(log2f(iter) / 8.0f));
    //buf[(y * width + x)] = pack_color(vec3f_to_vec3(forward));
}

}
