#include "renderer/bvh.h"
#include "model.h"

namespace shady {
extern "C" {

#include "shady/runner.h"

}
}

struct BVHHost {
    BVHHost(Model&, shady::Device*);

    BVH host_bvh;
    BVH gpu_bvh;
    shady::Buffer* gpu_nodes;
    shady::Buffer* gpu_indices;
};