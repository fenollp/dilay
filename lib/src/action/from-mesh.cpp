#include <vector>
#include <map>
#include <glm/glm.hpp>
#include "action/from-mesh.hpp"
#include "action/modify-winged-mesh.hpp"
#include "winged/mesh.hpp"
#include "winged/edge.hpp"
#include "mesh.hpp"
#include "action/unit/on.hpp"
#include "partial-action/modify-winged-mesh.hpp"
#include "partial-action/modify-winged-face.hpp"
#include "partial-action/modify-winged-edge.hpp"
#include "partial-action/modify-winged-vertex.hpp"
#include "id.hpp"
#include "primitive/triangle.hpp"

struct ActionFromMesh :: Impl {
  ActionFromMesh*           self;
  ActionUnitOn <WingedMesh> actions;

  Impl (ActionFromMesh* s) : self (s) {}

  void run (WingedMesh& w, const Mesh& m) {
    assert (this->actions.isEmpty ());
    assert (w.isEmpty ());

    typedef std::pair  <unsigned int,unsigned int> uiPair;
    std::vector        <WingedVertex*>             vecVertices (m.numVertices ());
    std::map           <uiPair, WingedEdge*>       mapEdges;

    /** `findOrAddEdge (i1,i2,f)` searches an edge with indices `(i2,i1)` (in that order).
     * If such an edge exists, `f` becomes its new right face.
     * Otherwise a new edge is added with `f` being its left face.
     * The found (resp. created) edge is returned
     */
    auto findOrAddEdge = [&w,&vecVertices,&mapEdges,this] 
      (unsigned int index1, unsigned int index2, WingedFace& face) -> WingedEdge* {
        auto result = mapEdges.find (uiPair (index2, index1));
        if (result == mapEdges.end ()) {
          WingedVertex* v1    = vecVertices [index1];
          WingedVertex* v2    = vecVertices [index2];
          WingedEdge& newEdge = this->actions.add <PAModifyWMesh> ()
                                     .addEdge (w, WingedEdge ( v1, v2
                                                             , &face  , nullptr
                                                             , nullptr, nullptr
                                                             , nullptr, nullptr
                                                             , nullptr, nullptr
                                                             , Id ()  , false
                                                             , FaceGradient::None
                                                             ));
            
          mapEdges.insert (std::pair <uiPair,WingedEdge*> ( uiPair (index1,index2)
                                                          , &newEdge ));

          this->actions.add <PAModifyWVertex> ().edge (*v1 , &newEdge);
          this->actions.add <PAModifyWVertex> ().edge (*v2 , &newEdge);
          this->actions.add <PAModifyWFace  > ().edge (face, &newEdge);

          return &newEdge;
        }
        else {
          WingedEdge* existingEdge = result->second;

          this->actions.add <PAModifyWEdge> ().rightFace (*existingEdge, &face);
          this->actions.add <PAModifyWFace> ().edge      (face, existingEdge);
          return existingEdge;
        }
      };

    glm::vec3 maxVertex (std::numeric_limits <float>::lowest ());
    glm::vec3 minVertex (std::numeric_limits <float>::max    ());

    // octree
    for (unsigned int i = 0; i < m.numVertices (); i++) {
      glm::vec3 v = m.vertex (i);
      maxVertex = glm::max (maxVertex, v);
      minVertex = glm::min (minVertex, v);
    }
    glm::vec3 center = (maxVertex + minVertex) * glm::vec3 (0.5f);
    glm::vec3 delta  =  maxVertex - minVertex;
    float     width  = glm::max (glm::max (delta.x, delta.y), delta.z);

    this->actions.add <PAModifyWMesh> ().setupOctreeRoot (w, center, width);

    // vertices
    for (unsigned int i = 0; i < m.numVertices (); i++) {
      vecVertices[i] = &this->actions.add<PAModifyWMesh> ()
                                     .addVertex (w, m.vertex (i));
    }

    // faces & edges
    for (unsigned int i = 0; i < m.numIndices (); i += 3) {
      unsigned int index1 = m.index (i + 0);
      unsigned int index2 = m.index (i + 1);
      unsigned int index3 = m.index (i + 2);

      WingedFace& f = this->actions.add<PAModifyWMesh> ()
                           .addFace (w, PrimTriangle (w, *vecVertices [index1]
                                                       , *vecVertices [index2]
                                                       , *vecVertices [index3]
                                                       ));
      WingedEdge* e1 = findOrAddEdge (index1, index2, f);
      WingedEdge* e2 = findOrAddEdge (index2, index3, f);
      WingedEdge* e3 = findOrAddEdge (index3, index1, f);

      this->actions.add <PAModifyWEdge> ().predecessor (*e1, f, e3);
      this->actions.add <PAModifyWEdge> ().successor   (*e1, f, e2);
      this->actions.add <PAModifyWEdge> ().predecessor (*e2, f, e1);
      this->actions.add <PAModifyWEdge> ().successor   (*e2, f, e3);
      this->actions.add <PAModifyWEdge> ().predecessor (*e3, f, e2);
      this->actions.add <PAModifyWEdge> ().successor   (*e3, f, e1);
    }

    this->actions.add <ActionModifyWMesh> ().position       (w, m.position ());
    this->actions.add <ActionModifyWMesh> ().rotationMatrix (w, m.rotationMatrix ());
    this->actions.add <ActionModifyWMesh> ().scaling        (w, m.scaling ());

    // write & buffer
    w.writeAllIndices ();
    w.writeAllNormals ();
    w.bufferData      ();
  }

  void runUndo (WingedMesh& mesh) { 
    this->actions.undo (mesh); 
    mesh.reset ();
  }

  void runRedo (WingedMesh& mesh) { 
    this->actions.redo   (mesh); 
    mesh.writeAllIndices ();
    mesh.writeAllNormals ();
    mesh.bufferData      ();
  }
};

DELEGATE_BIG3_SELF (ActionFromMesh)
DELEGATE2          (void, ActionFromMesh, run, WingedMesh&, const Mesh&);
DELEGATE1          (void, ActionFromMesh, runUndo, WingedMesh&)
DELEGATE1          (void, ActionFromMesh, runRedo, WingedMesh&)
