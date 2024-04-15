#include <iostream>
#include <cassert>
#include <cmath>

extern "C" {
namespace shady {

#include "shady/runtime.h"
#include "shady/driver.h"

}

bool read_file(const char* filename, size_t* size, char** output);

}

#include "MiniFB.h"

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

int main() {
    struct mfb_window *window = mfb_open_ex("my display", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window)
        return 0;

    shady::RuntimeConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::default_compiler_config();
    compiler_config.hacks.restructure_everything = true;
    shady::Runtime* runtime = shady::initialize_runtime(runtime_config);
    shady::Device* device = shady::get_an_device(runtime);
    assert(device);

    size_t size;
    char* src;
    bool ok = read_file("main_gpu.ll", &size, &src);
    assert(ok);

    shady::IrArena* a = new_ir_arena(shady::default_arena_config());
    shady::Module* m;
    shady::driver_load_source_file(&compiler_config, shady::SrcLLVM, size, src, "vcc_rt_demo", &m);
    shady::Program* program = new_program_from_module(runtime, &compiler_config, m);

    int fb_size = sizeof(uint32_t) * WIDTH * HEIGHT;
    uint32_t* cpu_fb = (uint32_t *) malloc(fb_size);
    for (size_t x = 0; x < WIDTH; x++) {
        for (size_t y = 0; y < HEIGHT; y++) {
            cpu_fb[(x * HEIGHT + y)] = 0;
        }
    }

    shady::Buffer* gpu_fb = shady::allocate_buffer_device(device, fb_size);
    uint64_t fb_gpu_addr = shady::get_buffer_device_pointer(gpu_fb);
    copy_to_buffer(gpu_fb, 0, cpu_fb, fb_size);

    std::vector<Sphere> cpu_spheres;
    for (size_t i = 0; i < 256; i++) {
        float spread = 200;
        Sphere s = {{rng() * spread - spread / 2, rng() * spread - spread / 2, rng() * spread - spread / 2}, rng() * 5 + 2};
        cpu_spheres.emplace_back(s);
    }

    shady::Buffer* gpu_spheres = shady::allocate_buffer_device(device, cpu_spheres.size() * sizeof(Sphere));
    uint64_t spheres_gpu_addr = shady::get_buffer_device_pointer(gpu_spheres);
    copy_to_buffer(gpu_spheres, 0, cpu_spheres.data(), cpu_spheres.size() * sizeof(Sphere));

    do {
        int state;

        std::vector<void*> args;
        args.push_back(&WIDTH);
        args.push_back(&HEIGHT);
        args.push_back(&fb_gpu_addr);
        int nspheres = cpu_spheres.size();
        args.push_back(&nspheres);
        args.push_back(&spheres_gpu_addr);
        wait_completion(launch_kernel(program, device, "main", (WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1, args.size(), args.data()));

        copy_from_buffer(gpu_fb, 0, cpu_fb, fb_size);

        state = mfb_update_ex(window, cpu_fb, WIDTH, HEIGHT);
        if (state < 0) {
            window = NULL;
            break;
        }
    } while(mfb_wait_sync(window));
    return 0;
}
