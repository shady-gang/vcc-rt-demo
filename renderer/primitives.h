#include "ra_math.h"

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct Hit {
    float t;
    vec3 p, n;
};

struct Sphere {
    vec3 center;
    float radius;

    Hit intersect(Ray r);
};

struct BBox {
    vec3 min, max;

    Hit intersect(Ray r, vec3 ray_inv_dir);
};

