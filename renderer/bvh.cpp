#include "bvh.h"

Hit BVH::intersect(Ray ray, nasl::vec3 inverted_ray_dir) {
    Node* stack[32];
    int stack_size = 0;

    Hit best_hit = { .t = -1 };
    Node* n = &nodes[root];
    while (true) {
        if (n->is_leaf) {
            for (int i = 0; i < n->leaf.count; i++) {
                Hit hit = tris[indices[n->leaf.start + i]].intersect(ray);
                if (hit.t > 0.0f && (best_hit.t < 0 || hit.t < best_hit.t))
                    best_hit = hit;
            }
        } else {
            Hit bbox_hit = n->inner.box.intersect(ray, inverted_ray_dir);
            if ((bbox_hit.t > 0 || n->inner.box.contains(ray.origin)) && (bbox_hit.t < best_hit.t || best_hit.t < 0)) {
                for (int i = 0; i < BVH_ARITY - 1; i++) {
                    stack[stack_size++] = &nodes[n->inner.children[i]];
                    //if (nodes[n->inner.children[i]].is_leaf)
                        //return bbox_hit;
                }
                n = &nodes[n->inner.children[BVH_ARITY - 1]];
                continue;
            }
        }
        if (stack_size == 0)
            break;
        n = stack[--stack_size];
    }
    return best_hit;
}