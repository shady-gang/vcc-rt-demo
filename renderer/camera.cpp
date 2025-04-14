#include "camera.h"

using namespace nasl;

RA_FUNCTION mat4 camera_rotation_matrix(const Camera* camera) {
    mat4 matrix = identity_mat4;
    matrix = mul_mat4(rotate_axis_mat4(1, camera->rotation.yaw), matrix);
    matrix = mul_mat4(rotate_axis_mat4(0, camera->rotation.pitch), matrix);
    return matrix;
}

RA_FUNCTION mat4 rotate_axis_mat4f(unsigned int axis, float f) {
    mat4 m = { 0 };
    m.elems.m33 = 1;

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

RA_FUNCTION vec3 camera_get_forward_vec(const Camera* cam, vec3 forward) {
    vec4 initial_forward(forward, 1);
    // we invert the rotation matrix and use the front vector from the camera space to get the one in world space
    mat4 matrix = invert_mat4(camera_rotation_matrix(cam));
    vec4 result = mul_mat4_vec4f(matrix, initial_forward);
    return vec3_scale(result.xyz, 1.0f / result.w);
}

RA_FUNCTION vec3 camera_get_left_vec(const Camera* cam) {
    return camera_get_forward_vec(cam, vec3(-1, 0, 0));
}

RA_FUNCTION vec3 camera_get_up_vec(const Camera* cam) {
    return camera_get_forward_vec(cam, vec3(0, 1, 0));
}
