#include "bvh/v2/thread_pool.h"
#include "bvh/v2/executor.h"
#include "bvh/v2/default_builder.h"
#include "bvh/v2/node.h"

#include "bvh_host.h"
#include "bvh/v2/tri.h"

using Scalar  = float;
using Vec3    = bvh::v2::Vec<Scalar, 3>;
using BBBox    = bvh::v2::BBox<Scalar, 3>;
using BTri     = bvh::v2::Tri<Scalar, 3>;
using BNode    = bvh::v2::Node<Scalar, 3>;
using BBvh     = bvh::v2::Bvh<BNode>;
using BRay     = bvh::v2::Ray<Scalar, 3>;

static inline vec3 bvh2nasl(Vec3 v) {
    return vec3(v[0], v[1], v[2]);
}

static inline Vec3 nasl2bvh(vec3 v) {
    return Vec3(v.x, v.y, v.z);
}

using namespace shady;

static int count_tris(BVH* bvh, int i, int* maxdepth, int depth = 1) {
    if (depth > *maxdepth)
        *maxdepth = depth;
    BVH::Node* n = &bvh->nodes[i];
    if (n->is_leaf)
        return n->leaf.count;
    else
        return count_tris(bvh, n->inner.children[0], maxdepth, depth + 1) + count_tris(bvh, n->inner.children[1], maxdepth, depth + 1);
}

BVHHost::BVHHost(Model& model, Device* device) {
    bvh::v2::ThreadPool thread_pool;
    bvh::v2::ParallelExecutor executor(thread_pool);

    std::vector<BTri> input_tris;
    for (int i = 0; i < model.triangles.size(); i++) {
        auto& oldtri = model.triangles[i];
        BTri tri;
        tri.p0 = nasl2bvh(oldtri.v0);
        tri.p1 = nasl2bvh(oldtri.v1);
        tri.p2 = nasl2bvh(oldtri.v2);
        //printf("Tri: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)\n", tri.p0[0], tri.p0[1], tri.p0[2], tri.p1[0], tri.p1[1], tri.p1[2], tri.p2[0], tri.p2[1], tri.p2[2]);
        input_tris.push_back(tri);
    }

    std::vector<BBBox> bboxes(input_tris.size());
    std::vector<Vec3> centers(input_tris.size());
    executor.for_each(0, input_tris.size(), [&] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            bboxes[i]  = input_tris[i].get_bbox();
            centers[i] = input_tris[i].get_center();
        }
    });

    typename bvh::v2::DefaultBuilder<BNode>::Config config {  };
    config.quality = bvh::v2::DefaultBuilder<BNode>::Quality::High;
    config.min_leaf_size = 4;
    config.max_leaf_size = 8;
    auto bvh = bvh::v2::DefaultBuilder<BNode>::build(thread_pool, bboxes, centers, config);

    std::vector<BVH::Node> tmp_nodes;
    std::vector<int> tmp_indices;
    for (auto& old : bvh.nodes) {
        BVH::Node n = { old.is_leaf() };
        auto old_box = old.get_bbox();
        n.box = BBox(bvh2nasl(old_box.min), bvh2nasl(old_box.max));
        if (old.is_leaf()) {
            n.leaf.count = 0;
            n.leaf.start = tmp_indices.size();
            //printf("Box: (%f,%f,%f), (%f,%f,%f)\n", (float) n.box.min.x, (float) n.box.min.y, (float) n.box.min.z, (float) n.box.max.x, (float) n.box.max.y, (float) n.box.max.z);
            for (int i = 0; i < old.index.prim_count(); i++) {
                //tris.push_back(t);
                auto id = bvh.prim_ids[old.index.first_id() + i];
                tmp_indices.push_back(id);
                n.leaf.count += 1;
                Triangle tri = model.triangles[id];
                //printf("Tri: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)\n", tri.v0[0], tri.v0[1], tri.v0[2], tri.v1[0], tri.v1[1], tri.v1[2], tri.v2[0], tri.v2[1], tri.v2[2]);
            }
        } else {
            //printf("Box: (%f,%f,%f), (%f,%f,%f)\n", (float) n.inner.box.min.x, (float) n.inner.box.min.y, (float) n.inner.box.min.z, (float) n.inner.box.max.x, (float) n.inner.box.max.y, (float) n.inner.box.max.z);
            n.inner.children[0] = ( old.index.first_id());
            n.inner.children[1] = ( old.index.first_id() + 1);
            //printf("Inner[%d]: left: %d, right: %d\n", id, n.inner.children[0], n.inner.children[1]);
        }
        tmp_nodes.push_back(n);
    }

    const auto scene_bbox = bvh.get_root().get_bbox();
    scene_min = vec3(scene_bbox.min[0], scene_bbox.min[1], scene_bbox.min[2]);
    scene_max = vec3(scene_bbox.max[0], scene_bbox.max[1], scene_bbox.max[2]);
    
    // This precomputes some data to speed up traversal further.
    std::vector<Triangle> tmp_reordered_tris(model.triangles.size());
    executor.for_each(0, model.triangles.size(), [&] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            auto j = true ? tmp_indices[i] : i;
            tmp_reordered_tris[i] = model.triangles[j];        
        }
    });
    
// #ifdef BVH_REORDER_TRIS
//     // Ensure the prim ids are still correct
//     executor.for_each(0, tmp_reordered_tris.size(), [&] (size_t begin, size_t end) {
//         for (size_t i = begin; i < end; ++i) {
//             tmp_reordered_tris[i].prim_id = (int)i;        
//         }
//     });
// #endif

    nodes = std::move(tmp_nodes);
#ifdef BVH_REORDER_TRIS
    reordered_tris = std::move(tmp_reordered_tris);
#else
    indices = std::move(tmp_indices);
#endif

    auto root_index = ((&bvh.get_root() - bvh.nodes.data()) / sizeof(*bvh.nodes.data()));

    host_bvh.root = (int) root_index;
    host_bvh.nodes = nodes.data();
#ifdef BVH_REORDER_TRIS
    host_bvh.tris = reordered_tris.data();
#else
    host_bvh.indices = indices.data();
#endif

    offload(device, nodes, gpu_nodes);
#ifdef BVH_REORDER_TRIS
    offload(device, reordered_tris, gpu_reordered_tris);
#else
    offload(device, indices, gpu_indices);
#endif

    gpu_bvh = host_bvh;
    gpu_bvh.nodes = reinterpret_cast<BVH::Node*>(shd_rn_get_buffer_device_pointer(gpu_nodes));
#ifdef BVH_REORDER_TRIS
    gpu_bvh.tris = reinterpret_cast<Triangle*>(shd_rn_get_buffer_device_pointer(gpu_reordered_tris));
#else
    gpu_bvh.tris = reinterpret_cast<Triangle*>(shd_rn_get_buffer_device_pointer(model.triangles_gpu));
    gpu_bvh.indices = reinterpret_cast<int*>(shd_rn_get_buffer_device_pointer(gpu_indices));
#endif

    int maxdepth = 0;
    int c = count_tris(&host_bvh, host_bvh.root, &maxdepth);
    printf("BVH is %d nodes long and at most %d nodes deep.\n", (int) nodes.size(), maxdepth);
    assert(c == model.triangles.size());
}

BVHHost::~BVHHost() {
#ifdef BVH_REORDER_TRIS
    shd_rn_destroy_buffer(gpu_reordered_tris);
#else
    shd_rn_destroy_buffer(gpu_indices);
#endif
    shd_rn_destroy_buffer(gpu_nodes);
}