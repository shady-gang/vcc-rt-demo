#ifndef RA_BVH
#define RA_BVH

#include "primitives.h"

#define BVH_ARITY 2

#define BVH_REORDER_TRIS

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
#ifndef BVH_REORDER_TRIS
    int* indices;
#endif
    Triangle* tris;

    RA_METHOD bool intersect(Ray ray, Hit& hit, bool return_early, int* iteration_count);
    RA_METHOD bool intersect(Ray ray, Hit& hit, int* iteration_count) {
        return intersect(ray, hit, false, iteration_count);
    }
    RA_METHOD bool intersect(Ray ray, Hit& hit, bool return_early = false) { 
        int ic;
        return intersect(ray, hit, return_early, &ic);
    }
    RA_METHOD bool intersect_shadow(Ray ray) { 
        Hit hit;
        return intersect(ray, hit, true);
    }
};

#endif
