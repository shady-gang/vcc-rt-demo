#include <iostream>
#include <cassert>

extern "C" {
namespace shady {

#include "shady/runtime.h"
#include "shady/driver.h"

}

bool read_file(const char* filename, size_t* size, char** output);

}

#include "MiniFB.h"

int WIDTH = 800, HEIGHT = 600;

int main() {
    struct mfb_window *window = mfb_open_ex("my display", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window)
        return 0;

    shady::RuntimeConfig runtime_config = {};
    runtime_config.use_validation = true;
    runtime_config.dump_spv = true;
    shady::CompilerConfig compiler_config = shady::default_compiler_config();
    shady::Runtime* runtime = shady::initialize_runtime(runtime_config);
    shady::Device* device = shady::get_an_device(runtime);
    assert(device);

    size_t size;
    char* src;
    bool ok = read_file("main_gpu.ll", &size, &src);
    assert(ok);

    shady::IrArena* a = new_ir_arena(shady::default_arena_config());
    shady::Module* m = new_module(a, "vcc_rt_demo");
    shady::driver_load_source_file(shady::SrcLLVM, size, src, m);
    shady::Program* program = new_program_from_module(runtime, &compiler_config, m);

    int buf_size = sizeof(uint32_t) * WIDTH * HEIGHT;
    uint32_t* buffer = (uint32_t *) malloc(buf_size);

    shady::Buffer* buf = shady::allocate_buffer_device(device, buf_size);
    uint64_t buf_addr = shady::get_buffer_device_pointer(buf);

    copy_to_buffer(buf, 0, buffer, buf_size);

    for (size_t x = 0; x < WIDTH; x++) {
        for (size_t y = 0; y < HEIGHT; y++) {
            buffer[(x * HEIGHT + y)] = 0;
        }
    }

    do {
        int state;

        std::vector<void*> args;
        args.push_back(&WIDTH);
        args.push_back(&HEIGHT);
        args.push_back(&buf_addr);
        wait_completion(launch_kernel(program, device, "main", WIDTH, HEIGHT, 1, args.size(), args.data()));

        copy_from_buffer(buf, 0, buffer, buf_size);

        state = mfb_update_ex(window, buffer, WIDTH, HEIGHT);
        if (state < 0) {
            window = NULL;
            break;
        }
    } while(mfb_wait_sync(window));
    return 0;
}
