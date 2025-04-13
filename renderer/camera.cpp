#include "camera.h"

using namespace nasl;

RA_FUNCTION void camera_update_orientation(Camera* cam, vec3 ndir, vec3 nup) {
    cam->direction = ndir;
    cam->right = normalize(cross(cam->direction, nup));
    cam->up = normalize(cross(cam->right, cam->direction));
}
