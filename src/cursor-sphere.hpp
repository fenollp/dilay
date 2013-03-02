#include <glm/glm.hpp>
#include "util.hpp"
#include "mesh.hpp"

#ifndef CURSOR_SPHERE
#define CURSOR_SPHERE

class CursorSphere {
  public: CursorSphere () : _radius (0.1f) {}
          CursorSphere (const CursorSphere&) = delete;

    void  initialize   ();
    void  setPosition  (const glm::vec3& v) { this->_mesh.setPosition (v); }
    void  render       ();

  private:
    Mesh  _mesh;
    float _radius;
};

#endif