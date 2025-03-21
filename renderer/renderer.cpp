#include <shady.h>
using namespace vcc;
#include <stdint.h>

#include "cunk/camera.h"

Vec3f camera_get_forward_vec(const Camera* cam, vec3 forward);

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct Hit {
    float t;
    vec3 p, n;
};

struct BBox {
    vec3 min, max;
};

struct Sphere {
    vec3 center;
    float radius;
};

float dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

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

static_assert(sizeof(Sphere) == sizeof(float) * 4);

int32_t pack_color(vec3 color) {
    return (((int) (color.z * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.x * 255) & 0xFF);
}

extern "C" {

vec3 vec3f_to_vec3(Vec3f v) {
    return vec3(v.x, v.y, v.z);
}

compute_shader local_size(16, 16, 1)
void main(Camera cam, int width, int height, int32_t* buf, int nspheres, Sphere* spheres) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = vec3f_to_vec3(cam.position);
    //Ray r = { origin, normalize(vec3(dx, dy, -1.0f)) };
    Ray r = { origin, normalize(vec3f_to_vec3(camera_get_forward_vec(&cam, vec3(dx, dy, -1.0f)))) };
    /*forward.x = 1.0f;
    forward.y = 0.0f;
    forward.z = 0.0f;
    Ray r = { origin, normalize(vec3(0.0f, dx, dy) + vec3f_to_vec3(forward)) };*/
    //Ray r = { origin, normalize(vec3f_to_vec3(forward)) };
    buf[(y * width + x)] = pack_color(vec3(0.0f, 0.5f, 1.0f));

    Hit nearest_hit = { -1 };
    for (int i = 0; i < nspheres; i++) {
        Sphere s = ((Sphere*)spheres)[i];
        Hit hit = intersect(r, s);
        if (hit.t > 0.0f && (hit.t < nearest_hit.t || nearest_hit.t == -1))
            nearest_hit = hit;
    }

    if (nearest_hit.t > 0.0f)
        buf[(y * width + x)] = pack_color(nearest_hit.n);
    //buf[(y * width + x)] = pack_color(vec3f_to_vec3(forward));
}

}
