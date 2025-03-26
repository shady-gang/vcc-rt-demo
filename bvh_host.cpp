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

BVHHost::BVHHost(Model& model, Device* device) {
    bvh::v2::ThreadPool thread_pool;
    bvh::v2::ParallelExecutor executor(thread_pool);

    std::vector<BTri> btris;
    for (int i = 0; i < model.triangles_count; i++) {
        auto& oldtri = model.triangles_host[i];
        BTri tri;
        tri.p0 = nasl2bvh(oldtri.v0);
        tri.p1 = nasl2bvh(oldtri.v1);
        tri.p2 = nasl2bvh(oldtri.v2);
        //printf("Tri: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)\n", tri.p0[0], tri.p0[1], tri.p0[2], tri.p1[0], tri.p1[1], tri.p1[2], tri.p2[0], tri.p2[1], tri.p2[2]);
        btris.push_back(tri);
    }

    std::vector<BBBox> bboxes(btris.size());
    std::vector<Vec3> centers(btris.size());
    executor.for_each(0, btris.size(), [&] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            bboxes[i]  = btris[i].get_bbox();
            centers[i] = btris[i].get_center();
        }
    });

    typename bvh::v2::DefaultBuilder<BNode>::Config config {  };
    config.quality = bvh::v2::DefaultBuilder<BNode>::Quality::High;
    auto bvh = bvh::v2::DefaultBuilder<BNode>::build(thread_pool, bboxes, centers, config);

    std::vector<BVH::Node> nodes;
    std::vector<int> indices;
    int id = 0;
    for (auto& old : bvh.nodes) {
        BVH::Node n = { old.is_leaf() };
        if (old.is_leaf()) {
            n.leaf.count = 0;
            n.leaf.start = indices.size();
            for (int i = 0; i < old.index.prim_count(); i++) {
                //tris.push_back(t);
                auto id = bvh.prim_ids[old.index.first_id() + i];
                indices.push_back(id);
                n.leaf.count += 1;
                Triangle tri = model.triangles_host[id];
                //printf("Tri: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)\n", tri.v0[0], tri.v0[1], tri.v0[2], tri.v1[0], tri.v1[1], tri.v1[2], tri.v2[0], tri.v2[1], tri.v2[2]);
            }
        } else {
            auto old_box = old.get_bbox();
            n.inner.box = BBox(bvh2nasl(old_box.min), bvh2nasl(old_box.max));
            //printf("Box: (%f,%f,%f), (%f,%f,%f)\n", (float) n.inner.box.min.x, (float) n.inner.box.min.y, (float) n.inner.box.min.z, (float) n.inner.box.max.x, (float) n.inner.box.max.y, (float) n.inner.box.max.z);
            n.inner.children[0] = ( old.index.first_id());
            n.inner.children[1] = ( old.index.first_id() + 1);
            //printf("Inner[%d]: left: %d, right: %d\n", id, n.inner.children[0], n.inner.children[1]);
        }
        nodes.push_back(n);
        id++;
    }

    host_bvh.root = ((&bvh.get_root() - bvh.nodes.data()) / sizeof(*bvh.nodes.data()));
    host_bvh.tris = model.triangles_host;
    //printf("BVH: %d nodes, root=%d\n", nodes.size(), host_bvh.root);
    host_bvh.nodes = (BVH::Node*) malloc(nodes.size() * sizeof(BVH::Node));
    memcpy(host_bvh.nodes, nodes.data(), nodes.size() * sizeof(BVH::Node));
    gpu_nodes = shd_rn_allocate_buffer_device(device, nodes.size() * sizeof(BVH::Node));
    shd_rn_copy_to_buffer(gpu_nodes, 0, host_bvh.nodes, nodes.size() * sizeof(BVH::Node));
    host_bvh.indices = (int*) malloc(indices.size() * sizeof(int));
    memcpy(host_bvh.indices, indices.data(), indices.size() * sizeof(int));
    gpu_indices = shd_rn_allocate_buffer_device(device, indices.size() * sizeof(int));
    shd_rn_copy_to_buffer(gpu_indices, 0, host_bvh.indices, indices.size() * sizeof(int));
    gpu_bvh = host_bvh;
    gpu_bvh.nodes = reinterpret_cast<BVH::Node*>(shd_rn_get_buffer_device_pointer(gpu_nodes));
    gpu_bvh.indices = reinterpret_cast<int*>(shd_rn_get_buffer_device_pointer(gpu_indices));
    gpu_bvh.tris = reinterpret_cast<Triangle*>(shd_rn_get_buffer_device_pointer(model.triangles_gpu));
}