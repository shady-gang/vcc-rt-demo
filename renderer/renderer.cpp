#include "renderer.h"
#include "colormap.h"
#include "rendercontext.h"
#include "ao.h"
#include "pt.h"

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

    RNGState rng = 0x811C9DC5;
    rng = fnv_hash(rng, accum);
    rng = fnv_hash(rng, x);
    rng = fnv_hash(rng, y);

    const auto camera_scale = camera_scale_from_hfov(cam.fov, width/(float)height);
    float dx = ((x + randf(&rng)) / (float) width) * 2.0f - 1;
    float dy = ((y + randf(&rng)) / (float) height) * 2.0f - 1;
    vec3 origin = cam.position;

    Ray r = { origin, normalize(camera_get_forward_vec(&cam, vec3(camera_scale[0]*dx, camera_scale[1]*dy, -1.0f))), 0, 99999 };

    switch (mode) {
        case FACENORMAL: {
            vec3 color = vec3(0.0f, 0.5f, 1.0f);

            //Hit nearest_hit = { .t = r.tmin, .prim_id = -1 };
            Hit nearest_hit { };
            nearest_hit.t = r.tmin;
            nearest_hit.prim_id = -1;
            int iter;
            bvh.intersect(r, nearest_hit, &iter);

            if (nearest_hit.t > 0.0f && nearest_hit.prim_id >= 0) {
                Triangle tri = bvh.tris[nearest_hit.prim_id];
                color = color_normal(tri.get_face_normal());
            }
            access_frame_buffer(fb, x, y, width, height) = pack_color(color);
            break;
        }
        case VERTEXNORMAL: {
            vec3 color = vec3(0.0f, 0.5f, 1.0f);

            Hit nearest_hit { };
            nearest_hit.t = r.tmin;
            nearest_hit.prim_id = -1;
            int iter;
            bvh.intersect(r, nearest_hit, &iter);

            if (nearest_hit.t > 0.0f && nearest_hit.prim_id >= 0) {
                Triangle tri = bvh.tris[nearest_hit.prim_id];
                color = color_normal(tri.get_vertex_normal(nearest_hit.primary));
            }
            access_frame_buffer(fb, x, y, width, height) = pack_color(color);
            break;
        }
        case TEXCOORDS: {
            vec3 color = vec3(0.0f, 0.0f, 0.0f);

            //Hit nearest_hit = { .t = r.tmin, .prim_id = -1 };
            Hit nearest_hit { };
            nearest_hit.t = r.tmin;
            nearest_hit.prim_id = -1;
            int iter;
            bvh.intersect(r, nearest_hit, &iter);

            if (nearest_hit.t > 0.0f && nearest_hit.prim_id >= 0) {
                Triangle tri = bvh.tris[nearest_hit.prim_id];
                color.xy = tri.get_texcoords(nearest_hit.primary);
            }
            access_frame_buffer(fb, x, y, width, height) = pack_color(color);
            break;
        }
        case PRIM_IDS: {
            vec3 color = vec3(0.0f, 0.0f, 0.0f);

            //Hit nearest_hit = { .t = r.tmin, .prim_id = -1 };
            Hit nearest_hit { };
            nearest_hit.t = r.tmin;
            nearest_hit.prim_id = -1;
            int iter;
            bvh.intersect(r, nearest_hit, &iter);

            if (nearest_hit.t > 0.0f && nearest_hit.prim_id >= 0) {
                color = color_palette(nearest_hit.prim_id);
            }
            access_frame_buffer(fb, x, y, width, height) = pack_color(color);
            break;
        }
        case PRIMARY_HEATMAP: {
            Hit nearest_hit = { r.tmin };
            int iter;
            bvh.intersect(r, nearest_hit, &iter);
            access_frame_buffer(fb, x, y, width, height) = pack_color(vec3(log2f(iter) / 8.0f));
            break;
        }
        case AO: {
            vec3 color = pathtrace_ao(&rng, bvh, triangles, r);

            vec3 film_data = vec3(0);
            if (accum > 0) {
                film_data = read_film(film, x, y, width, height);
            }
            film_data = film_data + color;
            write_film(film, x, y, width, height, film_data);
            access_frame_buffer(fb, x, y, width, height) = pack_color(1.0f * film_data / (accum + 1));
            break;
        }
        case PT:
        case PT_NEE: {
            RenderContext ctx {
                .primitives = triangles,
                .materials = materials,
                .num_lights = nlights, // Note: there is always an evironment map (but maybe black though)
                .emitters = emitters,
                .bvh = &bvh,

                .max_depth = max_depth,
                .enable_nee = (mode == PT_NEE) && nlights > 1
            };

            vec3 color = clamp(pathtrace(&rng, r, 0, vec3(1.0f), 1.0f, ctx), vec3(0.0), vec3(9999.0f));

            vec3 film_data = vec3(0);
            if (accum > 0) {
                film_data = read_film(film, x, y, width, height);
            }
            film_data = film_data + color;
            write_film(film, x, y, width, height, film_data);
            access_frame_buffer(fb, x, y, width, height) = pack_color(1.0f * film_data / (accum + 1));
            break;
        }
    }
}

}
