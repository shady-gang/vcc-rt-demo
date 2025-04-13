#include "camera.h"
#include "GLFW/glfw3.h"

#include <cassert>

void camera_update(GLFWwindow* handle, CameraInput* input) {
    input->mouse_held = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    glfwGetCursorPos(handle, &input->mouse_x, &input->mouse_y);
    input->keys.forward = glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS;
    input->keys.back = glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS;
    input->keys.left = glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS;
    input->keys.right = glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS;
    if (input->should_capture)
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

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

bool camera_move_freelook(Camera* cam, CameraInput* input, CameraFreelookState* state, float delta) {
    assert(cam && input && state);
    bool moved = false;
    if (input->mouse_held) {
        if (state->mouse_was_held) {
            double diff_x = input->mouse_x - state->last_mouse_x;
            double diff_y = input->mouse_y - state->last_mouse_y;

            mat3 yu = rotate_around_axis((float) diff_x / 180.0f * (float) M_PI * state->mouse_sensitivity / 100, cam->up);
            mat3 pr = rotate_around_axis(-(float) diff_y / 180.0f * (float) M_PI * state->mouse_sensitivity / 100, cam->right);

            mat3 rot = mul_mat3(yu, pr);
            camera_update_orientation(cam, mul_mat3_vec3(rot, cam->direction), mul_mat3_vec3(rot, cam->up));

            moved = true;
        } else
            input->should_capture = true;

        state->last_mouse_x = input->mouse_x;
        state->last_mouse_y = input->mouse_y;
    } else
        input->should_capture = false;
    state->mouse_was_held = input->mouse_held;

    if (input->keys.forward) {
        cam->position = vec3_sub(cam->position, vec3_scale(cam->direction, state->fly_speed * delta));
        moved = true;
    } else if (input->keys.back) {
        cam->position = vec3_add(cam->position, vec3_scale(cam->direction, state->fly_speed * delta));
        moved = true;
    }

    if (input->keys.right) {
        cam->position = vec3_add(cam->position, vec3_scale(cam->right, state->fly_speed * delta));
        moved = true;
    } else if (input->keys.left) {
        cam->position = vec3_sub(cam->position, vec3_scale(cam->right, state->fly_speed * delta));
        moved = true;
    }
    return moved;
}
