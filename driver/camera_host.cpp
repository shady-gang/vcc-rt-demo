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
    input->keys.up = glfwGetKey(handle, GLFW_KEY_Q) == GLFW_PRESS;
    input->keys.down = glfwGetKey(handle, GLFW_KEY_E) == GLFW_PRESS;
    if (input->should_capture)
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool camera_move_freelook(Camera* cam, CameraInput* input, CameraFreelookState* state, float delta) {
    assert(cam && input && state);
    bool moved = false;
    if (input->mouse_held) {
        if (state->mouse_was_held) {
            double diff_x = input->mouse_x - state->last_mouse_x;
            double diff_y = input->mouse_y - state->last_mouse_y;

            camera_update_orientation_from_yaw_pitch(cam, 
                (float) diff_x / 180.0f * (float) M_PI * state->mouse_sensitivity / 50,
                (float) diff_y / 180.0f * (float) M_PI * state->mouse_sensitivity / 50);

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
    } 
    if (input->keys.back) {
        cam->position = vec3_add(cam->position, vec3_scale(cam->direction, state->fly_speed * delta));
        moved = true;
    }

    if (input->keys.right) {
        cam->position = vec3_sub(cam->position, vec3_scale(cam->right, state->fly_speed * delta));
        moved = true;
    } 
    if (input->keys.left) {
        cam->position = vec3_add(cam->position, vec3_scale(cam->right, state->fly_speed * delta));
        moved = true;
    }

    if (input->keys.down) {
        cam->position = vec3_sub(cam->position, vec3_scale(cam->up, state->fly_speed * delta));
        moved = true;
    }
    if (input->keys.up) {
        cam->position = vec3_add(cam->position, vec3_scale(cam->up, state->fly_speed * delta));
        moved = true;
    }

    return moved;
}
