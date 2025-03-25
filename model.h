#ifndef RA_MODEL_H
#define RA_MODEL_H

#include <string>

#include "nasl.h"

namespace shd {
extern "C" {

#include "shady/runner.h"

}
}

struct Triangle {

};

class Model {
    Model(std::string& path, shd::Device*);
    ~Model();

    int triangles_count;
    shd::Buffer* triangles;
};

#endif
