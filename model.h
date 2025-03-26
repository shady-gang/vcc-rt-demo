#ifndef RA_MODEL_H
#define RA_MODEL_H

#include "renderer/primitives.h"

#include <string>

#include "nasl.h"

namespace shady {
extern "C" {

#include "shady/runner.h"

}
}

struct Model {
    Model(const char* path, shady::Device*);
    ~Model();

    int triangles_count = 0;
    Triangle* triangles_host;
    shady::Buffer* triangles_gpu;
};

#endif
