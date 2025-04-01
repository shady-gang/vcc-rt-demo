#ifndef RA_HOST_H
#define RA_HOST_H

namespace shady {
extern "C" {

#include "shady/runner.h"

}
}

template<typename T>
void offload(shady::Device* device, const std::vector<T>& src, shady::Buffer*& dst) {
    assert(device && !dst);
    dst = shady::shd_rn_allocate_buffer_device(device, src.size() * sizeof(T));
    shd_rn_copy_to_buffer(dst, 0, (void*) src.data(), src.size() * sizeof(T));
}

#endif
