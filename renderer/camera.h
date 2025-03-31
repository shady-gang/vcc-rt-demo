#ifndef RA_CAMERA_H
#define RA_CAMERA_H

#include "ra_math.h"

#include "nasl.h"
#include "nasl_mat.h"

using namespace nasl;

typedef struct {
    vec3 position;
    struct {
        float yaw, pitch;
    } rotation;
    float fov;
} Camera;

RA_FUNCTION vec3 camera_get_forward_vec(const Camera* cam, vec3 forward = vec3(0, 0, -1));
RA_FUNCTION vec3 camera_get_left_vec(const Camera*);
RA_FUNCTION mat4 camera_get_view_mat4(const Camera*, size_t, size_t);

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
        bool forward, back, left, right;
    } keys;
} CameraInput;

RA_FUNCTION void camera_move_freelook(Camera*, CameraInput*, CameraFreelookState*);

#endif
