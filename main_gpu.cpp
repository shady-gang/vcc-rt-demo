#include <shady.h>
#include <stdint.h>

#include "cunk/camera.h"

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
    return (((int) (color.x * 255) & 0xFF) << 16) | (((int) (color.y * 255) & 0xFF) << 8) | ((int) (color.z * 255) & 0xFF);
}
float sinf(float) __asm__("shady::prim_op::sin");
float cosf(float) __asm__("shady::prim_op::cos");

static Mat4f camera_rotation_matrix(const Camera* camera) {
    Mat4f matrix = identity_mat4f;
    matrix = mul_mat4f(rotate_axis_mat4f(1, camera->rotation.yaw), matrix);
    matrix = mul_mat4f(rotate_axis_mat4f(0, camera->rotation.pitch), matrix);
    return matrix;
}

Mat4f rotate_axis_mat4f(unsigned int axis, float f) {
    Mat4f m = { 0 };
    m.m33 = 1;

    unsigned int t = (axis + 2) % 3;
    unsigned int s = (axis + 1) % 3;

    m.rows[t].arr[t] =  cosf(f);
    m.rows[t].arr[s] = -sinf(f);
    m.rows[s].arr[t] =  sinf(f);
    m.rows[s].arr[s] =  cosf(f);

    // leave that unchanged
    m.rows[axis].arr[axis] = 1;

    return m;
}

Vec3f camera_get_forward_vec(const Camera* cam, vec3 forward) {
    Vec4f initial_forward = { .x = forward.x, .y = forward.y, .z = forward.z, .w = 1 };
    // we invert the rotation matrix and use the front vector from the camera space to get the one in world space
    Mat4f matrix = invert_mat4(camera_rotation_matrix(cam));
    Vec4f result = mul_mat4f_vec4f(matrix, initial_forward);
    return vec3f_scale(result.xyz, 1.0f / result.w);
}

extern "C" {

vec3 vec3f_to_vec3(Vec3f v) {
    return vec3(v.x, v.y, v.z);
}

compute_shader local_size(16, 16, 1)
void main(float px, float py, float pz, float rx, float ry, int width, int height, int32_t* buf, int nspheres, Sphere* spheres) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    if (x >= width || y >= height)
        return;
    buf[(y * width + x)] = ((x & 0xFF) << 8) | (y & 0xFF);

    Camera cam;
    cam.rotation.pitch = rx;
    cam.rotation.yaw = ry;

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = vec3(px, py, pz);
    //Ray r = { origin, normalize(vec3(dx, dy, -1.0f)) };
    Ray r = { origin, normalize(vec3f_to_vec3(camera_get_forward_vec(&cam, vec3(dx, dy, -1.0f)))) };
    /*forward.x = 1.0f;
    forward.y = 0.0f;
    forward.z = 0.0f;
    Ray r = { origin, normalize(vec3(0.0f, dx, dy) + vec3f_to_vec3(forward)) };*/
    //Ray r = { origin, normalize(vec3f_to_vec3(forward)) };
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
    //buf[(y * width + x)] = pack_color(vec3f_to_vec3(forward));
}

}