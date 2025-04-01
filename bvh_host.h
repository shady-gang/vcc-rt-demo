#include "host.h"
#include "renderer/bvh.h"
#include "model.h"

struct BVHHost {
    BVHHost(Model&, shady::Device*);

    std::vector<BVH::Node> nodes;
#ifdef BVH_REORDER_TRIS
    std::vector<Triangle> reordered_tris;
#else
    std::vector<int> indices;
#endif

    BVH host_bvh;
    BVH gpu_bvh;
    shady::Buffer* gpu_nodes;
#ifdef BVH_REORDER_TRIS
    shady::Buffer* gpu_reordered_tris;
#else
    shady::Buffer* gpu_indices;
#endif
};