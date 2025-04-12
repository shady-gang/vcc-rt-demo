#ifndef RA_TEXTURE_H_
#define RA_TEXTURE_H_

#include "ra_math.h"

struct Texture {
    unsigned char* bytes;
    int width;
    int height;
};

namespace texture {
inline RA_FUNCTION vec3 fetch_texture_unsafe(int px, int py, const Texture& tex) {
    int idx = py * tex.width + px;
    return vec3(tex.bytes[idx*4 + 0]/255.0f, tex.bytes[idx*4 + 1]/255.0f, tex.bytes[idx*4 + 2]/255.0f);
}

inline RA_FUNCTION vec3 fetch_texture(int px, int py, const Texture& tex) {
    return fetch_texture_unsafe(px % tex.width, py % tex.height, tex);
}

struct TextureTexel {
    int ix;
    int iy;
    float fx;
    float fy;
};

inline RA_FUNCTION TextureTexel map_uv_to_image_pixel(vec2 uv, const Texture& tex) {    
    // Texels are on the middle of each ~pixel~
    float u = uv.x * tex.width  - 0.5f;
    float v = uv.y * tex.height - 0.5f;
    
    return TextureTexel {
        .ix = (int)floorf(u),
        .iy = (int)floorf(v),
        .fx = fractf(u),
        .fy = fractf(v),
    };
}

inline RA_FUNCTION vec3 lookup_texture(vec2 uv, const Texture& tex) {
    const auto texel = map_uv_to_image_pixel(uv, tex);

    // Clamp border
    int x0 = clamp(texel.ix + 0, 0, tex.width  - 1);
    int y0 = clamp(texel.iy + 0, 0, tex.height - 1);
    int x1 = clamp(texel.ix + 1, 0, tex.width  - 1);
    int y1 = clamp(texel.iy + 1, 0, tex.height - 1);

    // Bilinear filtering
    vec3 p00 = fetch_texture_unsafe(x0, y0, tex);
    vec3 p10 = fetch_texture_unsafe(x1, y0, tex);
    vec3 p01 = fetch_texture_unsafe(x0, y1, tex);
    vec3 p11 = fetch_texture_unsafe(x1, y1, tex);

    return color_lerp(color_lerp(p00, p10, texel.fx), color_lerp(p01, p11, texel.fx), texel.fy);
}

inline RA_FUNCTION vec3 lookup_color_property(vec2 uv, vec3 flat_color, int tex_idx, const Texture* textures) {
    if (tex_idx < 0)
        return flat_color;
    
    return flat_color * lookup_texture(uv, textures[tex_idx]);
}
}

#endif