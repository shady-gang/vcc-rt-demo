#include <iostream>
#include <cassert>
#include <cmath>
#include <array>
#include <vector>

#include <cstdint>

extern "C" {
namespace shady {

#include "shady/runner.h"
#include "shady/driver.h"

}

bool read_file(const char* filename, size_t* size, char** output);

#include "cunk/graphics.h"
#include "cunk/camera.h"
#include "GLFW/glfw3.h"
GLFWwindow* gfx_get_glfw_handle(Window*);
}

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

void blitImage(Window* window, GfxCtx* ctx, size_t, size_t, uint32_t* image);

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 0.1f,
    .mouse_sensitivity = 1.0,
};
CameraInput camera_input;

static auto time() -> uint64_t {
    timespec t = { 0 };
    timespec_get(&t, TIME_UTC);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

int main() {
    GfxCtx* gfx_ctx;
    int WIDTH = 800, HEIGHT = 600;
    Window* window = gfx_create_window("vcc-rt", WIDTH, HEIGHT, &gfx_ctx);
    if (!window)
        return 0;

    shady::RunnerConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::shd_default_compiler_config();
    compiler_config.input_cf.restructure_with_heuristics = true;
    shady::Runner* runner = shd_rn_initialize(runtime_config);
    shady::Device* device = shd_rn_get_an_device(runner);
    assert(device);

    // auto x = shd_rn_initialize(nullptr);

    size_t size;
    char* src;
    bool ok = read_file("main_gpu.ll", &size, &src);
    assert(ok);

    shady::Module* m;
    shd_driver_load_source_file(&compiler_config, shady::SrcLLVM, size, src, "vcc_rt_demo", &m);
    shady::Program* program = shd_rn_new_program_from_module(runner, &compiler_config, m);

    int fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;
    uint32_t* cpu_fb = (uint32_t *) malloc(fb_size);

    shady::Buffer* gpu_fb = shd_rn_allocate_buffer_device(device, fb_size);
    uint64_t fb_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_fb);

    std::vector<Sphere> cpu_spheres;
    for (size_t i = 0; i < 256; i++) {
        float spread = 200;
        Sphere s = {{rng() * spread - spread / 2, rng() * spread - spread / 2, rng() * spread - spread / 2}, rng() * 5 + 2};
        cpu_spheres.emplace_back(s);
    }

    shady::Buffer* gpu_spheres = shd_rn_allocate_buffer_device(device, cpu_spheres.size() * sizeof(Sphere));
    uint64_t spheres_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_spheres);
    shd_rn_copy_to_buffer(gpu_spheres, 0, cpu_spheres.data(), cpu_spheres.size() * sizeof(Sphere));

    auto then = time();
    int frames = 0;
    uint64_t total_time = 0;

    glfwSwapInterval(1);
    do {
        int nwidth, nheight;
        glfwGetWindowSize(gfx_get_glfw_handle(window), &nwidth, &nheight);
        if (nwidth != WIDTH || nheight != HEIGHT && nwidth * nheight > 0) {
            WIDTH = nwidth;
            HEIGHT = nheight;
            fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;
            cpu_fb = static_cast<uint32_t*>(realloc(cpu_fb, fb_size));
            // reallocate fb
            shd_rn_destroy_buffer(gpu_fb);
            gpu_fb = shd_rn_allocate_buffer_device(device, fb_size);
            fb_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_fb);
        }

        std::vector<void*> args;
        args.push_back(&camera);
        args.push_back(&WIDTH);
        args.push_back(&HEIGHT);
        args.push_back(&fb_gpu_addr);
        int nspheres = cpu_spheres.size();
        args.push_back(&nspheres);
        args.push_back(&spheres_gpu_addr);

        gfx_camera_update(window, &camera_input);
        camera_move_freelook(&camera, &camera_input, &camera_state);

        uint64_t profiled_gpu_time = 0;
        shady::ExtraKernelOptions launch_options = {
            .profiled_gpu_time = &profiled_gpu_time,
        };
        shd_rn_wait_completion(shd_rn_launch_kernel(program, device, "main", (WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1, args.size(), args.data(), &launch_options));

        frames++;
        total_time += profiled_gpu_time;
        auto now = time();
        auto delta = now - then;
        if (delta > 1000000000) {
            assert(frames > 0);
            auto avg_time = total_time / frames;
            glfwSetWindowTitle(gfx_get_glfw_handle(window), (std::string("Average frametime: ") + std::to_string(avg_time / 1000) + "us, over" + std::to_string(frames) + "frames.").c_str());
            frames = 0;
            total_time = 0;
            then = now;
        }

        shd_rn_copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);

        blitImage(window, gfx_ctx, WIDTH, HEIGHT, cpu_fb);
        glfwSwapInterval(0);
        glfwSwapBuffers(gfx_get_glfw_handle(window));
        glfwPollEvents();
    } while(!glfwWindowShouldClose(gfx_get_glfw_handle(window)));
    return 0;
}
