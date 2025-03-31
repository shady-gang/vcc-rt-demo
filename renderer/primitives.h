#ifndef RA_PRIMITIVES
#define RA_PRIMITIVES

#include "ra_math.h"

struct Ray {
    vec3 origin;
    vec3 dir;
    float tmin = 0, tmax = 99999;
};

struct Hit {
    float t;
    vec3 p, n;
};

struct Sphere {
    vec3 center;
    float radius;

    RA_METHOD bool intersect(Ray r, Hit&);
};

struct BBox {
    vec3 min, max;

    RA_METHOD void intersect_range(Ray r, vec3 ray_inv_dir, float t[2]);
    RA_METHOD bool intersect(Ray r, vec3 ray_inv_dir, Hit&);
    RA_METHOD bool contains(vec3 point);
};

struct Triangle {
    vec3 v0, v1, v2;

    RA_METHOD bool intersect(Ray r, Hit&);
};

#endif
