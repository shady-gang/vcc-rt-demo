#include "bvh.h"

RA_METHOD bool BVH::intersect(Ray ray, Hit& hit, int* iteration_count) {
    int stack[32];
    int stack_size = 0;

    vec3 inverted_ray_dir = vec3(1.0f) / ray.dir;
    vec3 morigin_t_riv = -ray.origin * inverted_ray_dir;

    auto isect = [&](BBox& box, float& distance) {
        float bbox_t[2];
        box.intersect_range(ray, inverted_ray_dir, morigin_t_riv, bbox_t);
        distance = bbox_t[0];
        return (bbox_t[0] <= bbox_t[1] && bbox_t[1] > 0 && bbox_t[0] < ray.tmax);
    };

    bool hit_something = false;
    int id = root;
    int max_iter = 256;
    int k;
    for (k = 0; k < max_iter; k++) {
        Node n = nodes[id];
        float d;
        if (true || isect(n.box, d)) {
            if (n.is_leaf) {
                for (int i = 0; i < n.leaf.count; i++) {
                    size_t iindex = n.leaf.start + i;
#ifndef BVH_REORDER_TRIS
                    size_t tindex = indices[iindex];
#else
                    size_t tindex = iindex;
#endif
                    if (tris[tindex].intersect(ray, hit)) {
                        hit_something = true;
                        ray.tmax = hit.t;
                    }
                }
            } else {
                int left_child = n.inner.children[0];
                float left_d;
                bool left_hit = isect(nodes[left_child].box, left_d);

                int right_child = n.inner.children[1];
                float right_d;
                bool right_hit = isect(nodes[right_child].box, right_d);

                if (left_hit && right_hit) {
                    int closest_child = right_d < left_d;
                    id = n.inner.children[closest_child];
                    stack[stack_size++] = n.inner.children[1 - closest_child];
                    continue;
                } else if (left_hit) {
                    id = left_child;
                    continue;
                } else if (right_hit) {
                    id = right_child;
                    continue;
                }

                /*float bbox_t[2];
                n.box.intersect_range(ray, inverted_ray_dir, bbox_t);
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
                }*/
            }
        }
        if (stack_size == 0)
            break;
        id = stack[--stack_size];
    }
    *iteration_count = k;
    return hit_something;
}