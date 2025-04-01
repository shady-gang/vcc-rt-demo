#ifndef RA_BVH
#define RA_BVH

#include "primitives.h"

#define BVH_ARITY 2

struct BVH {
    struct Node {
        bool is_leaf;
        BBox box;
        union {
            struct {
                size_t count;
                int start;
            } leaf;
            struct {
                int children[BVH_ARITY];
            } inner;
        };
    };
    int root = 0;
    Node* nodes;
    int* indices;
    Triangle* tris;

    RA_METHOD bool intersect(Ray ray, Hit& hit, int* iteration_count);
};

#endif
