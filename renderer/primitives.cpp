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

Hit Triangle::intersect(Ray ray) {
    const auto &v0       = this->v0;
    const auto &v1       = this->v1;
    const auto &v2       = this->v2;

    // why does 'auto' break it ?
    const vec3 edge1 = v1 - v0;
    const vec3 edge2 = v2 - v0;

    const auto pvec = ray.dir.cross(edge2);
    const auto det  = edge1.dot(pvec);
    if (det > -1e-8f && det < 1e-8f)
        return (Hit) { -1.0f };
    const auto invDet = 1 / det;

    const auto tvec = ray.origin - v0;
    const float u   = tvec.dot(pvec) * invDet;
    if (u < 0 || u > 1)
        return (Hit) { -1.0f };

    const auto qvec = tvec.cross(edge1);
    const float v   = ray.dir.dot(qvec) * invDet;
    if (v < 0 || u + v > 1)
        return (Hit) { -1.0f };

    const float t = edge2.dot(qvec) * invDet;
    if (t < epsilon)
        return (Hit) { -1.0f };

    // const auto vInterpolated = Vertex::interpolate({ u, v }, v0, v1, v2);

    //const auto deltaUV1 = v1.uv - v0.uv;
    //const auto deltaUV2 = v2.uv - v0.uv;
    //const float r =
    //    deltaUV1.x() * deltaUV2.y() - deltaUV1.y() * deltaUV2.x();
    //const auto tangent =
    //    abs(r) < 1e-6 ? edge1
    //                  : (edge1 * deltaUV2.y() - edge2 * deltaUV1.y()) / r;

    Hit hit {
        .t = t,
        .p = interpolateBarycentric<vec3>({u, v}, v0, v1, v2),
        .n = normalize(edge1.cross(edge2)),
    };

    return hit;

    //its.t              = t;
    //its.position       = vInterpolated.position;
    //its.uv             = vInterpolated.uv;
    //its.geometryNormal = edge1.cross(edge2).normalized();
    //its.shadingNormal  = m_smoothNormals ? vInterpolated.normal.normalized()
    //                                     : its.geometryNormal;
    //its.tangent        = tangent;
    //its.pdf            = 0;
}