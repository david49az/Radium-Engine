#include <Engine/Renderer/Camera/Camera.hpp>

#include <Core/Math/Math.hpp>

#include <Engine/Renderer/Mesh/Mesh.hpp>
#include <Engine/Renderer/RenderObject/RenderObject.hpp>
#include <Engine/Renderer/RenderTechnique/ShaderConfigFactory.hpp>

namespace Ra {

using Core::Math::Pi;
using Core::Math::PiDiv2;
using Core::Math::PiDiv4;

namespace Engine {

Camera::Camera( Entity* entity, const std::string& name, Scalar height, Scalar width ) :
    Component( name, entity ),
    m_width{width},
    m_height{height},
    m_aspect{width / height} {}

Camera::~Camera() = default;

void Camera::initialize() {
    if ( !m_renderObjects.empty() ) return;
    // Create the render mesh for the camera
    auto m = std::make_shared<Mesh>( m_name + "_mesh" );
    Ra::Core::Geometry::TriangleMesh triMesh;
    triMesh.vertices()  = {{0_ra, 0_ra, 0_ra},
                          {-.5_ra, -.5_ra, -1_ra},
                          {-.5_ra, .5_ra, -1_ra},
                          {.5_ra, .5_ra, -1_ra},
                          {.5_ra, -.5_ra, -1_ra},
                          {-.3_ra, .5_ra, -1_ra},
                          {0_ra, .7_ra, -1_ra},
                          {.3_ra, .5_ra, -1_ra}};
    triMesh.m_triangles = {{0, 1, 2}, {0, 2, 3}, {0, 3, 4}, {0, 4, 1}, {5, 6, 7}};
    m->loadGeometry( std::move( triMesh ) );
    Core::Vector4Array c( 8, {.2_ra, .2_ra, .2_ra, 1_ra} );
    m->addData( Mesh::VERTEX_COLOR, c );

    // Create the RO
    m_RO = RenderObject::createRenderObject( m_name + "_RO", this, RenderObjectType::Debug, m );
    m_RO->getRenderTechnique()->setConfiguration(
        ShaderConfigurationFactory::getConfiguration( "Plain" ) );
    m_RO->setLocalTransform( m_frame );
    addRenderObject( m_RO );
}

void Camera::show( bool on ) {
    m_RO->setVisible( on );
}

void Camera::applyTransform( const Core::Transform& T ) {
    Core::Transform t1 = Core::Transform::Identity();
    Core::Transform t2 = Core::Transform::Identity();
    t1.translation()   = -getPosition();
    t2.translation()   = getPosition();

    m_frame = t2 * T * t1 * m_frame;
    m_RO->setLocalTransform( m_frame );
}

void Camera::updateProjMatrix() {

    switch ( m_projType )
    {
    case ProjType::ORTHOGRAPHIC:
    {
        const Scalar dx = m_zoomFactor * .5_ra;
        const Scalar dy = dx / m_aspect;
        // ------------
        // Compute projection matrix as describe in the doc of gluPerspective()
        const Scalar l = -dx; // left
        const Scalar r = dx;  // right
        const Scalar t = dy;  // top
        const Scalar b = -dy; // bottom

        Core::Vector3 tr( -( r + l ) / ( r - l ),
                          -( t + b ) / ( t - b ),
                          -( ( m_zFar + m_zNear ) / ( m_zFar - m_zNear ) ) );

        m_projMatrix.setIdentity();

        m_projMatrix.coeffRef( 0, 0 )    = 2_ra / ( r - l );
        m_projMatrix.coeffRef( 1, 1 )    = 2_ra / ( t - b );
        m_projMatrix.coeffRef( 2, 2 )    = -2_ra / ( m_zFar - m_zNear );
        m_projMatrix.block<3, 1>( 0, 3 ) = tr;
    }
    break;

    case ProjType::PERSPECTIVE:
    {
        // Compute projection matrix as describe in the doc of gluPerspective()
        const Scalar f    = std::tan( ( PiDiv2 ) - ( m_fov * m_zoomFactor * .5_ra ) );
        const Scalar diff = m_zNear - m_zFar;

        m_projMatrix.setZero();

        m_projMatrix.coeffRef( 0, 0 ) = f / m_aspect;
        m_projMatrix.coeffRef( 1, 1 ) = f;
        m_projMatrix.coeffRef( 2, 2 ) = ( m_zFar + m_zNear ) / diff;
        m_projMatrix.coeffRef( 2, 3 ) = ( 2_ra * m_zFar * m_zNear ) / diff;
        m_projMatrix.coeffRef( 3, 2 ) = -1_ra;
    }
    break;

    default:
        break;
    }
}

Core::Ray Camera::getRayFromScreen( const Core::Vector2& pix ) const {
    // Ray starts from the camera's current position.
    return Core::Ray::Through( getPosition(), unProject( pix ) );
}

} // namespace Engine
} // namespace Ra
