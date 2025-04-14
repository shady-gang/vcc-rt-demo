#include <cstdint>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Will delinearize using sRGB gamma function
static void map_texture(unsigned char* buffer, size_t size) {
    for(size_t i = 0; i < size; ++i) {
        float v = buffer[i] / 255.0f;
        
        if (v <= 0.0031308f)
            v = 12.92f * v;
        else
            v= 1.055f * std::pow(v, 1 / 2.4f) - 0.055f;

        buffer[i] = v * 255;
    }
}

void remap_texture_for_save(unsigned char* buffer, size_t width, size_t height) {
    for(size_t i = 0; i < width * height; ++i) {
        const auto c0 = buffer[i*4 + 0];
        const auto c1 = buffer[i*4 + 1];
        const auto c2 = buffer[i*4 + 2];
        const auto c3 = buffer[i*4 + 3];

        buffer[i*4 + 0] = c2;
        buffer[i*4 + 1] = c1;
        buffer[i*4 + 2] = c0;
        buffer[i*4 + 3] = 255;
    }
}

void save_image(const char* filename, void* buffer, int width, int height, int channels) {
    assert(channels == 4);

    // map_texture((unsigned char*)buffer, width * height * channels);
    remap_texture_for_save((unsigned char*)buffer, width, height);
    stbi_write_png(filename, width, height, channels, buffer, sizeof(unsigned char)*channels*width);
}