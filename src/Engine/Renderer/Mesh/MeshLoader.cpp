#include <Engine/Renderer/Mesh/MeshLoader.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <Core/Math/Vector.hpp>
#include <Core/Math/Matrix.hpp>
#include <Core/Mesh/TriangleMesh.hpp>
#include <Core/Mesh/MeshUtils.hpp>
#include <Core/String/StringUtils.hpp>
#include <Engine/RadiumEngine.hpp>
#include <Engine/Entity/Entity.hpp>
#include <Engine/Renderer/Material/Material.hpp>
#include <Engine/Renderer/Shader/ShaderProgram.hpp>
#include <Engine/Renderer/Shader/ShaderProgramManager.hpp>
#include <Engine/Renderer/Mesh/Mesh.hpp>
#include <Engine/Renderer/Drawable/DrawableComponent.hpp>

namespace Ra
{

namespace
{
const Engine::ShaderConfiguration blinnPhongShaderConfiguration("BlinnPhong", "../Shaders");

void assimpToCore(const aiVector3D& inVector, Core::Vector3& outVector);
void assimpToCore(const aiColor4D& inColor, Core::Color& outColor);
void assimpToCore(const aiMatrix4x4& inMatrix, Core::Matrix4& outMatrix);

Core::Vector3 assimpToCore(const aiVector3D&  vector);
Core::Color   assimpToCore(const aiColor4D&   color);
Core::Matrix4 assimpToCore(const aiMatrix4x4& matrix);

void runThroughNodes(const aiNode* node, const aiScene* scene,
                     const Core::Matrix4& transform, Engine::RadiumEngine* engine);

void loadMesh(const aiMesh* mesh, Engine::DrawableComponent* component, const std::string& name);

void loadMaterial(const aiMaterial* mat, Engine::DrawableComponent* component, const std::string& name);
void loadDefaultMaterial(Engine::DrawableComponent* component, const std::string& name);

}

void Engine::MeshLoader::loadFile(const std::string& name, RadiumEngine* engine)
{
    fprintf(stderr, "Loading file \"%s\"...\n", name.c_str());

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(name,
                                             aiProcess_Triangulate |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_GenSmoothNormals |
                                             aiProcess_SortByPType |
                                             aiProcess_FixInfacingNormals |
                                             aiProcess_CalcTangentSpace |
                                             aiProcess_GenUVCoords);
    if (!scene)
    {
        fprintf(stderr, "Error while loading file \"%s\" :\n\t%s.\n",
                 name.c_str(), importer.GetErrorString());
        return;
    }

    runThroughNodes(scene->mRootNode, scene, Core::Matrix4::Identity(), engine);
}

namespace
{

void runThroughNodes(const aiNode* node, const aiScene* scene,
                     const Core::Matrix4& transform, Engine::RadiumEngine* engine)
{
    static int currentMesh = 0;

    if (node->mNumChildren == 0 && node->mNumMeshes == 0)
    {
        return;
    }

    Core::Matrix4 matrix = transform * assimpToCore(node->mTransformation);

    if (node->mNumMeshes > 0)
    {
        Engine::Entity* entity = engine->createEntity();
        Engine::DrawableComponent* component = new Engine::DrawableComponent;
        engine->addComponent(component, entity, "RenderSystem");

        entity->setTransform(matrix);

        for (uint i = 0; i < node->mNumMeshes; ++i)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            std::string name;
            Core::StringUtils::stringPrintf(name, "Mesh_%s_%u", node->mName.C_Str(), currentMesh++);

            loadMesh(mesh, component, name);

            if (scene->HasMaterials())
            {
                aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
                name.clear();
                Core::StringUtils::stringPrintf(name, "Material_%u", mesh->mMaterialIndex);

                loadMaterial(material, component, name);
            }
            else
            {
                name = "Material_default";
                loadDefaultMaterial(component, name);
            }
        }
    }

    for (uint i = 0; i < node->mNumChildren; ++i)
    {
        runThroughNodes(node->mChildren[i], scene, matrix, engine);
    }
}

void loadMesh(const aiMesh* mesh, Engine::DrawableComponent* component, const std::string& name)
{
    Core::TriangleMesh data;
    Engine::Mesh* emesh = new Engine::Mesh(name);

    Core::VectorArray<Core::Vector3> tangents;
    Core::VectorArray<Core::Vector3> bitangents;

    for (uint i = 0; i < mesh->mNumVertices; ++i)
    {
        data.m_vertices.push_back(assimpToCore(mesh->mVertices[i]));
        data.m_normals.push_back(assimpToCore(mesh->mNormals[i]));

        if (mesh->HasTangentsAndBitangents())
        {
            tangents.push_back(assimpToCore(mesh->mTangents[i]));
            bitangents.push_back(assimpToCore(mesh->mBitangents[i]));
        }
    }

    for (uint i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace f = mesh->mFaces[i];

        data.m_triangles.push_back(
                    Core::Triangle(f.mIndices[0], f.mIndices[1], f.mIndices[2]));
    }

    emesh->loadGeometry(data, tangents, bitangents);
    component->addDrawable(emesh);
}

void loadMaterial(const aiMaterial* mat, Engine::DrawableComponent* component, const std::string& name)
{
    if (mat == nullptr)
    {
        loadDefaultMaterial(component, name);
        return;
    }

    // TODO(Charly): Handle different shader programs
    // TODO(Charly): Handle transparency
    Engine::ShaderProgram* shader = Engine::ShaderProgramManager::getInstancePtr()->getShaderProgram(blinnPhongShaderConfiguration);

    Engine::Material* material = new Engine::Material(name);
    material->setShaderProgram(shader);

    aiColor4D color;
    if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color))
    {
        Core::Color c = assimpToCore(color);
        material->setKd(c);
        fprintf(stderr, "%s kd : %.3f %.3f %.3f\n", name.c_str(), c.x(), c.y(), c.z());
    }

    if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_SPECULAR, color))
    {
        Core::Color c = assimpToCore(color);
        material->setKs(c);
        fprintf(stderr, "%s ks : %.3f %.3f %.3f\n", name.c_str(), c[0], c[1], c[2]);
    }

    component->setMaterial(material);
}

void loadDefaultMaterial(Engine::DrawableComponent* component, const std::string& name)
{
    Engine::ShaderProgram* shader = Engine::ShaderProgramManager::getInstancePtr()->getShaderProgram(blinnPhongShaderConfiguration);
    Engine::Material* material = new Engine::Material(name);
    material->setShaderProgram(shader);
    component->setMaterial(material);
}

void assimpToCore(const aiVector3D& inVector, Core::Vector3& outVector)
{
    for (uint i = 0; i < 3; ++i)
    {
        outVector[i] = inVector[i];
    }
}

void assimpToCore(const aiColor4D& inColor, Core::Color& outColor)
{
    for (uint i = 0; i < 4; ++i)
    {
        outColor[i] = inColor[i];
    }
}

void assimpToCore(const aiMatrix4x4& inMatrix, Core::Matrix4& outMatrix)
{
    for (uint i = 0; i < 4; ++i)
    {
        for (uint j = 0; j < 4; ++j)
        {
            outMatrix(i, j) = inMatrix[i][j];
        }
    }

}

Core::Vector3 assimpToCore(const aiVector3D& vector)
{
    Core::Vector3 v;
    assimpToCore(vector, v);
    return v;
}

Core::Color assimpToCore(const aiColor4D& color)
{
    Core::Color c;
    assimpToCore(color, c);
    return c;
}

Core::Matrix4 assimpToCore(const aiMatrix4x4& matrix)
{
    Core::Matrix4 m;
    assimpToCore(matrix, m);
    return m;
}

} // namespace


} // namespace Ra
