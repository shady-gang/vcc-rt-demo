#include <iostream>
#include <cassert>
#include <cmath>
#include <array>
#include <vector>

#include <cstdint>
#include <cstring>

extern "C" {
namespace shady {

#include "shady/runner/runner.h"
#include "shady/driver.h"
#include "shady/ir/module.h"

}

bool read_file(const char* filename, size_t* size, char** output);

}

#include "imr/imr.h"
#include "GLFW/glfw3.h"

#include "renderer.h"

#include "model.h"
#include "bvh_host.h"

static_assert(sizeof(Sphere) == sizeof(float) * 4);

float rng() {
    float n = (rand() / 65536.0f);
    n = n - floorf(n);
    return n;
}

Camera camera;
CameraFreelookState camera_state = {
    //.fly_speed = 0.1f,
    .fly_speed = 10.0f,
    .mouse_sensitivity = 1.0,
};
CameraInput camera_input;

#if defined(__MINGW64__) | defined(__MINGW32__)
#include <pthread.h>
static auto time() -> uint64_t {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}
#else
static auto time() -> uint64_t {
    timespec t = { 0 };
    timespec_get(&t, TIME_UTC);
    return t.tv_sec * 1000000000 + t.tv_nsec;
}
#endif

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

thread_local vec2 gl_GlobalInvocationID;
RA_RENDERER_SIGNATURE;
}

bool gpu = true;
bool use_bvh = true;
RenderMode render_mode = AO;

int max_frames = 0;
int nframe = 0, accum = 0;

int main(int argc, char** argv) {
    char* model_filename = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0) {
            max_frames = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--no-bvh") == 0) {
            use_bvh = false;
            continue;
        }
        if (strcmp(argv[i], "--speed") == 0) {
            camera_state.fly_speed = strtof(argv[++i], nullptr);
            continue;
        }
        model_filename = argv[i];
    }

    if (!model_filename) {
        printf("Usage: ./ra <model>\n");
        exit(-1);
    }

    int WIDTH = 832*2, HEIGHT = 640*2;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Example", nullptr, nullptr);
    if (!window)
        return 0;

    imr::Context context;
    imr::Swapchain swapchain(context, window);
    imr::FpsCounter fps_counter;

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (action == GLFW_PRESS && key == GLFW_KEY_T) {
            gpu = !gpu;
        }if (action == GLFW_PRESS && key == GLFW_KEY_B) {
            use_bvh = !use_bvh;
        }if (action == GLFW_PRESS && key == GLFW_KEY_H) {
            render_mode = (RenderMode) ((render_mode + 1) % (MAX_RENDER_MODE + 1));
            accum = 0;
        }
    });

    shady::RunnerConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::shd_default_compiler_config();
    compiler_config.input_cf.restructure_with_heuristics = true;
    compiler_config.dynamic_scheduling = false;
    compiler_config.per_thread_stack_size = 328;
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
    for (size_t i = 0; i < 1; i++) {
        float spread = 200;
        Sphere s = {{rng() * spread - spread / 2, rng() * spread - spread / 2, rng() * spread - spread / 2}, rng() * 5 + 2};
        cpu_spheres.emplace_back(s);
    }

    shady::Buffer* gpu_spheres = shd_rn_allocate_buffer_device(device, cpu_spheres.size() * sizeof(Sphere));
    uint64_t spheres_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_spheres);
    shd_rn_copy_to_buffer(gpu_spheres, 0, cpu_spheres.data(), cpu_spheres.size() * sizeof(Sphere));

    std::vector<BBox> cpu_boxes;
    for (size_t i = 0; i < 1; i++) {
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

    Model model(model_filename, device);
    BVHHost bvh(model, device);

    auto epoch = time();
    int frames_in_epoch = 0;
    uint64_t total_time = 0;

    //glfwSwapInterval(1);
    while ((max_frames == 0 || nframe < max_frames) && !glfwWindowShouldClose(window)) {
        using Frame = imr::Swapchain::Frame;
        swapchain.beginFrame([&](Frame& frame) {
            int nwidth = frame.width, nheight = frame.height;

            WIDTH = nwidth;
            HEIGHT = nheight;
            fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;
            if (nwidth != WIDTH || nheight != HEIGHT && nwidth * nheight > 0) {
                cpu_fb = static_cast<uint32_t*>(realloc(cpu_fb, fb_size));
                // reallocate fb
                shd_rn_destroy_buffer(gpu_fb);
                gpu_fb = shd_rn_allocate_buffer_device(device, fb_size);
                fb_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_fb);
            }

            camera_update(window, &camera_input);
            if (camera_move_freelook(&camera, &camera_input, &camera_state))
                accum = 0;

            uint64_t render_time;
            if (gpu) {
                std::vector<void*> args;
                args.push_back(&camera);
                args.push_back(&WIDTH);
                args.push_back(&HEIGHT);
                args.push_back(&fb_gpu_addr);
                int ntris = model.triangles.size();
                if (use_bvh)
                    ntris = 0;
                args.push_back(&ntris);
                uint64_t ptr = shd_rn_get_buffer_device_pointer(model.triangles_gpu);
                args.push_back(&ptr);
                args.push_back(&bvh.gpu_bvh);
                args.push_back(&frame);
                args.push_back(&accum);
                args.push_back(&render_mode);
                //BVH* gpu_bvh = bvh.gpu_bvh;

                shady::ExtraKernelOptions launch_options = {
                    .profiled_gpu_time = &render_time,
                };
                shd_rn_wait_completion(shd_rn_launch_kernel(program, device, "render_a_pixel", (WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1, args.size(), args.data(), &launch_options));
                shd_rn_copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);
            } else {
                auto then = time();
                #pragma omp parallel for
                for (int x = 0; x < WIDTH; x++) {
                    #pragma omp simd
                    for (int y = 0; y < HEIGHT; y++) {
                        gl_GlobalInvocationID.x = x;
                        gl_GlobalInvocationID.y = y;
                        int ntris = model.triangles.size();
                        if (use_bvh)
                            ntris = 0;
                        render_a_pixel(camera, WIDTH, HEIGHT, cpu_fb, ntris, model.triangles.data(), bvh.host_bvh, nframe, accum, render_mode);
                    }
                }
                auto now = time();
                render_time = now - then;
            }

            frames_in_epoch++;
            auto now = time();
            total_time += render_time;
            auto delta = now - epoch;
            if (delta > 1000000000) {
                assert(frames_in_epoch > 0);
                auto avg_time = total_time / frames_in_epoch;
                glfwSetWindowTitle(window, (std::string("Average frametime: ") + std::to_string(avg_time / 1000) + "us, over" + std::to_string(frames_in_epoch) + "frames.").c_str());
                frames_in_epoch = 0;
                total_time = 0;
                epoch = now;
            }

            VkFence fence;
            vkCreateFence(context.device, tmp((VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            }), nullptr, &fence);
            auto vk_buffer = imr::Buffer(context, fb_size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            auto buffer = &vk_buffer;

            uint8_t* mapped_buffer;
            CHECK_VK(vkMapMemory(context.device, buffer->memory, buffer->memory_offset, buffer->size, 0, (void**) &mapped_buffer), abort());
            memcpy(mapped_buffer, cpu_fb, fb_size);
            vkUnmapMemory(context.device, buffer->memory);

            frame.presentFromBuffer(vk_buffer.handle, fence, std::nullopt);
            vkWaitForFences(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(context.device, fence, nullptr);
            //frame.present(std::nullopt);
        });

        nframe++;
        accum++;

        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        glfwPollEvents();
    }
    return 0;
}
