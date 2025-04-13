#ifndef RA_MATERIAL_H_
#define RA_MATERIAL_H_

#include "ra_math.h"

struct Material {
    vec3 base_color;
    int base_color_tex;

    float roughness;
    int roughness_tex;
    float ior;
    float metallic;
    
    float transmission;
    vec3 emission;
};
#endif