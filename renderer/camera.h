#ifndef RA_CAMERA_H
#define RA_CAMERA_H

#include "ra_math.h"

#include "nasl.h"
#include "nasl_mat.h"

using namespace nasl;

typedef struct {
    vec3 position;
    vec3 direction;
    vec3 right;
    vec3 up;
    float fov; // In radians
    float _pad[3];
} Camera;

typedef struct {
    float fly_speed, mouse_sensitivity;
    double last_mouse_x, last_mouse_y;
    unsigned mouse_was_held: 1;
} CameraFreelookState;

typedef struct {
    bool mouse_held;
    bool should_capture;
    double mouse_x, mouse_y;
    struct {
        bool forward, back, left, right, up, down;
    } keys;
} CameraInput;

RA_FUNCTION void camera_update_orientation(Camera* cam, vec3 ndir, vec3 nup);
RA_FUNCTION void camera_update_orientation_from_yaw_pitch(Camera* cam, float yaw, float pitch);

RA_FUNCTION bool camera_move_freelook(Camera*, CameraInput*, CameraFreelookState*, float);

inline RA_FUNCTION vec2 camera_scale_from_hfov(float fov, float aspect) {
    float sw = tanf(fov * 0.5f);
    float sh = sw / aspect;
    return vec2(sw, sh);
}
#endif
