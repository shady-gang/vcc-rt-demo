#ifndef RA_MODEL_H
#define RA_MODEL_H

#include "host.h"
#include "primitives.h"
#include "material.h"
#include "camera.h"

#include <string>

struct Model {
    Model(const char* path, shady::Device*);
    ~Model();

    // int triangles_count = 0;
    // Triangle* triangles_host;
    std::vector<Triangle> triangles;
    shady::Buffer* triangles_gpu = nullptr;

    std::vector<Material> materials;
    shady::Buffer* materials_gpu = nullptr;

    bool has_camera;
    Camera loaded_camera;
};

#endif
