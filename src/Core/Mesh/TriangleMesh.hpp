#ifndef RADIUMENGINE_TRIANGLEMESH_HPP
#define RADIUMENGINE_TRIANGLEMESH_HPP

#include <Core/Containers/VectorArray.hpp>
#include <Core/Math/LinearAlgebra.hpp>
#include <Core/Mesh/MeshTypes.hpp>
#include <Core/RaCore.hpp>

namespace Ra {
namespace Core {

/// VertexAttribBase is the base class for VertexAttrib.
/// A VertexAttrib is data linked to Vertices of a mesh
/// It is also used for rendering, and
class VertexAttribBase {
  public:
    /// attrib name is used to automatic location binding when using shaders.
    virtual ~VertexAttribBase() {}
    std::string getName() const { return m_name; }
    void setName( std::string name ) { m_name = name; }
    virtual void resize( size_t s ) = 0;

    virtual uint getSize() = 0;
    virtual int getStride() = 0;

  private:
    std::string m_name;
};

template <typename T>
class VertexAttrib : public VertexAttribBase {
  public:
    using value_type = T;
    using Container = VectorArray<T>;

    /// resize the container (value_type must have a default ctor).
    void resize( size_t s ) override { m_data.resize( s ); }

    /// RW acces to container data
    inline Container& data() { return m_data; }

    /// R only acccess to container data
    inline const Container& data() const { return m_data; }

    virtual ~VertexAttrib() { m_data.clear(); }
    uint getSize() override { return Container::Vector::RowsAtCompileTime; }
    int getStride() override { return sizeof( typename Container::Vector ); }

  private:
    Container m_data;
};

template <typename T>
class VertexAttribHandle {
  public:
    typedef T value_type;
    using Container = typename VertexAttrib<T>::Container;

    /// There is no validity check against the corresponding mesh, but just a
    /// simple test to allow the manipuation of unitialized handles.
    constexpr bool isValid() const { return m_idx != -1; }

  private:
    int m_idx = -1;

    friend class VertexAttribManager;
};

class VertexAttribManager {
  public:
    using value_type = VertexAttribBase*;
    using Container = std::vector<value_type>;

    const Container& attribs() const { return m_attribs; }
    /// clear all attribs, invalidate handles !
    void clear() {
        for ( auto a : m_attribs )
        {
            delete a;
        }
        m_attribs.clear();
    }

    value_type getAttrib( const std::string& name ) {
        auto c = m_attribsIndex.find( name );
        if ( c != m_attribsIndex.end() )
        {
            return m_attribs[c->second];
        }
        return nullptr;
    }

    template <typename T>
    inline VertexAttrib<T>& getAttrib( VertexAttribHandle<T> h ) {
        return *static_cast<VertexAttrib<T>*>( m_attribs[h.m_idx] );
    }

    template <typename T>
    inline const VertexAttrib<T>& getAttrib( VertexAttribHandle<T> h ) const {
        return *static_cast<VertexAttrib<T>*>( m_attribs[h.m_idx] );
    }

    template <typename T>
    VertexAttribHandle<T> addAttrib( const T&, std::string name ) {
        VertexAttribHandle<T> h;
        addAttrib( h, name );
        return h;
    }

    template <typename T>
    void addAttrib( VertexAttribHandle<T>& h, std::string name ) {
        VertexAttrib<T>* attrib = new VertexAttrib<T>;
        attrib->setName( name );
        m_attribs.push_back( attrib );
        h.m_idx = m_attribs.size() - 1;
        m_attribsIndex[name] = h.m_idx;
    }

    /// Remove attrib by name, invalidate all handle
    void removeAttrib( const std::string& name ) {
        auto c = m_attribsIndex.find( name );
        if ( c != m_attribsIndex.end() )
        {
            int idx = c->second;
            delete m_attribs[idx];
            m_attribs.erase( m_attribs.begin() + idx );
            m_attribsIndex.erase( c );

            // reindex attribs with index superior to removed index
            for ( auto& d : m_attribsIndex )
            {
                if ( d.second > idx )
                {
                    --d.second;
                }
            }
        }
    }

  private:
    std::map<std::string, int> m_attribsIndex;
    Container m_attribs;
};

/// Simple Mesh structure that handles indexed triangle mesh with vertex
/// attributes.
/// Attributes are unique per vertex, so that same position with different
/// normals are two vertices.
/// The VertexAttribManager allows to dynammicaly add VertexAttrib per Vertex
/// See MeshUtils for geometric functions operating on a mesh.
/// Points and Normals are always present, accessible with
/// points() and normals()
/// Other attribs could be added with attribManager().addAttrib() and
/// accesssed with attribManager().getAttrib()
struct TriangleMesh {
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    using PointAttribHandle = VertexAttribHandle<Vector3>;
    using NormalAttribHandle = VertexAttribHandle<Vector3>;
    using Vec3AttribHandle = VertexAttribHandle<Vector3>;
    using Vec4AttribHandle = VertexAttribHandle<Vector4>;
    using Face = VectorNui;

    /// Create an empty mesh.
    inline TriangleMesh() { initDefaultAttribs(); }
    /// Copy constructor and assignment operator
    TriangleMesh( const TriangleMesh& ) = default;
    TriangleMesh& operator=( const TriangleMesh& ) = default;

    /// Erases all data, making the mesh empty.
    inline void clear();

    /// Appends another mesh to this one.
    /// \todo handle attrib here as well !
    inline void append( const TriangleMesh& other );

    VectorArray<Triangle> m_triangles;
    VectorArray<Face> m_faces;

    PointAttribHandle::Container& vertices() {
        return m_vertexAttribs.getAttrib( m_verticesHandle ).data();
    }
    NormalAttribHandle::Container& normals() {
        return m_vertexAttribs.getAttrib( m_normalsHandle ).data();
    }

    const PointAttribHandle::Container& vertices() const {
        return m_vertexAttribs.getAttrib( m_verticesHandle ).data();
    }
    const NormalAttribHandle::Container& normals() const {
        return m_vertexAttribs.getAttrib( m_normalsHandle ).data();
    }

    const VertexAttribManager& attribManager() const { return m_vertexAttribs; }
    VertexAttribManager& attribManager() { return m_vertexAttribs; }

  private:
    VertexAttribManager m_vertexAttribs;
    PointAttribHandle m_verticesHandle;
    NormalAttribHandle m_normalsHandle;

    inline void initDefaultAttribs() {
        m_vertexAttribs.addAttrib( m_verticesHandle, "in_position" );
        m_vertexAttribs.addAttrib( m_normalsHandle, "in_normal" );
    }
};

} // namespace Core
} // namespace Ra

#include <Core/Mesh/TriangleMesh.inl>

#endif // RADIUMENGINE_TRIANGLEMESH_HPP
