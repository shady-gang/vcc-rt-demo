#ifndef RA_MODEL_H
#define RA_MODEL_H

#include "host.h"
#include "primitives.h"
#include "material.h"
#include "emitter.h"
#include "camera.h"

#include <string>

struct Model {
    Model(const char* path, shady::Device*);
    ~Model();

    // int triangles_count = 0;
    // Triangle* triangles_host;
    std::vector<Triangle> triangles;
    shady::Buffer* triangles_gpu;

    std::vector<Material> materials;
    shady::Buffer* materials_gpu;

    // Note: The first entry is the environment constant color
    std::vector<Emitter> emitters;
    shady::Buffer* emitters_gpu;

    Camera loaded_camera;
};

#endif
