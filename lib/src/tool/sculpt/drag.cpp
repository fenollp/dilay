#include <QDoubleSpinBox>
#include <glm/glm.hpp>
#include "color.hpp"
#include "mesh.hpp"
#include "primitive/ray.hpp"
#include "render-flags.hpp"
#include "render-mode.hpp"
#include "sculpt-brush/carve.hpp"
#include "state.hpp"
#include "tool/sculpt/behaviors.hpp"
#include "tool/util/movement.hpp"
#include "view/cursor.hpp"
#include "view/properties.hpp"
#include "view/tool/tip.hpp"
#include "winged/face-intersection.hpp"

struct ToolSculptDrag::Impl {
  ToolSculptDrag*  self;
  SculptBrushCarve brush;
  ToolUtilMovement movement;

  std::vector <glm::vec3> draggedVertices;
  Mesh                    draggedVerticesMesh;

  Impl (ToolSculptDrag* s) 
    : self     (s)
    , movement ( this->self->state ().camera ()
               , glm::vec3 (0.0f)
               //, MovementConstraint::CameraPlane )
               //, MovementConstraint::YAxis )
               , MovementConstraint::XYPlane )
  {}

  void runSetupBrush () {}

  void runSetupProperties (ViewProperties&) {}

  void runSetupToolTip (ViewToolTip& toolTip) {
    toolTip.add (ViewToolTip::MouseEvent::Left, QObject::tr ("Drag to carve"));
  }

  void updateDraggedVerticesMesh () {
    this->draggedVerticesMesh.reset ();

    if (this->draggedVertices.size () >= 2) {
      for (unsigned int i = 0; i < this->draggedVertices.size (); i++) {
        this->draggedVerticesMesh.addVertex (this->draggedVertices.at (i));

        if (i > 0) {
          this->draggedVerticesMesh.addIndex (i-1);
          this->draggedVerticesMesh.addIndex (i);
        }
      }
      this->draggedVerticesMesh.renderMode (RenderMode::Constant);
      this->draggedVerticesMesh.color      (Color::Red ());
      this->draggedVerticesMesh.bufferData ();
    }
  }

  void runMouseLeftPressEvent (const glm::ivec2& pos) {

    WingedFaceIntersection intersection;
    if (this->self->intersectsSelection (pos, intersection)) {
      this->self->cursor ().disable ();

      this->movement.resetPosition ( intersection.position ());
      this->brush.mesh             (&intersection.mesh     ());
      this->brush.face             (&intersection.face     ());
      this->brush.setPosition      ( intersection.position ());

      this->draggedVertices.clear        ();
      this->draggedVertices.emplace_back (intersection.position ());
      this->draggedVerticesMesh.reset    ();
    }
    else {
      this->self->cursor ().enable ();
      this->brush.resetPosition ();
    }
  }

  void runMouseMoveEvent (const glm::ivec2& pos, bool leftButton) {
    if (leftButton == false) {
      WingedFaceIntersection intersection;

      if (this->self->intersectsSelection (pos, intersection)) {
        this->self->cursor ().enable   ();
        this->self->cursor ().position (intersection.position ());
        this->self->cursor ().normal   (intersection.normal   ());
      }
      else {
        this->self->cursor ().disable ();
      }
    }
    else if ( this->draggedVertices.empty () == false
           && this->brush.hasPosition () 
           && this->movement.move     (pos)
           && this->brush.updatePosition (this->movement.position ()) )
    {
      this->draggedVertices.emplace_back (this->movement.position ());
      this->updateDraggedVerticesMesh    ();

    }
  }

  void runMouseLeftReleaseEvent (const glm::ivec2&) {

    auto initialSculpt = [this] {
      this->brush.useIntersection (false);
      this->brush.direction       (this->draggedVertices.at (1) - this->draggedVertices.at (0));
      this->brush.setPosition     (this->draggedVertices.at (0));
      this->self->sculpt ();
    };

    auto checkAgainstSegment = [this] (const glm::vec3& a, const glm::vec3& b) -> bool {
      const PrimRay ray         (a, b - a);
      const float   maxDistance (glm::length (b - a));

      WingedFaceIntersection intersection;
      if ( this->self->intersectsSelection (ray, intersection) 
        && intersection.distance () <= maxDistance ) 
      {
        this->brush.mesh            (&intersection.mesh     ());
        this->brush.face            (&intersection.face     ());
        this->brush.direction       (b - a);
        this->brush.setPosition     (intersection.position ());
        this->brush.useIntersection (true);
        this->self->sculpt ();
        return true;
      }
      else {
        return false;
      }
    };

    auto checkAgainstDraggedVertices = [this,&checkAgainstSegment] () -> bool {
      for (unsigned int i = 1; i < this->draggedVertices.size (); i++) {
        if (checkAgainstSegment ( this->draggedVertices.at (i-1)
                                , this->draggedVertices.at (i) )) 
        {
          return true;
        }
      }
      return false;
    };

    this->brush.intensityFactor (1.0f / this->brush.radius ());
    this->brush.order (20);

    initialSculpt ();

    while (checkAgainstDraggedVertices ()) {}

    this->draggedVertices.clear     ();
    this->draggedVerticesMesh.reset ();
  }

  void runRender () const {
    if (this->draggedVertices.size () >= 2) {
      this->draggedVerticesMesh.renderLines ( this->self->state ().camera ()
                                            , RenderFlags::NoDepthTest () );
    }
  }
};

DELEGATE_TOOL_BEHAVIOR                              (ToolSculptDrag)
DELEGATE_TOOL_BEHAVIOR_RUN_MOUSE_LEFT_RELEASE_EVENT (ToolSculptDrag)
DELEGATE_TOOL_BEHAVIOR_RUN_RENDER                   (ToolSculptDrag)
