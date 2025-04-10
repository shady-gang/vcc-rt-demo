#include <iostream>
#include <cassert>
#include <cmath>
#include <array>
#include <vector>

#include <cstdint>
#include <cstring>

#include "imr/imr.h"
#include "GLFW/glfw3.h"

extern "C" {
namespace shady {

#include "shady/runner/runner.h"
#include "shady/runner/vulkan.h"
#include "shady/driver.h"
#include "shady/ir/module.h"

}

bool read_file(const char* filename, size_t* size, char** output);

}

#include "renderer.h"

#include "model.h"
#include "bvh_host.h"

// static_assert(sizeof(Sphere) == sizeof(float) * 4);

float rng() {
    float n = (rand() / 65536.0f);
    n = n - floorf(n);
    return n;
}

Camera camera;
CameraFreelookState camera_state = {
    .fly_speed = 1.0f,
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
bool cuda = false;
bool use_bvh = true;
RenderMode render_mode = DEFAULT_RENDER_MODE;

int max_frames = 0;
int nframe = 0, accum = 0;

template<typename T>
auto walk_pNext_chain(VkBaseInStructure* s, T t) {
    if (!s)
        return;
    if (s->pNext)
        walk_pNext_chain<T>((VkBaseInStructure*) s->pNext, t);
    t(s);
};

int main(int argc, char** argv) {
    char* model_filename = nullptr;

    int WIDTH = 832*2, HEIGHT = 640*2;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0) {
            max_frames = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--no-bvh") == 0) {
            use_bvh = false;
            continue;
        }
        if (strcmp(argv[i], "--cpu") == 0) {
            gpu = false;
            continue;
        }
        if (strcmp(argv[i], "--cuda") == 0) {
            cuda = true;
            continue;
        }
        if (strcmp(argv[i], "--size") == 0) {
            WIDTH = strtol(argv[++i], nullptr, 10);
            HEIGHT = strtol(argv[++i], nullptr, 10);
            continue;
        }
        model_filename = argv[i];
    }

    if (!model_filename) {
        printf("Usage: ./ra <model>\n");
        exit(-1);
    }

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Example", nullptr, nullptr);
    if (!window)
        return 0;

    vkb::SystemInfo system_info = vkb::SystemInfo::get_system_info().value();
    imr::Context context([&](vkb::InstanceBuilder& b) {
#define E(req, name) \
        if (req || system_info.is_extension_available("VK_"#name)) { \
            b.enable_extension("VK_"#name);         \
        }
        SHADY_SUPPORTED_INSTANCE_EXTENSIONS(E)
#undef E
    });

    shady::ShadyVkrPhysicalDeviceCaps caps;
    std::optional<vkb::PhysicalDevice> selected_physical_device = std::nullopt;
    for (auto& physical_device : context.available_devices()) {
        if (shady::shd_rt_check_physical_device_suitability(physical_device, &caps)) {
            selected_physical_device = physical_device;
            break;
        }
    }
    if (!selected_physical_device) {
        fprintf(stderr, "Failed to pick a suitable physical device.");
        exit(-1);
    }

    for (size_t i = 0; i < caps.device_extensions_count; i++) {
        selected_physical_device->enable_extension_if_present(caps.device_extensions[i]);
        selected_physical_device->enable_features_if_present(caps.features.base.features);
        walk_pNext_chain((VkBaseInStructure*) caps.features.base.pNext, [&](VkBaseInStructure* s) {
            selected_physical_device->enable_extension_features_if_present(s);
        });
    }

    imr::Device imr_device(context, *selected_physical_device);
    imr::Swapchain swapchain(imr_device, window);
    imr::FpsCounter fps_counter;

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        const bool shiftPressed = (mods & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT;
        if (action == GLFW_PRESS && key == GLFW_KEY_T) {
            gpu = !gpu;
            accum = 0;
        } if (action == GLFW_PRESS && key == GLFW_KEY_B) {
            use_bvh = !use_bvh;
        } if (action == GLFW_PRESS && key == GLFW_KEY_H) {
            if (shiftPressed) {
                if (render_mode == (RenderMode)0)
                    render_mode = MAX_RENDER_MODE;
                else
                    render_mode = (RenderMode) (render_mode - 1);
            } else {
                render_mode = (RenderMode) ((render_mode + 1) % (MAX_RENDER_MODE + 1));
            }
            accum = 0;
        } if (action == GLFW_PRESS && key == GLFW_KEY_F4) {
            printf("--position %f %f %f --rotation %f %f\n", (float) camera.position.x, (float) camera.position.y, (float) camera.position.z, (float) camera.rotation.yaw, (float) camera.rotation.pitch);
        }
    });

    shady::RunnerConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::shd_default_compiler_config();
    compiler_config.input_cf.restructure_with_heuristics = true;
    compiler_config.dynamic_scheduling = true;
    //compiler_config.per_thread_stack_size = 512;
    //compiler_config.per_thread_stack_size = 1024;
    //compiler_config.per_thread_stack_size = 1564;
    compiler_config.per_thread_stack_size = 2048;
    shady::shd_rn_provide_vkinstance(context.instance);
    shady::Runner* runner = shd_rn_initialize(runtime_config);
    shady::Device* device = nullptr;
    if (cuda) {
        for (size_t i = 0; i < shd_rn_device_count(runner); i++) {
            shady::Device* candidate = shd_rn_get_device(runner, i);
            if (shd_rn_get_device_backend(candidate) == shady::CUDARuntimeBackend) {
                device = candidate;
                break;
            }
        }
        if (!device) {
            fprintf(stderr, "Failed to find a CUDA shady device.\n");
            exit(-1);
        }
    } else {
        device = shady::shd_rn_open_vkdevice(runner, imr_device.physical_device, imr_device.device);
    }
    assert(device);

    shady::TargetConfig target_config = shd_rn_get_device_target_config(device);

    std::string files = xstr(RENDERER_LL_FILES);
    shady::Module* mod = nullptr;
    for (auto& file : split(files, ":")) {
        size_t size;
        char* src;
        bool ok = read_file(file.c_str(), &size, &src);
        assert(ok);

        shady::Module* m;
        shd_driver_load_source_file(&compiler_config, &target_config, shady::SrcLLVM, size, src, "ra", &m);
        if (mod == nullptr)
            mod = m;
        else {
            shady::shd_module_link(mod, m);
            shady::shd_destroy_module(m);
        }
    }
    shady::Program* program = shd_rn_new_program_from_module(runner, &compiler_config, mod);

    uint32_t* cpu_fb = nullptr;
    float* cpu_film = nullptr;
    shady::Buffer* gpu_fb = nullptr;
    shady::Buffer* gpu_film = nullptr;
    uint64_t fb_gpu_addr, film_gpu_addr;

    Model model(model_filename, device);
    BVHHost bvh(model, device);

    // Setup camera
    if (model.has_camera)
        camera = model.loaded_camera;
    const auto scene_diameter = bvh.scene_max - bvh.scene_min;
    camera_state.fly_speed = fmaxf(1e-4f, fmaxf(scene_diameter[0], fmaxf(scene_diameter[1], scene_diameter[2])));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--speed") == 0) {
            camera_state.fly_speed = strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--position") == 0) {
            camera.position.x = strtof(argv[++i], nullptr);
            camera.position.y = strtof(argv[++i], nullptr);
            camera.position.z = strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--rotation") == 0) {
            camera.rotation.yaw = strtof(argv[++i], nullptr);
            camera.rotation.pitch = strtof(argv[++i], nullptr);
            continue;
        }
    }

    auto epoch = time();
    auto prev_frame = epoch;
    int frames_in_epoch = 0;
    uint64_t total_time = 0;

    // if we're not using the presenting Vulkan device to render, we need to upload the frame to it.
    std::unique_ptr<imr::Buffer> fallback_buffer;

    float delta = 0;

    //glfwSwapInterval(1);
    while ((max_frames == 0 || nframe < max_frames) && !glfwWindowShouldClose(window)) {
        using Frame = imr::Swapchain::Frame;
        swapchain.beginFrame([&](Frame& frame) {
            int nwidth = frame.width, nheight = frame.height;

            int fb_size = sizeof(uint32_t) * nwidth * nheight;
            int film_size = sizeof(float) * nwidth * nheight * 3;
            if (!cpu_fb || (nwidth != WIDTH || nheight != HEIGHT) && nwidth * nheight > 0) {
                WIDTH = nwidth;
                HEIGHT = nheight;
                free(cpu_fb);
                free(cpu_film);
                cpu_fb = static_cast<uint32_t*>(malloc(fb_size));
                cpu_film = (float*) malloc(film_size);
                // reallocate fb
                if (gpu_fb)
                    shd_rn_destroy_buffer(gpu_fb);
                gpu_fb = shd_rn_allocate_buffer_device(device, fb_size);
                fb_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_fb);

                if (gpu_film)
                    shd_rn_destroy_buffer(gpu_film);
                gpu_film = shd_rn_allocate_buffer_device(device, film_size);
                film_gpu_addr = shd_rn_get_buffer_device_pointer(gpu_film);
                accum = 0;

                fallback_buffer.reset();
            }

            camera_update(window, &camera_input);
            if (camera_move_freelook(&camera, &camera_input, &camera_state, delta))
                accum = 0;

            uint64_t render_time;
            if (gpu) {
                std::vector<void*> args;
                args.push_back(&camera);
                args.push_back(&WIDTH);
                args.push_back(&HEIGHT);
                args.push_back(&fb_gpu_addr);
                args.push_back(&film_gpu_addr);
                int ntris = model.triangles.size();
                if (use_bvh)
                    ntris = 0;
                args.push_back(&ntris);
                uint64_t ptr_tris = shd_rn_get_buffer_device_pointer(model.triangles_gpu);
                args.push_back(&ptr_tris);
                int nmats = model.materials.size();
                args.push_back(&nmats);
                uint64_t ptr_mats = shd_rn_get_buffer_device_pointer(model.materials_gpu);
                args.push_back(&ptr_mats);
                args.push_back(&bvh.gpu_bvh);
                args.push_back(&frame);
                args.push_back(&accum);
                args.push_back(&render_mode);
                //BVH* gpu_bvh = bvh.gpu_bvh;

                shady::ExtraKernelOptions launch_options = {
                    .profiled_gpu_time = &render_time,
                };
                shd_rn_wait_completion(shd_rn_launch_kernel(program, device, "render_a_pixel", (WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1, args.size(), args.data(), &launch_options));
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
                        int nmats = model.materials.size();
                        render_a_pixel(camera, WIDTH, HEIGHT, cpu_fb, cpu_film, ntris, model.triangles.data(), nmats, model.materials.data(), bvh.host_bvh, nframe, accum, render_mode);
                    }
                }
                auto now = time();
                render_time = now - then;
            }

            frames_in_epoch++;
            auto now = time();
            total_time += render_time;
            auto delta_ns = now - epoch;
            if (delta_ns > 1000000000) {
                assert(frames_in_epoch > 0);
                auto avg_time = total_time / frames_in_epoch;
                glfwSetWindowTitle(window, (std::string("Average frametime: ") + std::to_string(avg_time / 1000) + "us, over" + std::to_string(frames_in_epoch) + "frames.").c_str());
                frames_in_epoch = 0;
                total_time = 0;
                epoch = now;
            }
            delta = (float) ((now - prev_frame) / 1000000) / 1000.0f;
            prev_frame = now;

            VkFence fence;
            vkCreateFence(imr_device.device, tmp((VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            }), nullptr, &fence);
            if (!cuda && gpu) {
                frame.presentFromBuffer(shady::shd_rn_get_vkbuffer(gpu_fb), fence, std::nullopt);
            } else {
                if (gpu)
                    shd_rn_copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);
                if (!fallback_buffer)
                    fallback_buffer = std::make_unique<imr::Buffer>(imr_device, fb_size, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
                uint8_t* mapped_buffer;
                CHECK_VK(vkMapMemory(imr_device.device, fallback_buffer->memory, fallback_buffer->memory_offset, fallback_buffer->size, 0, (void**) &mapped_buffer), abort());
                memcpy(mapped_buffer, cpu_fb, fb_size);
                vkUnmapMemory(imr_device.device, fallback_buffer->memory);
                frame.presentFromBuffer(fallback_buffer->handle, fence, std::nullopt);
            }
            vkWaitForFences(imr_device.device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(imr_device.device, fence, nullptr);
        });

        nframe++;
        accum++;

        fps_counter.tick();
        fps_counter.updateGlfwWindowTitle(window);
        glfwPollEvents();
    }

    shady::shd_rn_destroy_buffer(gpu_fb);
    shady::shd_rn_shutdown(runner);

    return 0;
}
