#include <shady.h>
#include <stdint.h>

using namespace vcc;

vec3 make_vec3(float x, float y, float z) {
    return (vcc::vec3) {x, y, z};
}

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct BBox {
    vec3 min, max;
};

float dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float lengthSquared(vec3 v) {
    return dot(v, v);
}

float sqrtf(float f) __asm__("shady::prim_op::sqrt");

float length(vec3 v) {
    return sqrtf(lengthSquared(v));
}

vec3 normalize(vec3 v) {
    return v / length(v);
}

extern "C" {

compute_shader local_size(1, 1, 1)
void main(int width, int height, global int32_t* buf) {
    int x = gl_GlobalInvocationID.x;
    int y = gl_GlobalInvocationID.y;
    buf[(y * width + x)] = ((x & 0xFF) << 8) | (y & 0xFF);

    float dx = (x / (float) width) * 2.0f - 1;
    float dy = (y / (float) height) * 2.0f - 1;
    vec3 origin = make_vec3(0.0f, 0.0f, 0.0f);
    Ray r = { origin, normalize(make_vec3(1.0f, dx, dy)) };
    buf[(y * width + x)] = (((int) (r.dir.y * 255) & 0xFF) << 8) | ((int) (r.dir.z * 255) & 0xFF);
}

}