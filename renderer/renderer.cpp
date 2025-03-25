// #include <shady.h>
// using namespace vcc;
#include "ra_math.h"
using namespace nasl;

#include <stdint.h>

#include "camera.h"
#include "primitives.h"

Hit intersect(Ray r, Sphere s) {
    vec3 rs = r.origin - s.center;
    float b = dot(rs, r.dir);
    float c = dot(rs, rs) - s.radius * s.radius;
    float d = b * b - c;

    if (d > 0.0f) {
        float t = -b - sqrtf(d);
        if (t > 0.0f) {
            vec3 p = r.origin + r.dir * t;
            vec3 n = normalize(p - s.center);
            return (Hit) { t, p, n };
        }
    }

    return (Hit) { -1.0f };
}

// static float min(float a, float b) { if (a < b) return a; return b; }
// static float max(float a, float b) { if (a > b) return a; return b; }

Hit intersect(Ray r, BBox bbox, vec3 ray_inv_dir) {
    float txmin = fmaf(bbox.min.x, ray_inv_dir.x, -(r.origin.x * ray_inv_dir.x));
    float txmax = fmaf(bbox.max.x, ray_inv_dir.x, -(r.origin.x * ray_inv_dir.x));
    float tymin = fmaf(bbox.min.y, ray_inv_dir.y, -(r.origin.y * ray_inv_dir.y));
    float tymax = fmaf(bbox.max.y, ray_inv_dir.y, -(r.origin.y * ray_inv_dir.y));
    float tzmin = fmaf(bbox.min.z, ray_inv_dir.z, -(r.origin.z * ray_inv_dir.z));
    float tzmax = fmaf(bbox.max.z, ray_inv_dir.z, -(r.origin.z * ray_inv_dir.z));

    auto t0x = fminf(txmin, txmax);
    auto t1x = fmaxf(txmin, txmax);
    auto t0y = fminf(tymin, tymax);
    auto t1y = fmaxf(tymin, tymax);
    auto t0z = fminf(tzmin, tzmax);
    auto t1z = fmaxf(tzmin, tzmax);

    //auto t0 = max(max(t0x, t0y), max(r.tmin, t0z));
    //auto t1 = min(min(t1x, t1y), min(r.tmax, t1z));

    enum {
        X = 0, Y, Z
    } axis = X;
    float max_t0xy = t0x;
    if (t0y > t0x) {
        axis = Y;
        max_t0xy = t0y;
    }
    auto t0 = max_t0xy;
    if (t0z > max_t0xy) {
        axis = Z;
        t0 = t0z;
    }

    //auto t0 = fmaxf(fmaxf(t0x, t0y), t0z);
    auto t1 = fminf(fminf(t1x, t1y), t1z);

    if (t0 < t1) {
        vec3 p = r.origin + r.dir * t0;
        vec3 n(0.0f);
        n.arr[axis] = -sign(r.dir.arr[axis]);
        return (Hit) { t0, p, n };
    }

    return (Hit) { -1.0f };
}

static_assert(sizeof(Sphere) == sizeof(float) * 4);

int32_t pack_color(vec3 color) {
    return (((int) (color.z * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.x * 255) & 0xFF);
}

extern "C" {

#ifdef __SHADY__
#include "shady.h"
using namespace vcc;
#else
extern vec2 gl_GlobalInvocationID;
#endif

#ifdef __SHADY__
compute_shader local_size(16, 16, 1)
#endif
//[[gnu::flatten]]
void render_a_pixel(Camera cam, int width, int height, int32_t* buf, int nspheres, Sphere* spheres, int nboxes, BBox* boxes) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = cam.position;
    //Ray r = { origin, normalize(vec3(dx, dy, -1.0f)) };
    Ray r = { origin, normalize(camera_get_forward_vec(&cam, vec3(dx, dy, -1.0f))) };
    /*forward.x = 1.0f;
    forward.y = 0.0f;
    forward.z = 0.0f;
    Ray r = { origin, normalize(vec3(0.0f, dx, dy) + vec3f_to_vec3(forward)) };*/
    //Ray r = { origin, normalize(forward) };
    buf[(y * width + x)] = pack_color(vec3(0.0f, 0.5f, 1.0f));

    vec3 ray_inv_dir = vec3(1.0f) / r.dir;

    Hit nearest_hit = { -1 };
    for (int i = 0; i < nspheres; i++) {
        Sphere s = ((Sphere*)spheres)[i];
        Hit hit = intersect(r, s);
        if (hit.t > 0.0f && (hit.t < nearest_hit.t || nearest_hit.t == -1))
            nearest_hit = hit;
    }
    for (int i = 0; i < nboxes; i++) {
        BBox s = ((BBox*)boxes)[i];
        Hit hit = intersect(r, s, ray_inv_dir);
        if (hit.t > 0.0f && (hit.t < nearest_hit.t || nearest_hit.t == -1))
            nearest_hit = hit;
    }

    if (nearest_hit.t > 0.0f)
        buf[(y * width + x)] = pack_color(nearest_hit.n);
    //buf[(y * width + x)] = pack_color(vec3f_to_vec3(forward));
}

}
