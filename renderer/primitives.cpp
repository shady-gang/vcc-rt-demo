#include "primitives.h"

Hit Sphere::intersect(Ray r) {
    vec3 rs = r.origin - this->center;
    float b = dot(rs, r.dir);
    float c = dot(rs, rs) - this->radius * this->radius;
    float d = b * b - c;

    if (d > 0.0f) {
        float t = -b - sqrtf(d);
        if (t > 0.0f) {
            vec3 p = r.origin + r.dir * t;
            vec3 n = normalize(p - this->center);
            return (Hit) { t, p, n };
        }
    }

    return (Hit) { -1.0f };
}

Hit BBox::intersect(Ray r, vec3 ray_inv_dir) {
    float txmin = fmaf(this->min.x, ray_inv_dir.x, -(r.origin.x * ray_inv_dir.x));
    float txmax = fmaf(this->max.x, ray_inv_dir.x, -(r.origin.x * ray_inv_dir.x));
    float tymin = fmaf(this->min.y, ray_inv_dir.y, -(r.origin.y * ray_inv_dir.y));
    float tymax = fmaf(this->max.y, ray_inv_dir.y, -(r.origin.y * ray_inv_dir.y));
    float tzmin = fmaf(this->min.z, ray_inv_dir.z, -(r.origin.z * ray_inv_dir.z));
    float tzmax = fmaf(this->max.z, ray_inv_dir.z, -(r.origin.z * ray_inv_dir.z));

    auto t0x = fminf(txmin, txmax);
    auto t1x = fmaxf(txmin, txmax);
    auto t0y = fminf(tymin, tymax);
    auto t1y = fmaxf(tymin, tymax);
    auto t0z = fminf(tzmin, tzmax);
    auto t1z = fmaxf(tzmin, tzmax);

    //auto t0 = max(max(t0x, t0y), max(r.tmin, t0z));
    //auto t1 = min(min(t1x, t1y), min(r.tmax, t1z));

    enum {
        X = 0, Y, Z
    } axis = X;
    float max_t0xy = t0x;
    if (t0y > t0x) {
        axis = Y;
        max_t0xy = t0y;
    }
    auto t0 = max_t0xy;
    if (t0z > max_t0xy) {
        axis = Z;
        t0 = t0z;
    }

    //auto t0 = fmaxf(fmaxf(t0x, t0y), t0z);
    auto t1 = fminf(fminf(t1x, t1y), t1z);

    if (t0 < t1) {
        vec3 p = r.origin + r.dir * t0;
        vec3 n(0.0f);
        n.arr[axis] = -sign(r.dir.arr[axis]);
        return (Hit) { t0, p, n };
    }

    return (Hit) { -1.0f };
}