#include "renderer.h"

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

// FNV hash function
auto fnv_hash(uint32_t h, uint32_t d) -> uint32_t {
    h = (h * 16777619u) ^ ( d           & 0xFFu);
    h = (h * 16777619u) ^ ((d >>  8u) & 0xFFu);
    h = (h * 16777619u) ^ ((d >> 16u) & 0xFFu);
    h = (h * 16777619u) ^ ((d >> 24u) & 0xFFu);
    return h;
}

RA_FUNCTION unsigned int nrand(unsigned int* rng) {
    unsigned int orand = *rng;
    *rng = FNVHash((char*) &orand, 4);
    return *rng;
}

RA_FUNCTION auto xorshift(uint32_t* seed) -> uint32_t {
    auto x = *seed;
    // x = select(x == 0u, 1u, x);
    x = (x == 0) ? 1u : x;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    *seed = x;
    return x;
}

//auto randi = xorshift;
#define randi xorshift

// [0.0, 1.0]
RA_FUNCTION auto randf(uint32_t* rnd) -> float {
    // Assumes IEEE 754 floating point format
    auto x = randi(rnd);
    //return std::bit_cast<float>((127u << 23u) | (x & 0x7FFFFFu)) - 1.0f;
    unsigned i = (127u << 23u) | (x & 0x7FFFFFu);
    float f;
    f = *(float*) &i;
    return f - 1.0f;
    //return std::bit_cast<float>() - 1.0f;
}

/*RA_FUNCTION auto randf(uint32_t* rnd) -> float {
    // Assumes IEEE 754 floating point format
    auto x = randi(rnd);
    //return std::bit_cast<float>((127u << 23u) | (x & 0x7FFFFFu)) - 1.0f;
    unsigned i = (127u << 23u) | (x & 0x7FFFFFu);
    float f;
    f = *(float*) &i;
    return f / 2.0f - 0.5f;
}*/

struct DirSample {
    vec3 dir;
    float pdf;
};

// Probability density function for uniform sphere sampling
RA_FUNCTION auto uniform_sphere_pdf() -> float { return 1.0f / (4.0f * M_PI); }

RA_FUNCTION auto make_dir_sample(float c, float s, float phi, float pdf) -> DirSample {
    auto x = s * cosf(phi);
    auto y = s * sinf(phi);
    auto z = c;
    return DirSample {
        .dir = vec3(x, y, z),
        .pdf = pdf
    };
}

// Samples a direction uniformly on a sphere
RA_FUNCTION auto sample_uniform_sphere(float u, float v) -> DirSample {
    auto c = 2.0f * v - 1.0f;
    auto s = sqrtf(1.0f - c * c);
    auto phi = 2.0f * M_PI * u;
    return make_dir_sample(c, s, phi, uniform_sphere_pdf());
}

// Probability density function for cosine weighted hemisphere sampling
RA_FUNCTION auto cosine_hemisphere_pdf(float c) -> float { return c * (1.0f / M_PI); }

RA_FUNCTION auto sample_cosine_hemisphere(float u, float v) -> DirSample {
    auto c = sqrtf(1.0f - v);
    auto s = sqrtf(v);
    auto phi = 2.0f * M_PI * u;
    return make_dir_sample(c, s, phi, cosine_hemisphere_pdf(c));
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

RA_FUNCTION vec3 ambient_occlusion(unsigned int* rng, BVH& bvh, Ray prev_ray, const Hit *isect) {
    int    i, j;
    int    ntheta = NAO_SAMPLES;
    int    nphi   = NAO_SAMPLES;
    float eps = 0.0001f;

    vec3 p;

    //p.x = ray_origin.x * isect->t + eps * isect->n.x;
    //p.y = ray_origin.y * isect->t + eps * isect->n.y;
    //p.z = ray_origin.z * isect->t + eps * isect->n.z;

    p = prev_ray.origin + prev_ray.dir * isect->t + isect->n * epsilon;

    vec3 basis[3];
    orthoBasis(basis, isect->n);

    float occlusion = 0.0f;

    for (j = 0; j < ntheta; j++) {
        for (i = 0; i < nphi; i++) {
            float theta = sqrtf(randf(rng));
            float phi   = 2.0f * M_PI * randf(rng);

            Ray ray = { .origin = p };
            ray.origin = p;
            //float x = cosf(phi) * theta;
            //float y = sinf(phi) * theta;
            //float z = sqrtf(1.0f - theta * theta);
            // local -> global
            float u = randf(rng);
            auto sample_dir = sample_cosine_hemisphere(u, randf(rng));
            float rx = sample_dir.dir.x * basis[0].x + sample_dir.dir.y * basis[1].x + sample_dir.dir.z * basis[2].x;
            float ry = sample_dir.dir.x * basis[0].y + sample_dir.dir.y * basis[1].y + sample_dir.dir.z * basis[2].y;
            float rz = sample_dir.dir.x * basis[0].z + sample_dir.dir.y * basis[1].z + sample_dir.dir.z * basis[2].z;
            ray.dir.x = rx;
            ray.dir.y = ry;
            ray.dir.z = rz;

            //ray.dir = -sample_dir.dir;

            //return ray.dir;
            ray.origin = ray.origin + ray.dir * epsilon;
            ray.tmax = 1.0e+17f;

            //return ray.dir;

            Hit occ_hit = { .t = ray.tmax };

            int dc;
            if (!bvh.intersect(ray, occ_hit, &dc))
                occlusion += 1.0f / sample_dir.pdf;
        }
    }

    //occlusion = (ntheta * nphi - occlusion) / (float)(ntheta * nphi);

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
    color.x = sqrtf(color.x);
    color.y = sqrtf(color.y);
    color.z = sqrtf(color.z);
    color = color.zyx;
    return (((int) (color.z * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.x * 255) & 0xFF);
}

RA_FUNCTION uint32_t& access_frame_buffer(uint32_t* buffer, int x, int y, int width, int height) {
    return buffer[((height - 1 - y) * width + x)];
}

RA_FUNCTION vec3 read_film(float* film, int x, int y, int width, int height) {
    size_t film_size_per_component = (width * height);
    float fx = film[((height - 1 - y) * width + x) + film_size_per_component * 0];
    float fy = film[((height - 1 - y) * width + x) + film_size_per_component * 1];
    float fz = film[((height - 1 - y) * width + x) + film_size_per_component * 2];
    return vec3(fx, fy, fz);
}

RA_FUNCTION void write_film(float* film, int x, int y, int width, int height, vec3 value) {
    size_t film_size_per_component = (width * height);
    film[((height - 1 - y) * width + x) + film_size_per_component * 0] = value.x;
    film[((height - 1 - y) * width + x) + film_size_per_component * 1] = value.y;
    film[((height - 1 - y) * width + x) + film_size_per_component * 2] = value.z;
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
RA_RENDERER_SIGNATURE {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = cam.position;
    Ray r = { origin, normalize(camera_get_forward_vec(&cam, vec3(dx, dy, -1.0f))), 0, 99999 };

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
        bvh.intersect(r, nearest_hit, &iter);
    }

    switch (mode) {
        case PRIMARY: {
            vec3 color = vec3(0.0f, 0.5f, 1.0f);
            if (nearest_hit.t > 0.0f)
                color = vec3(0.5f) + nearest_hit.n * 0.5f;
            access_frame_buffer(fb, x, y, width, height) = pack_color(color);
            break;
        }
        case PRIMARY_HEATMAP: {
            access_frame_buffer(fb, x, y, width, height) = pack_color(vec3(log2f(iter) / 8.0f));
            break;
        }
        case AO: {
            vec3 color = vec3(0.0f, 0.5f, 1.0f);
            if (nearest_hit.t > 0.0f) {
                uint32_t rng = 0x811C9DC5;
                rng = fnv_hash(rng, accum);
                rng = fnv_hash(rng, x);
                rng = fnv_hash(rng, y);
                //unsigned int rng = (x * width + y) + accum;// ^ FNVHash(reinterpret_cast<char*>(&accum), sizeof(accum));
                color = ambient_occlusion(&rng, bvh, r, &nearest_hit);
            }
            vec3 film_data = vec3(0);
            if (accum > 0) {
                film_data = read_film(film, x, y, width, height);
                // float f = 0.01f + 1.0f / accum;
                // access_buffer(buf, x, y, width, height) = pack_color(unpack_color(access_buffer(buf, x, y, width, height)) * (1.0f - f) + color * f);
            }
            film_data = film_data + color;
            write_film(film, x, y, width, height, film_data);
            access_frame_buffer(fb, x, y, width, height) = pack_color(1.0f * film_data / (accum + 1));
        }
    }
}

}
