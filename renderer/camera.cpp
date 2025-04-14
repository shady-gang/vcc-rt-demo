#include "camera.h"

using namespace nasl;

static inline mat3 rotate_around_axis(float angle, vec3 axis) {
    float s = sinf(angle);
    float c = cosf(angle);
    
    float c1 = 1 - c;
    float x  = axis.x;
    float y  = axis.y;
    float z  = axis.z;
    return mat3 {
        .elems = {
            .m00 = x * x + (1.0f - x * x) * c,
            .m01 = x * y * c1 + z * s,
            .m02 = x * z * c1 - y * s,
            .m10 = x * y * c1 - z * s,
            .m11= y * y + (1.0f - y * y) * c,
            .m12 = y * z * c1 + x * s,
            .m20 = x * z * c1 + y * s,
            .m21 = y * z * c1 - x * s,
            .m22 = z * z + (1.0f - z * z) * c
        }
    };
}

static inline vec3 mul_mat3_vec3(mat3 m, vec3 v) {
    return vec3 {
        m.elems.m00 * v.x + m.elems.m01 * v.y + m.elems.m02 * v.z,
        m.elems.m10 * v.x + m.elems.m11 * v.y + m.elems.m12 * v.z,
        m.elems.m20 * v.x + m.elems.m21 * v.y + m.elems.m22 * v.z,
    };
}

RA_FUNCTION void camera_update_orientation(Camera* cam, vec3 ndir, vec3 nup) {
    cam->direction = ndir;
    cam->right     = normalize(cross(cam->direction, nup));
    cam->up        = normalize(cross(cam->right, cam->direction));
}

RA_FUNCTION void camera_update_orientation_from_yaw_pitch(Camera* cam, float yaw, float pitch) {
    mat3 yu  = rotate_around_axis(yaw, cam->up);
    mat3 pr  = rotate_around_axis(-pitch, cam->right);
    mat3 rot = mul_mat3(yu, pr);
    camera_update_orientation(cam, mul_mat3_vec3(rot, cam->direction), mul_mat3_vec3(rot, cam->up));
}
