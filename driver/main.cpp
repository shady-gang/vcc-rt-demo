#include <iostream>
#include <cassert>
#include <cmath>
#include <array>
#include <vector>
#include <optional>

#include <cstdint>
#include <cstring>

#include "imr/imr.h"
#include "GLFW/glfw3.h"

void save_image(const char* filename, void* buffer, int width, int height, int channels);

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

bool headless = false;
bool gpu = true;
bool cuda = false;
bool use_bvh = true;
RenderMode render_mode = DEFAULT_RENDER_MODE;

int max_frames = 0;
int nframe = 0, accum = 0;
int runs = 1;

bool screenshotRequested = false;

template<typename T>
auto walk_pNext_chain(VkBaseInStructure* s, T t) {
    if (!s)
        return;
    if (s->pNext)
        walk_pNext_chain<T>((VkBaseInStructure*) s->pNext, t);
    t(s);
};

struct CommandArguments {
    int max_depth = 5;
    std::optional<float> camera_speed;
    std::optional<vec3> camera_eye;
    std::optional<vec3> camera_dir;
    std::optional<vec3> camera_up;
    std::optional<vec2> camera_rot;
    std::optional<float> camera_fov;
};

int main(int argc, char** argv) {
    char* model_filename = nullptr;

    int WIDTH = 832*2, HEIGHT = 640*2;

    shady::CompilerConfig compiler_config = shady::shd_default_compiler_config();
    shady::shd_parse_compiler_config_args(&compiler_config, &argc, argv);

    CommandArguments cmd_args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0) {
            max_frames = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--runs") == 0) {
            runs = atoi(argv[++i]);
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
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
            continue;
        }
        if (strcmp(argv[i], "--size") == 0) {
            WIDTH = strtol(argv[++i], nullptr, 10);
            HEIGHT = strtol(argv[++i], nullptr, 10);
            continue;
        }
        if (strcmp(argv[i], "--max-depth") == 0) {
            cmd_args.max_depth = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--speed") == 0) {
            cmd_args.camera_speed= strtof(argv[++i], nullptr);
            continue;
        }
        if (strcmp(argv[i], "--position") == 0) {
            vec3 pos;
            pos.x = strtof(argv[++i], nullptr);
            pos.y = strtof(argv[++i], nullptr);
            pos.z = strtof(argv[++i], nullptr);
            cmd_args.camera_eye = pos;
            continue;
        }
        if (strcmp(argv[i], "--rotation") == 0) {
            vec2 rot;
            rot.x = strtof(argv[++i], nullptr);
            rot.y = strtof(argv[++i], nullptr);
            cmd_args.camera_rot = rot;
            continue;
        }
        if (strcmp(argv[i], "--dir") == 0) {
            vec3 dir;
            dir.x = strtof(argv[++i], nullptr);
            dir.y = strtof(argv[++i], nullptr);
            dir.z = strtof(argv[++i], nullptr);
            cmd_args.camera_dir = dir;
            continue;
        }
        if (strcmp(argv[i], "--up") == 0) {
            vec3 up;
            up.x = strtof(argv[++i], nullptr);
            up.y = strtof(argv[++i], nullptr);
            up.z = strtof(argv[++i], nullptr);
            cmd_args.camera_up = up;
            continue;
        }
        if (strcmp(argv[i], "--fov") == 0) {
            vec2 rot;
            cmd_args.camera_fov = strtof(argv[++i], nullptr);
            continue;
        }
        model_filename = argv[i];
    }

    if (!model_filename) {
        printf("Usage: ./ra <model>\n");
        exit(-1);
    }

    GLFWwindow* window = nullptr;

    if (!headless) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Example", nullptr, nullptr);
        if (!window)
            return 0;
    }

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
    }
    selected_physical_device->enable_features_if_present(caps.features.base.features);

    struct PaddedVkStruct {
        VkBaseInStructure base;
        VkBool32 padding[256];
    };

    size_t ext_features_len;
    shady::shd_rt_get_device_caps_ext_features(&caps, &ext_features_len, nullptr, nullptr);
    std::vector<VkBaseInStructure*> ext_features;
    ext_features.resize(ext_features_len);
    std::vector<size_t> ext_features_lens;
    ext_features_lens.resize(ext_features_len);
    shady::shd_rt_get_device_caps_ext_features(&caps, &ext_features_len, ext_features.data(), ext_features_lens.data());

    for (size_t i = 0; i < ext_features_len; i++) {
        auto feature = ext_features[i];
        if (feature->sType) {
            PaddedVkStruct padded = {};
            memset(&padded, 0, sizeof(padded));
            memcpy(&padded, feature, ext_features_lens[i]);
            selected_physical_device->enable_extension_features_if_present(padded);
        }
    }

    imr::Device imr_device(context, *selected_physical_device);
    imr::FpsCounter fps_counter;
    std::unique_ptr<imr::Swapchain> swapchain;

    if (!headless) {
        swapchain = std::make_unique<imr::Swapchain>(imr_device, window);
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
                printf("--position %f %f %f --dir %f %f %f --up %f %f %f --fov %f\n",
                    (float) camera.position.x, (float) camera.position.y, (float) camera.position.z,
                    (float) camera.direction.x, (float) camera.direction.y, (float) camera.direction.z,
                    (float) camera.up.x, (float) camera.up.y, (float) camera.up.z,
                    (float) camera.fov);
            }
            if (action == GLFW_PRESS && key == GLFW_KEY_MINUS) {
                camera.fov -= 0.02f;
            }
            if (action == GLFW_PRESS && key == GLFW_KEY_EQUAL) {
                camera.fov += 0.02f;
            }
            if (action == GLFW_PRESS && key == GLFW_KEY_F8) {
                screenshotRequested = true;
            }
        });
    }

    shady::RunnerConfig runtime_config = {};
    runtime_config.use_validation = false;
    runtime_config.dump_spv = true;
    // compiler_config.input_cf.restructure_with_heuristics = true;
    compiler_config.dynamic_scheduling = true;
#ifdef RA_USE_RT_PIPELINES
    compiler_config.dynamic_scheduling = false;
    compiler_config.use_rt_pipelines_for_calls = true;
#endif
#ifdef RA_USE_SCRATCH_PRIVATE
    compiler_config.lower.use_scratch_for_private = true;
#endif
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

    shady::TargetConfig target_config = shd_rn_get_device_target_config(&compiler_config, device);

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
    camera = model.loaded_camera;

    if (cmd_args.camera_speed.has_value()) {
        camera_state.fly_speed = *cmd_args.camera_speed;
    } else {
        const auto scene_diameter = bvh.scene_max - bvh.scene_min;
        camera_state.fly_speed = fmaxf(1e-4f, fmaxf(scene_diameter[0], fmaxf(scene_diameter[1], scene_diameter[2])));
    }

    if (cmd_args.camera_eye.has_value())
        camera.position = *cmd_args.camera_eye;

    if (cmd_args.camera_rot.has_value()) {
        // Simulate old behaviour 
        camera.direction = vec3(0,0,1);
        camera.up = vec3(0,1,0);
        camera.right = vec3(1,0,0);
        camera_update_orientation_from_yaw_pitch(&camera, cmd_args.camera_rot->x, cmd_args.camera_rot->y);
    }

    if (cmd_args.camera_dir.has_value())
        camera.direction = *cmd_args.camera_dir;

    if (cmd_args.camera_up.has_value())
        camera.up = *cmd_args.camera_up;

    if (cmd_args.camera_fov)
        camera.fov = *cmd_args.camera_fov;
    
    // Ensure camera has correct orientation
    camera_update_orientation(&camera, camera.direction, camera.up);
    printf("Loaded Camera: eye=(%f, %f, %f) dir=(%f, %f, %f) up=(%f, %f, %f)\n",
         (float)camera.position.x, (float)camera.position.y, (float)camera.position.z, 
         (float)camera.direction.x, (float)camera.direction.y, (float)camera.direction.z, 
         (float)camera.up.x, (float)camera.up.y, (float)camera.up.z);

    // Setup other stuff
    auto prev_frame = time();
    uint64_t total_time = 0;

    // if we're not using the presenting Vulkan device to render, we need to upload the frame to it.
    std::unique_ptr<imr::Buffer> fallback_buffer;

    float delta = 0;

    auto set_size = [&](int nwidth, int nheight) {
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
    };

    set_size(WIDTH, HEIGHT);

    auto render_frame = [&] () {
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
            uint64_t ptr_mats = shd_rn_get_buffer_device_pointer(model.materials_gpu);
            args.push_back(&ptr_mats);
            int nlights = model.emitters.size();
            args.push_back(&nlights);
            uint64_t ptr_emitters = shd_rn_get_buffer_device_pointer(model.emitters_gpu);
            args.push_back(&ptr_emitters);
            args.push_back(&bvh.gpu_bvh);
            uint64_t ptr_tex = model.textures_gpu ? shd_rn_get_buffer_device_pointer(model.textures_gpu) : 0;
            args.push_back(&ptr_tex);
            uint64_t ptr_tex_data = model.texture_data_gpu ? shd_rn_get_buffer_device_pointer(model.texture_data_gpu) : 0;
            args.push_back(&ptr_tex_data);
            args.push_back(&nframe);
            args.push_back(&accum);
            args.push_back(&render_mode);
            args.push_back(&cmd_args.max_depth);
            //BVH* gpu_bvh = bvh.gpu_bvh;

            shady::ExtraKernelOptions launch_options = {
                .profiled_gpu_time = &render_time,
            };
#ifdef RA_USE_RT_PIPELINES
            if (shd_rn_get_device_backend(device) == shady::VulkanRuntimeBackend)
                shd_rn_wait_completion(shd_vkr_launch_rays(program, device, "render_a_pixel", WIDTH, HEIGHT, 1, args.size(), args.data(), &launch_options));
            else
#endif
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
                    int nlights = model.emitters.size();
                    render_a_pixel(camera, WIDTH, HEIGHT, cpu_fb, cpu_film,
                        ntris, model.triangles.data(), model.materials.data(), nlights, model.emitters.data(),
                        bvh.host_bvh, model.textures.data(), model.texture_data.data(),
                        nframe, accum, render_mode, cmd_args.max_depth);
                }
            }
            auto now = time();
            render_time = now - then;
        }

        auto now = time();
        total_time += render_time;
        delta = (float) ((now - prev_frame) / 1000000) / 1000.0f;
        prev_frame = now;

        nframe++;
        accum++;
    };

    auto present_frame = [&](imr::Swapchain::Frame& frame) {
        int fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;
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
                fallback_buffer = std::make_unique<imr::Buffer>(imr_device, fb_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            uint8_t* mapped_buffer;
            CHECK_VK(vkMapMemory(imr_device.device, fallback_buffer->memory, fallback_buffer->memory_offset, fallback_buffer->size, 0, (void**) &mapped_buffer), abort());
            memcpy(mapped_buffer, cpu_fb, fb_size);
            vkUnmapMemory(imr_device.device, fallback_buffer->memory);
            frame.presentFromBuffer(fallback_buffer->handle, fence, std::nullopt);
        }

        vkWaitForFences(imr_device.device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(imr_device.device, fence, nullptr);
    };

    auto save_screenshot = [&]() {
        int fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;

        if (gpu)
            shd_rn_copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);
        if (!fallback_buffer)
            fallback_buffer = std::make_unique<imr::Buffer>(imr_device, fb_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uint8_t* mapped_buffer;
        CHECK_VK(vkMapMemory(imr_device.device, fallback_buffer->memory, fallback_buffer->memory_offset, fallback_buffer->size, 0, (void**) &mapped_buffer), abort());
        memcpy(mapped_buffer, cpu_fb, fb_size);
        vkUnmapMemory(imr_device.device, fallback_buffer->memory);

        save_image("screenshot.png", cpu_fb, WIDTH, HEIGHT, 4);
        printf("Screenshot saved to 'screenshot.png'\n");
    };

    for (int run = 0; run < runs; run++) {
        nframe = 0;
        total_time = 0;
        accum = 0;

        while ((max_frames == 0 || nframe < max_frames) && (!window || !glfwWindowShouldClose(window))) {
            using Frame = imr::Swapchain::Frame;
            if (headless)
                render_frame();
            else
                swapchain->beginFrame([&](Frame& frame) {
                    int nwidth = frame.width, nheight = frame.height;
                    set_size(nwidth, nheight);

                    camera_update(window, &camera_input);
                    if (camera_move_freelook(&camera, &camera_input, &camera_state, delta))
                        accum = 0;

                    render_frame();

                    if (screenshotRequested) {
                        save_screenshot();
                        screenshotRequested = false;
                    }

                    fps_counter.tick();
                    fps_counter.updateGlfwWindowTitle(window);
                    glfwPollEvents();

                    present_frame(frame);
                });
        }

        printf("Rendered %d frames in %zums\n", nframe, total_time / (1000 * 1000));

        if (headless)
            save_screenshot();
    }

    shady::shd_rn_destroy_buffer(gpu_fb);
    shady::shd_rn_shutdown(runner);

    return 0;
}
