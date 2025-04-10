#ifndef RA_PRIMITIVES
#define RA_PRIMITIVES

#include "ra_math.h"
#include "random.h"

struct Ray {
    vec3 origin;
    vec3 dir;
    float tmin = 0, tmax = 99999;
};

// struct Hit {
//     float t;
//     // pls pack well
//     // vec3 p;
//     float u;
//     vec3 n;
//     float v;
// };

struct Hit {
    float t;
    vec2 primary; // aka. barycentric coordinates
    int prim_id;
};

struct Sphere {
    int prim_id;
    int mat_id;
    vec3 center;
    float radius;

    RA_METHOD bool intersect(Ray r, Hit&);
};

struct BBox {
    vec3 min, max;

    RA_METHOD void intersect_range(Ray r, vec3 ray_inv_dir, vec3 morigin_t_riv, float t[2]);
    RA_METHOD bool intersect(Ray r, vec3 ray_inv_dir, Hit&);
    RA_METHOD bool contains(vec3 point);
};

struct Triangle {
    int prim_id;
    int mat_id;
    vec3 v0, v1, v2;
    vec3 n0, n1, n2;
    vec2 t0, t1, t2;

    RA_METHOD bool intersect(Ray r, Hit&);
    RA_METHOD vec3 get_face_normal() const;
    RA_METHOD vec3 get_position(vec2 bary) const;
    RA_METHOD vec3 get_vertex_normal(vec2 bary) const;
    RA_METHOD vec2 get_texcoords(vec2 bary) const;
    RA_METHOD float get_area() const;
    RA_METHOD vec2 sample_point_on_surface(RNGState* rng);
};

#endif
