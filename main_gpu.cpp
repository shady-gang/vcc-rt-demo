#include <shady.h>
#include <stdint.h>

using namespace vcc;

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

float lengthSquared(vec3 v) {
    return dot(v, v);
}

float sqrtf(float f) __asm__("shady::prim_op::sqrt");

float length(vec3 v) {
    return sqrtf(lengthSquared(v));
}

vec3 normalize(vec3 v) {
    return v / length(v);
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

extern "C" {

int32_t pack_color(vec3 color) {
    return (((int) (color.x * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.z * 255) & 0xFF);
}

compute_shader local_size(16, 16, 1)
void main(int width, int height, global int32_t* buf, int nspheres, global Sphere* spheres) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;
    buf[(y * width + x)] = ((x & 0xFF) << 8) | (y & 0xFF);

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = vec3(0.0f, 0.0f, 0.0f);
    Ray r = { origin, normalize(vec3(1.0f, dx, dy)) };
    buf[(y * width + x)] = pack_color(r.dir);

    Hit nearest_hit = { -1.0f };
    for (int i = 0; i < nspheres; i++) {
        Sphere s = ((Sphere*)spheres)[i];
        Hit hit = intersect(r, s);
        if (hit.t > nearest_hit.t && hit.t > 0.0f)
            nearest_hit = hit;
    }

    if (nearest_hit.t > 0.0f)
        buf[(y * width + x)] = pack_color(nearest_hit.n);
}

}