#include "host.h"
#include "bvh.h"
#include "model.h"

struct BVHHost {
    BVHHost(Model&, shady::Device*);
    ~BVHHost();

    std::vector<BVH::Node> nodes;
#ifdef BVH_REORDER_TRIS
    std::vector<Triangle> reordered_tris;
#else
    std::vector<int> indices;
#endif

    vec3 scene_min;
    vec3 scene_max;

    BVH host_bvh;
    BVH gpu_bvh;
    shady::Buffer* gpu_nodes = nullptr;
#ifdef BVH_REORDER_TRIS
    shady::Buffer* gpu_reordered_tris = nullptr;
#else
    shady::Buffer* gpu_indices = nullptr;
#endif
};