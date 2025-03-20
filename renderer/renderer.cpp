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

Mat4f invert_mat4(Mat4f m) {
    float a = m.m00 * m.m11 - m.m01 * m.m10;
    float b = m.m00 * m.m12 - m.m02 * m.m10;
    float c = m.m00 * m.m13 - m.m03 * m.m10;
    float d = m.m01 * m.m12 - m.m02 * m.m11;
    float e = m.m01 * m.m13 - m.m03 * m.m11;
    float f = m.m02 * m.m13 - m.m03 * m.m12;
    float g = m.m20 * m.m31 - m.m21 * m.m30;
    float h = m.m20 * m.m32 - m.m22 * m.m30;
    float i = m.m20 * m.m33 - m.m23 * m.m30;
    float j = m.m21 * m.m32 - m.m22 * m.m31;
    float k = m.m21 * m.m33 - m.m23 * m.m31;
    float l = m.m22 * m.m33 - m.m23 * m.m32;
    float det = a * l - b * k + c * j + d * i - e * h + f * g;
    det = 1.0f / det;
    Mat4f r;
    r.m00 = ( m.m11 * l - m.m12 * k + m.m13 * j) * det;
    r.m01 = (-m.m01 * l + m.m02 * k - m.m03 * j) * det;
    r.m02 = ( m.m31 * f - m.m32 * e + m.m33 * d) * det;
    r.m03 = (-m.m21 * f + m.m22 * e - m.m23 * d) * det;
    r.m10 = (-m.m10 * l + m.m12 * i - m.m13 * h) * det;
    r.m11 = ( m.m00 * l - m.m02 * i + m.m03 * h) * det;
    r.m12 = (-m.m30 * f + m.m32 * c - m.m33 * b) * det;
    r.m13 = ( m.m20 * f - m.m22 * c + m.m23 * b) * det;
    r.m20 = ( m.m10 * k - m.m11 * i + m.m13 * g) * det;
    r.m21 = (-m.m00 * k + m.m01 * i - m.m03 * g) * det;
    r.m22 = ( m.m30 * e - m.m31 * c + m.m33 * a) * det;
    r.m23 = (-m.m20 * e + m.m21 * c - m.m23 * a) * det;
    r.m30 = (-m.m10 * j + m.m11 * h - m.m12 * g) * det;
    r.m31 = ( m.m00 * j - m.m01 * h + m.m02 * g) * det;
    r.m32 = (-m.m30 * d + m.m31 * b - m.m32 * a) * det;
    r.m33 = ( m.m20 * d - m.m21 * b + m.m22 * a) * det;
    return r;
}

Mat4f mul_mat4f(Mat4f l, Mat4f r) {
    Mat4f dst = { 0 };
#define a(i, j) m##i##j
#define t(bc, br, i) l.a(i, br) * r.a(bc, i)
#define e(bc, br) dst.a(bc, br) = t(bc, br, 0) + t(bc, br, 1) + t(bc, br, 2) + t(bc, br, 3);
#define row(c) e(c, 0) e(c, 1) e(c, 2) e(c, 3)
#define genmul() row(0) row(1) row(2) row(3)
    genmul()
    return dst;
#undef a
#undef t
#undef e
#undef row
#undef genmul
}

Vec4f mul_mat4f_vec4f(Mat4f l, Vec4f r) {
    Vec4f dst = { 0 };
#define a(i, j) m##i##j
#define t(bc, br, i) l.a(i, br) * r.arr[i]
#define e(bc, br) dst.arr[br] = t(bc, br, 0) + t(bc, br, 1) + t(bc, br, 2) + t(bc, br, 3);
#define row(c) e(c, 0) e(c, 1) e(c, 2) e(c, 3)
#define genmul() row(0)
    genmul()
    return dst;
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
