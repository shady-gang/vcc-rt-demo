#ifndef RA_MODEL_H
#define RA_MODEL_H

#include "host.h"
#include "primitives.h"

#include <string>

struct Model {
    Model(const char* path, shady::Device*);
    ~Model();

    // int triangles_count = 0;
    // Triangle* triangles_host;
    std::vector<Triangle> triangles;
    shady::Buffer* triangles_gpu;
};

#endif
