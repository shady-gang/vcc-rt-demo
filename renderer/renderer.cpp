// #include <shady.h>
// using namespace vcc;
#include "ra_math.h"
using namespace nasl;

#include <stdint.h>

#include "camera.h"
#include "primitives.h"
#include "bvh.h"

static_assert(sizeof(Sphere) == sizeof(float) * 4);

int32_t pack_color(vec3 color) {
    return (((int) (color.z * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.x * 255) & 0xFF);
}

extern "C" {

#ifdef __SHADY__
#include "shady.h"
using namespace vcc;
#else
thread_local extern vec2 gl_GlobalInvocationID;
#endif

#ifdef __SHADY__
compute_shader local_size(16, 16, 1)
#endif
//[[gnu::flatten]]
void render_a_pixel(Camera cam, int width, int height, int32_t* buf, int nspheres, Sphere* spheres, int nboxes, BBox* boxes, int ntris, Triangle* triangles, BVH bvh) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = cam.position;
    Ray r = { origin, normalize(camera_get_forward_vec(&cam, vec3(dx, dy, -1.0f))) };
    buf[(y * width + x)] = pack_color(vec3(0.0f, 0.5f, 1.0f));

    vec3 ray_inv_dir = vec3(1.0f) / r.dir;

    Hit nearest_hit = { -1 };
    for (int i = 0; i < nspheres; i++) {
        Sphere& s = ((Sphere*)spheres)[i];
        Hit hit = s.intersect(r);
        if (hit.t > 0.0f && (hit.t < nearest_hit.t || nearest_hit.t == -1))
            nearest_hit = hit;
    }
    for (int i = 0; i < nboxes; i++) {
        BBox& b = ((BBox*)boxes)[i];
        Hit hit = b.intersect(r, ray_inv_dir);
        if (hit.t > 0.0f && (hit.t < nearest_hit.t || nearest_hit.t == -1))
            nearest_hit = hit;
    }
    //for (int i = 0; i < ntris; i++) {
    //    Triangle& b = (triangles)[i];
    //    Hit hit = b.intersect(r);
    //    if (hit.t > 0.0f && (hit.t < nearest_hit.t || nearest_hit.t == -1))
    //        nearest_hit = hit;
    //}

    // BVH shizzle
    Hit bvh_hit = bvh.intersect(r, ray_inv_dir);
    if (bvh_hit.t > 0.0f && (bvh_hit.t < nearest_hit.t || nearest_hit.t == -1))
        nearest_hit = bvh_hit;

    if (nearest_hit.t > 0.0f)
        buf[(y * width + x)] = pack_color(nearest_hit.n);
    //buf[(y * width + x)] = pack_color(vec3f_to_vec3(forward));
}

}
