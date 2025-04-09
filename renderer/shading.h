#ifndef RA_SHADING_H_
#define RA_SHADING_H_

#include "ra_math.h"
using namespace nasl;

namespace shading {
struct ShadingFrame {
  vec3 n;
  vec3 t;
  vec3 bt;
};

/* Tom Duff, James Burgess, Per Christensen, Christophe Hery, Andrew Kensler,
 * Max Liani, and Ryusuke Villemin, Building an Orthonormal Basis, Revisited,
 * Journal of Computer Graphics Techniques (JCGT), vol. 6, no. 1, 1-8, 2017
 * http://jcgt.org/published/0006/01/01/
 */
RA_METHOD ShadingFrame make_shading_frame(vec3 n) {
  const float sign = copysignf(1, n[2]);
  const float a = -1 / (sign + n[2]);
  const float b = n[0] * n[1] * a;

  const vec3 t = vec3(1 + sign * n[0] * n[0] * a, sign * b, -sign * n[0]);
  const vec3 bt = vec3(b, sign + n[1] * n[1] * a, -n[1]);
  return ShadingFrame{.n = n, .t = t, .bt = bt};
}

RA_METHOD vec3 to_world(vec3 local_dir, ShadingFrame frame) {
  return local_dir[0] * frame.t + local_dir[1] * frame.bt +
         local_dir[2] * frame.n;
}

RA_METHOD vec3 to_local(vec3 world_dir, ShadingFrame frame) {
  return vec3(frame.t.dot(world_dir), frame.bt.dot(world_dir),
              frame.n.dot(world_dir));
}

RA_METHOD vec3 to_world(vec3 local_dir, vec3 normal) {
  return to_world(local_dir, make_shading_frame(normal));
}

RA_METHOD vec3 to_local(vec3 world_dir, vec3 normal) {
  return to_local(world_dir, make_shading_frame(normal));
}

} // namespace shading

#endif