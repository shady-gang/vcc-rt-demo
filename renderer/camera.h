#include "nasl.h"

#ifdef __SHADY__
#include "shady.h"
static float M_PI = 3.14159f;
static float sinf(float) __asm__("shady::pure_op::GLSL.std.450::13::Invocation");
static float cosf(float) __asm__("shady::pure_op::GLSL.std.450::14::Invocation");
static float tanf(float) __asm__("shady::pure_op::GLSL.std.450::15::Invocation");
typedef long int size_t;
#else
#include <cmath>
#include <cstddef>
#endif

#include "nasl_mat.h"

using namespace nasl;

typedef struct {
    vec3 position;
    struct {
        float yaw, pitch;
    } rotation;
    float fov;
} Camera;

vec3 camera_get_forward_vec(const Camera* cam, vec3 forward = vec3(0, 0, -1));
vec3 camera_get_left_vec(const Camera*);
mat4 camera_get_view_mat4(const Camera*, size_t, size_t);

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

void camera_move_freelook(Camera*, CameraInput*, CameraFreelookState*);