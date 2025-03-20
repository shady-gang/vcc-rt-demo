#include "cunk/math.h"

Mat4f invert_mat4(Mat4f m) {
    float a = m.m00 * m.m11 - m.m01 * m.m10;
    float b = m.m00 * m.m12 - m.m02 * m.m10;
    float c = m.m00 * m.m13 - m.m03 * m.m10;
    float d = m.m01 * m.m12 - m.m02 * m.m11;
    float e = m.m01 * m.m13 - m.m03 * m.m11;
    float f = m.m02 * m.m13 - m.m03 * m.m12;
    float g = m.m20 * m.m31 - m.m21 * m.m30;
    float h = m.m20 * m.m32 - m.m22 * m.m30;
    float i = m.m20 * m.m33 - m.m23 * m.m30;
    float j = m.m21 * m.m32 - m.m22 * m.m31;
    float k = m.m21 * m.m33 - m.m23 * m.m31;
    float l = m.m22 * m.m33 - m.m23 * m.m32;
    float det = a * l - b * k + c * j + d * i - e * h + f * g;
    det = 1.0f / det;
    Mat4f r;
    r.m00 = ( m.m11 * l - m.m12 * k + m.m13 * j) * det;
    r.m01 = (-m.m01 * l + m.m02 * k - m.m03 * j) * det;
    r.m02 = ( m.m31 * f - m.m32 * e + m.m33 * d) * det;
    r.m03 = (-m.m21 * f + m.m22 * e - m.m23 * d) * det;
    r.m10 = (-m.m10 * l + m.m12 * i - m.m13 * h) * det;
    r.m11 = ( m.m00 * l - m.m02 * i + m.m03 * h) * det;
    r.m12 = (-m.m30 * f + m.m32 * c - m.m33 * b) * det;
    r.m13 = ( m.m20 * f - m.m22 * c + m.m23 * b) * det;
    r.m20 = ( m.m10 * k - m.m11 * i + m.m13 * g) * det;
    r.m21 = (-m.m00 * k + m.m01 * i - m.m03 * g) * det;
    r.m22 = ( m.m30 * e - m.m31 * c + m.m33 * a) * det;
    r.m23 = (-m.m20 * e + m.m21 * c - m.m23 * a) * det;
    r.m30 = (-m.m10 * j + m.m11 * h - m.m12 * g) * det;
    r.m31 = ( m.m00 * j - m.m01 * h + m.m02 * g) * det;
    r.m32 = (-m.m30 * d + m.m31 * b - m.m32 * a) * det;
    r.m33 = ( m.m20 * d - m.m21 * b + m.m22 * a) * det;
    return r;
}

Mat4f mul_mat4f(Mat4f l, Mat4f r) {
    Mat4f dst = { 0 };
#define a(i, j) m##i##j
#define t(bc, br, i) l.a(i, br) * r.a(bc, i)
#define e(bc, br) dst.a(bc, br) = t(bc, br, 0) + t(bc, br, 1) + t(bc, br, 2) + t(bc, br, 3);
#define row(c) e(c, 0) e(c, 1) e(c, 2) e(c, 3)
#define genmul() row(0) row(1) row(2) row(3)
    genmul()
    return dst;
#undef a
#undef t
#undef e
#undef row
#undef genmul
}

Vec4f mul_mat4f_vec4f(Mat4f l, Vec4f r) {
    Vec4f dst = { 0 };
#define a(i, j) m##i##j
#define t(bc, br, i) l.a(i, br) * r.arr[i]
#define e(bc, br) dst.arr[br] = t(bc, br, 0) + t(bc, br, 1) + t(bc, br, 2) + t(bc, br, 3);
#define row(c) e(c, 0) e(c, 1) e(c, 2) e(c, 3)
#define genmul() row(0)
    genmul()
    return dst;
}