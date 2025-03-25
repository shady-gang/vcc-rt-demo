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
#include "shady/ir/module.h"

}

bool read_file(const char* filename, size_t* size, char** output);

#include "cunk/graphics.h"
#include "GLFW/glfw3.h"
GLFWwindow* gfx_get_glfw_handle(Window*);
}

#include "renderer/primitives.h"

#include "renderer/camera.h"

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

#define xstr(s) str(s)
#define str(s) #s

std::vector<std::string> split(const std::string& str, const std::string& delim) {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

void camera_update(GLFWwindow*, CameraInput* input);


extern "C" {
vec2 gl_GlobalInvocationID;
void render_a_pixel(Camera cam, int width, int height, uint32_t* buf, int nspheres, Sphere* spheres, int nboxes, BBox* boxes);
}

bool gpu = true;

int main() {
    GfxCtx* gfx_ctx;
    int WIDTH = 800, HEIGHT = 600;
    Window* window = gfx_create_window("vcc-rt", WIDTH, HEIGHT, &gfx_ctx);
    if (!window)
        return 0;

    glfwSetKeyCallback(gfx_get_glfw_handle(window), [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS && key == GLFW_KEY_T) {
            gpu = !gpu;
        }
    });

    shady::RunnerConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::shd_default_compiler_config();
    compiler_config.input_cf.restructure_with_heuristics = true;
    compiler_config.dynamic_scheduling = false;
    shady::Runner* runner = shd_rn_initialize(runtime_config);
    shady::Device* device = shd_rn_get_an_device(runner);
    assert(device);

    std::string files = xstr(RENDERER_LL_FILES);
    shady::Module* mod = nullptr;
    for (auto& file : split(files, ":")) {
        size_t size;
        char* src;
        bool ok = read_file(file.c_str(), &size, &src);
        assert(ok);

        shady::Module* m;
        shd_driver_load_source_file(&compiler_config, shady::SrcLLVM, size, src, "vcc_rt_demo", &m);
        if (mod == nullptr)
            mod = m;
        else {
            shady::shd_module_link(mod, m);
            shady::shd_destroy_module(m);
        }
    }
    shady::Program* program = shd_rn_new_program_from_module(runner, &compiler_config, mod);

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

    std::vector<BBox> cpu_boxes;
    for (size_t i = 0; i < 256; i++) {
        float spread = 200;
        vec3 min = {rng() * spread - spread / 2, rng() * spread - spread / 2, rng() * spread - spread / 2};
        BBox b;
        b.min = min;
        min[0] += 1;
        min[1] += 1;
        min[2] += 1;
        b.max = min;
        cpu_boxes.emplace_back(b);
    }

    shady::Buffer* gpu_boxes = shd_rn_allocate_buffer_device(device, cpu_boxes.size() * sizeof(BBox));
    uint64_t boxes_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_boxes);
    shd_rn_copy_to_buffer(gpu_boxes, 0, cpu_boxes.data(), cpu_boxes.size() * sizeof(BBox));

    auto epoch = time();
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

        camera_update(gfx_get_glfw_handle(window), &camera_input);
        camera_move_freelook(&camera, &camera_input, &camera_state);

        uint64_t render_time;
        if (gpu) {
            std::vector<void*> args;
            args.push_back(&camera);
            args.push_back(&WIDTH);
            args.push_back(&HEIGHT);
            args.push_back(&fb_gpu_addr);
            int nspheres = cpu_spheres.size();
            args.push_back(&nspheres);
            args.push_back(&spheres_gpu_addr);
            int nboxes = cpu_boxes.size();
            //nboxes = 0;
            args.push_back(&nboxes);
            args.push_back(&boxes_gpu_addr);

            shady::ExtraKernelOptions launch_options = {
                .profiled_gpu_time = &render_time,
            };
            shd_rn_wait_completion(shd_rn_launch_kernel(program, device, "render_a_pixel", (WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1, args.size(), args.data(), &launch_options));
            shd_rn_copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);
        } else {
            auto then = time();
            #pragma omp parallel for
            for (int x = 0; x < WIDTH; x++) {
                for (int y = 0; y < HEIGHT; y++) {
                    gl_GlobalInvocationID.x = x;
                    gl_GlobalInvocationID.y = y;
                    render_a_pixel(camera, WIDTH, HEIGHT, cpu_fb, cpu_spheres.size(), cpu_spheres.data(), cpu_boxes.size(), cpu_boxes.data());
                }
            }
            auto now = time();
            render_time = now - then;
        }

        frames++;
        auto now = time();
        total_time += render_time;
        auto delta = now - epoch;
        if (delta > 1000000000) {
            assert(frames > 0);
            auto avg_time = total_time / frames;
            glfwSetWindowTitle(gfx_get_glfw_handle(window), (std::string("Average frametime: ") + std::to_string(avg_time / 1000) + "us, over" + std::to_string(frames) + "frames.").c_str());
            frames = 0;
            total_time = 0;
            epoch = now;
        }

        blitImage(window, gfx_ctx, WIDTH, HEIGHT, cpu_fb);
        glfwSwapInterval(0);
        glfwSwapBuffers(gfx_get_glfw_handle(window));
        glfwPollEvents();
    } while(!glfwWindowShouldClose(gfx_get_glfw_handle(window)));
    return 0;
}
