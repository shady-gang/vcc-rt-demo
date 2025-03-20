extern "C" {
#include "cunk/graphics.h"
}

#include <cstdint>
#include <optional>
#include <string>

float geometryData[] = {
        3.0, -1.0,
        -1.0,  -1.0,
        -1.0,  3.0,
};

std::optional<GfxShader*> shader;
std::optional<GfxBuffer*> buffer;

static std::string test_vs = "#version 110\n"
                             "\n"
                             "attribute vec2 vertexIn;\n"
                             "varying vec2 texCoord;\n"
                             "\n"
                             "void main() {\n"
                             "    gl_Position = vec4(vertexIn, 0.0, 1.0);\n"
                             "    texCoord = vertexIn;\n"
                             "}";
static std::string test_fs = "#version 110\n"
                             "\n"
                             "#define outputColor gl_FragData[0]\n"
                             "\n"
                             "uniform sampler2D the_texture;\n"
                             "varying vec2 texCoord;\n"
                             "\n"
                             "void main() {\n"
                             "    outputColor = texture2D(the_texture, texCoord * 0.5 + vec2(0.5));\n"
                             "}";

void blitImage(Window* window, GfxCtx* ctx, size_t width, size_t height, uint32_t* image) {
    GfxTexFormat f = { .base = GFX_TCF_UNORM8, .num_components = 4 };
    auto tex = gfx_create_texture(ctx, width, height, 0, f);
    if (!shader) {
        shader = gfx_create_shader(ctx, test_vs.c_str(), test_fs.c_str());
    }
    if (!buffer) {
        buffer = gfx_create_buffer(ctx, sizeof(geometryData));
        gfx_copy_to_buffer(buffer.value(), (void*) geometryData, sizeof(geometryData));
    }

    gfx_cmd_resize_viewport(ctx, window);
    gfx_cmd_clear(ctx);
    GfxState state = {
        .wireframe = false,
    };
    gfx_cmd_set_draw_state(ctx, state);
    gfx_cmd_use_shader(ctx, *shader);

    gfx_upload_texture(tex, image);
    gfx_cmd_set_shader_extern(ctx, "the_texture", tex);
    gfx_cmd_set_vertex_input(ctx, "vertexIn", *buffer, 2, sizeof(float) * 2, 0);
    gfx_cmd_draw_arrays(ctx, 0, 3);
    gfx_destroy_texture(tex);
}