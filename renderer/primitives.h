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
};

struct BBox {
    vec3 min, max;
};

