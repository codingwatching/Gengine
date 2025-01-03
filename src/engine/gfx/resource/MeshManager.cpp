#include "../../PCH.h"
#include "MeshManager.h"
#include <engine/gfx/Renderer.h>

#include <algorithm>
#include <iostream>
#include <vector>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
#include <assimp/Importer.hpp>

namespace GFX::MeshManager
{
  namespace
  {
    static Assimp::Importer importer;
    static std::unordered_map<hashed_string, MeshID> handleMap_;

    static glm::mat4 AssimpToGlm(aiMatrix4x4 mat)
    {
      glm::mat4 m;
      for (int y = 0; y < 4; y++)
      {
        for (int x = 0; x < 4; x++)
        {
          m[x][y] = mat[y][x];
        }
      }
      return m;
    }

    static glm::vec3 AssimpToGlm(aiVector3D vec)
    {
      return glm::vec3(vec.x, vec.y, vec.z);
    }

    static glm::vec4 AssimpToGlm(aiColor4D vec)
    {
      return glm::vec4(vec.r, vec.g, vec.b, vec.a);
    }

    static glm::vec3 AssimpToGlm(aiColor3D vec)
    {
      return glm::vec3(vec.r, vec.g, vec.b);
    }

    static glm::quat AssimpToGlm(aiQuaternion quat)
    {
      glm::quat q;
      q.x = quat.x;
      q.y = quat.y;
      q.z = quat.z;
      q.w = quat.w;

      return q;
    }
  }



  void LoadMesh([[maybe_unused]] const aiScene* scene, aiMesh* mesh, std::vector<Index>& indices, std::vector<Vertex>& vertices)
  {
    for (unsigned i = 0; i < mesh->mNumVertices; i++)
    {
      Vertex vertex;

      glm::vec3 vector;
      vector.x = mesh->mVertices[i].x;
      vector.y = mesh->mVertices[i].y;
      vector.z = mesh->mVertices[i].z;
      vertex.position = vector;

      vector = glm::vec3();
      if (mesh->HasNormals())
      {
        vector.x = mesh->mNormals[i].x;
        vector.y = mesh->mNormals[i].y;
        vector.z = mesh->mNormals[i].z;
      }
      vertex.normal = vector;

      glm::vec2 vec = glm::vec2();
      if (mesh->HasTextureCoords(0))
      {
        vec.x = mesh->mTextureCoords[0][i].x;
        vec.y = mesh->mTextureCoords[0][i].y;
      }
      vertex.texCoord = vec;

      vertices.push_back(vertex);
    }

    for (unsigned i = 0; i < mesh->mNumFaces; i++)
    {
      aiFace& face = mesh->mFaces[i];
      for (unsigned j = 0; j < face.mNumIndices; j++)
      {
        indices.push_back(face.mIndices[j]);
      }
    }
  }

  MeshID CreateMeshBatched(std::string_view filename, hashed_string name)
  {
    const aiScene* scene = importer.ReadFile(filename.data(), aiProcessPreset_TargetRealtime_Fast);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
      std::string rre = importer.GetErrorString();
      std::cout << "ERROR::ASSIMP::" << rre << std::endl;
    }

    if (scene->HasAnimations())
    {
      printf("Animation loading not supported by this function.\n");
    }
    if (scene->mMeshes[0]->HasBones())
    {
      printf("Bone loading not supported by this function.\n");
    }

    std::vector<MeshID> meshHandles;

    if (scene->mNumMeshes == 0)
    {
      printf("File does not contain a mesh.\n");
    }
    if (scene->mNumMeshes > 1)
    {
      printf("Multiple mesh loading not supported.\n");
    }

    std::vector<Vertex> vertices;
    std::vector<Index> indices;
    LoadMesh(scene, scene->mMeshes[0], indices, vertices);

    handleMap_[name] = name;
    Renderer::AddBatchedMesh(GetMeshBatched(name), vertices, indices);
    return name;
  }

  MeshID GetMeshBatched(hashed_string name)
  {
    return handleMap_[name];
  }
}