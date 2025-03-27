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
void render_a_pixel(Camera cam, int width, int height, int32_t* buf, int ntris, Triangle* triangles, BVH bvh) {
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

    if (ntris > 0) {
        for (int i = 0; i < ntris; i++) {
            Triangle& b = (triangles)[i];
            b.intersect(r, nearest_hit);
        }
    } else {
        // BVH shizzle
        bvh.intersect(r, ray_inv_dir, nearest_hit);
    }

    if (nearest_hit.t > 0.0f)
        buf[(y * width + x)] = pack_color(nearest_hit.n);
    //buf[(y * width + x)] = pack_color(vec3f_to_vec3(forward));
}

}
