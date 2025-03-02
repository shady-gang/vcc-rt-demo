#include <iostream>
#include <cassert>
#include <cmath>
#include <array>
#include <vector>

#include <cstdint>

extern "C" {
namespace shady {

#include "shady/runtime.h"
#include "shady/driver.h"

}

bool read_file(const char* filename, size_t* size, char** output);

#include "cunk/graphics.h"
#include "cunk/camera.h"
#include "GLFW/glfw3.h"
GLFWwindow* gfx_get_glfw_handle(Window*);
}


int WIDTH = 800, HEIGHT = 600;

using vec3 = std::array<float, 3>;

struct Sphere {
    vec3 center;
    float radius;
};

static_assert(sizeof(Sphere) == sizeof(float) * 4);

float rng() {
    float n = (rand() / 65536.0f);
    n = n - floorf(n);
    return n;
}

void blitImage(Window* window, GfxCtx* ctx, uint32_t* image);

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 0.1f,
    .mouse_sensitivity = 1.0,
};
CameraInput camera_input;

int main() {
    GfxCtx* gfx_ctx;
    Window* window = gfx_create_window("vcc-rt", WIDTH, HEIGHT, &gfx_ctx);
    if (!window)
        return 0;

    shady::RuntimeConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::shd_default_compiler_config();
    compiler_config.input_cf.restructure_with_heuristics = true;
    shady::Runtime* runtime = shd_rt_initialize(runtime_config);
    shady::Device* device = shd_rt_get_an_device(runtime);
    assert(device);

    // auto x = shd_rt_initialize(nullptr);

    size_t size;
    char* src;
    bool ok = read_file("main_gpu.ll", &size, &src);
    assert(ok);

    shady::Module* m;
    shd_driver_load_source_file(&compiler_config, shady::SrcLLVM, size, src, "vcc_rt_demo", &m);
    shady::Program* program = shd_rt_new_program_from_module(runtime, &compiler_config, m);

    int fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;
    uint32_t* cpu_fb = (uint32_t *) malloc(fb_size);
    for (size_t x = 0; x < WIDTH; x++) {
        for (size_t y = 0; y < HEIGHT; y++) {
            cpu_fb[(x * HEIGHT + y)] = 0;
        }
    }

    shady::Buffer* gpu_fb = shd_rt_allocate_buffer_device(device, fb_size);
    uint64_t fb_gpu_addr = shd_rt_get_buffer_device_pointer(gpu_fb);
    shd_rt_copy_to_buffer(gpu_fb, 0, cpu_fb, fb_size);

    std::vector<Sphere> cpu_spheres;
    for (size_t i = 0; i < 256; i++) {
        float spread = 200;
        Sphere s = {{rng() * spread - spread / 2, rng() * spread - spread / 2, rng() * spread - spread / 2}, rng() * 5 + 2};
        cpu_spheres.emplace_back(s);
    }

    shady::Buffer* gpu_spheres = shd_rt_allocate_buffer_device(device, cpu_spheres.size() * sizeof(Sphere));
    uint64_t spheres_gpu_addr = shd_rt_get_buffer_device_pointer(gpu_spheres);
    shd_rt_copy_to_buffer(gpu_spheres, 0, cpu_spheres.data(), cpu_spheres.size() * sizeof(Sphere));

    glfwSwapInterval(1);
    do {
        std::vector<void*> args;
        args.push_back(&camera);
        args.push_back(&WIDTH);
        args.push_back(&HEIGHT);
        args.push_back(&fb_gpu_addr);
        int nspheres = cpu_spheres.size();
        args.push_back(&nspheres);
        args.push_back(&spheres_gpu_addr);
        shady::ExtraKernelOptions launch_options {};

        gfx_camera_update(window, &camera_input);
        camera_move_freelook(&camera, &camera_input, &camera_state);

        //printf("%f %f %d\n", camera.position.x, camera.rotation.pitch, camera_input.keys.forward);
        printf("%f %f %f\n", camera.position.x, camera.position.y, camera.position.z);

        shd_rt_wait_completion(shd_rt_launch_kernel(program, device, "main", (WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1, args.size(), args.data(), &launch_options));

        shd_rt_copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);

        blitImage(window, gfx_ctx, cpu_fb);
        glfwSwapBuffers(gfx_get_glfw_handle(window));
        glfwPollEvents();
    } while(!glfwWindowShouldClose(gfx_get_glfw_handle(window)));
    return 0;
}
