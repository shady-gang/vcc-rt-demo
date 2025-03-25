#include "cunk/camera.h"
#include "nasl.h"

#include <shady.h>

using namespace vcc;
using namespace nasl;

static float sinf(float) __asm__("shady::prim_op::sin");
static float cosf(float) __asm__("shady::prim_op::cos");

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