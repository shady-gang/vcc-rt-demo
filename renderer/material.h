#ifndef RA_MATERIAL_H_
#define RA_MATERIAL_H_

#include "ra_math.h"

// TODO: Too simple
struct Material {
    vec3 base_color;
    float roughness;

    float ior;
    float metallic;
    float transmission;
    float _pad1;
    
    vec3 emission;
    float _pad2;
};
#endif