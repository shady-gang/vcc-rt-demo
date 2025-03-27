#include "bvh.h"

bool BVH::intersect(Ray ray, nasl::vec3 inverted_ray_dir, Hit& hit) {
    int stack[32];
    int stack_size = 0;

    bool hit_something = false;
    int id = root;
    while (true) {
        Node n = nodes[id];
        if (n.is_leaf) {
            for (int i = 0; i < n.leaf.count; i++) {
                if (tris[indices[n.leaf.start + i]].intersect(ray, hit)) {
                    hit_something = true;
                    ray.tmax = hit.t;
                }
            }
        } else {
            float bbox_t[2];
            n.inner.box.intersect_range(ray, inverted_ray_dir, bbox_t);
            if (bbox_t[0] <= bbox_t[1] && bbox_t[1] > 0 && bbox_t[0] < ray.tmax) {
                //for (int i = 0; i < BVH_ARITY - 1; i++) {
                //    stack[stack_size++] = &nodes[n->inner.children[i]];
                //    //if (nodes[n->inner.children[i]].is_leaf)
                //        //return bbox_hit;
                //}
                //n = &nodes[n->inner.children[BVH_ARITY - 1]];
                stack[stack_size++] = n.inner.children[0];
                id = n.inner.children[1];
                continue;
            }
        }
        if (stack_size == 0)
            break;
        id = stack[--stack_size];
    }
    return hit_something;
}