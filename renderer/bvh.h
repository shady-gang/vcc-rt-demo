#ifndef RA_BVH
#define RA_BVH

#include "primitives.h"

#define BVH_ARITY 2

struct BVH {
    struct Node {
        bool is_leaf;
        union {
            struct {
                size_t count;
                int start;
            } leaf;
            struct {
                BBox box;
                int children[BVH_ARITY];
            } inner;
        };
    };
    int root = 0;
    Node* nodes;
    int* indices;
    Triangle* tris;

    bool intersect(Ray ray, vec3 inverted_ray_dir, Hit& hit, int* iteration_count);
};

#endif
