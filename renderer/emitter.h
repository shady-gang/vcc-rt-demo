#ifndef RA_EMITTER_H_
#define RA_EMITTER_H_

#include "ra_math.h"

struct Emitter {
    /// @brief Emission value with pdf already applied
    vec3 emission;
    int prim_id;
};

#endif