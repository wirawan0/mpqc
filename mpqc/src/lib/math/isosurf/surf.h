
#ifndef _math_isosurf_surf_h
#define _math_isosurf_surf_h

#ifdef __GNUC__
#pragma interface
#endif

#include <math/isosurf/triangle.h>
#include <math/isosurf/vertexAVLSet.h>
#include <math/isosurf/edgeAVLSet.h>
#include <math/isosurf/triAVLSet.h>
#include <util/container/pixintRAVLMap.h>
#include <util/container/intpixRAVLMap.h>
#include <math/isosurf/edgeRAVLMap.h>
#include <math/isosurf/volume.h>

class TriangulatedSurface: public DescribedClass {
#   define CLASSNAME TriangulatedSurface
#   define HAVE_CTOR
#   define HAVE_KEYVAL_CTOR
//#   include <util/state/stated.h>
#   include <util/class/classd.h>
  protected:
    int _verbose;

    int _completed_surface;

    // sets of objects that make up the surface
    RefVertexAVLSet _vertices;
    RefEdgeAVLSet _edges;
    RefTriangleAVLSet _triangles;

    // map pixes to an integer index
    PixintRAVLMap _vertex_to_index;
    PixintRAVLMap _edge_to_index;
    PixintRAVLMap _triangle_to_index;

    // map integer indices to a pix
    intPixRAVLMap _index_to_vertex;
    intPixRAVLMap _index_to_edge;
    intPixRAVLMap _index_to_triangle;

    // mappings between array element numbers
    int** _triangle_vertex;
    int** _triangle_edge;
    int** _edge_vertex;

    // values for each of the vertices
    int _have_values;
    Arraydouble _values;

    // what to use to integrate over the surface
    RefTriangleIntegrator _integrator;

    void clear_int_arrays();

    void complete_ref_arrays();
    void complete_int_arrays();

    void recompute_index_maps();

    void add_triangle(const RefTriangle&);
    void add_vertex(const RefVertex&);
    void add_edge(const RefEdge&);

    // these members must be used to allocate new triangles and edges
    // since specializations of TriangulatedSurface might need to
    // override these to produce triangles and edges with interpolation
    // data.
    virtual Triangle* newTriangle(const RefEdge&,
                                  const RefEdge&,
                                  const RefEdge&,
                                  int orientation) const;
    virtual Edge* newEdge(const RefVertex&,const RefVertex&) const;

    // this map of edges to vertices is used to construct the surface
    RefVertexRefEdgeAVLSetRAVLMap _tmp_edges;
  public:
    TriangulatedSurface();
    TriangulatedSurface(const RefKeyVal&);
    virtual ~TriangulatedSurface();

    // control printing
    int verbose() const { return _verbose; }
    void verbose(int v) { _verbose = v; }

    // set up an integrator
    void set_integrator(const RefTriangleIntegrator&);
    virtual RefTriangleIntegrator integrator(int itri);

    // construct the surface
    void add_triangle(const RefVertex&,
                      const RefVertex&,
                      const RefVertex&);
    RefEdge find_edge(const RefVertex&, const RefVertex&);
    virtual void complete_surface();

    // clean up the surface
    virtual void remove_short_edges(double cutoff_length = 1.0e-6);
    virtual void remove_slender_triangles(double heigth_cutoff = 1.0e-6);
    virtual void fix_orientation();
    virtual void clear();

    // get information from the object sets
    inline int nvertex() { return _vertices.length(); };
    inline RefVertex vertex(int i) { return _vertices(_index_to_vertex[i]); };
    inline int nedge() { return _edges.length(); };
    inline RefEdge edge(int i) { return _edges(_index_to_edge[i]); };
    inline int ntriangle() { return _triangles.length(); };
    inline RefTriangle triangle(int i) {
        return _triangles(_index_to_triangle[i]);
      }

    // information from the index mappings
    inline int triangle_vertex(int i,int j) { return _triangle_vertex[i][j]; };
    inline int triangle_edge(int i,int j) { return _triangle_edge[i][j]; };
    inline int edge_vertex(int i,int j) { return _edge_vertex[i][j]; };

    // associate values with vertices
    //void compute_colors(Volume&);
    void compute_values(RefVolume&);

    // properties of the surface
    virtual double flat_area(); // use flat triangles
    virtual double flat_volume(); // use flat triangles
    virtual double area();
    virtual double volume();

    // output of the surface
    virtual void print(ostream&o=cout);
    virtual void print_vertices_and_triangles(ostream&o=cout);
    virtual void print_geomview_format(ostream&o=cout);

    // print information about the topology
    void topology_info(ostream&o=cout);
    void topology_info(int nvertex, int nedge, int ntri, ostream&o=cout);
};
DescribedClass_REF_dec(TriangulatedSurface);

class TriangulatedSurfaceIntegrator {
  private:
    RefTriangulatedSurface _ts;
    int _itri;
    int _irs;
    double _r;
    double _s;
    double _weight;
    double _surface_element;
    RefVertex _current;
    SCVector3 _dA;
  public:
    TriangulatedSurfaceIntegrator();
    // the surface cannot be changed until this is destroyed
    TriangulatedSurfaceIntegrator(const RefTriangulatedSurface&);
    ~TriangulatedSurfaceIntegrator();
    // Objects initialized by these operators are not automatically
    // updated.  This must be done with the update member.
    void operator = (const TriangulatedSurfaceIntegrator&);
    TriangulatedSurfaceIntegrator(const TriangulatedSurfaceIntegrator&i) {
        operator = (i);
      }
    // Return the number of integration points.
    int n();
    // Assign the surface.  Don't do this while iterating.
    void set_surface(const RefTriangulatedSurface&);
    // returns the number of the vertex in the current triangle
    int vertex_number(int i);
    inline double r() const { return _r; }
    inline double s() const { return _s; }
    inline double w() const { return _weight*_surface_element; }
    double weight() const { return _weight; }
    const SCVector3& dA() const { return _dA; }
    RefVertex current();
    // Tests to see if this point is valid, if it is then
    // _r, _s, etc are computed and 1 is returned.
    int update();
    // This can be used to loop through unique pairs of points.
    // The argument should be a TriangulatedSurfaceIntegrator for
    // the same surface as this.
    int operator < (TriangulatedSurfaceIntegrator&i) {
        update();
        return _itri<i._itri?1:(_itri>i._itri?0:(_irs<i._irs?1:0));
      }
    // Goes to the next point.  Does not update.
    void operator++();
    inline void operator++(int) { operator++(); }
    // setting TSI = i sets TSI to begin at the triangle i
    int operator = (int);
};

class TriangulatedImplicitSurface: public TriangulatedSurface {
#   define CLASSNAME TriangulatedImplicitSurface
#   define HAVE_KEYVAL_CTOR
//#   include <util/state/stated.h>
#   include <util/class/classd.h>
  private:
    // The surface is defined as an isosurface of the volume vol_.
    RefVolume vol_;
    double isovalue_;

    int remove_short_edges_;
    double short_edge_factor_;
    int remove_slender_triangles_;
    double slender_triangle_factor_;
    double resolution_;

    int order_;
  public:
    TriangulatedImplicitSurface(const RefKeyVal&);
    ~TriangulatedImplicitSurface();

    void init();
};
DescribedClass_REF_dec(TriangulatedImplicitSurface);

#endif
