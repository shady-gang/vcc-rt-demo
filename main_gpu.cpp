#include <shady.h>
#include <stdint.h>

using namespace vcc;

extern "C" {

compute_shader local_size(1, 1, 1)
void main(int width, int height, global int32_t* buf) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    buf[(y * width + x)] = ((x & 0xFF) << 8) | (y & 0xFF);
}

}