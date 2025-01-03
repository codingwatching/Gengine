#pragma once
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace GFX
{
  namespace Constants
  {
    enum class CardinalNames : uint32_t
    {
      PosX,
      NegX,
      PosY,
      NegY,
      PosZ,
      NegZ
    };

    constexpr glm::vec3 Cardinals[6]
    {
      { 1, 0, 0 },
      { -1, 0, 0 },
      { 0, 1, 0 },
      { 0, -1, 0 },
      { 0, 0, 1 },
      { 0, 0, -1 }
    };

    inline const glm::vec3 Up = Cardinals[static_cast<uint32_t>(CardinalNames::PosY)];
  }

  struct View
  {
    glm::vec3 position{};
    float pitch{}; // pitch angle in radians
    float yaw{};   // yaw angle in radians
    //float roll{};  // roll angle in radians
    Constants::CardinalNames upDir = Constants::CardinalNames::PosY;

    glm::vec3 GetForwardDir() const;
    glm::mat4 GetViewMatrix() const;
    void SetForwardDir(glm::vec3 dir);
  };

  struct PerspectiveProjectionInfo
  {
    float fovyRadians{ 3.1415f };
    float aspectRatio{ 1 };
    float nearPlane{ 1 };
  };

  struct OrthographicProjectionInfo
  {
    // TODO
  };

  struct ProjectionInfo
  {
    PerspectiveProjectionInfo info;
    glm::mat4 GetProjMatrix() const;
  };

  struct Camera
  {
    View viewInfo{};
    ProjectionInfo projInfo{};

    //glm::mat4 GetProj() const { return projInfo.GetProjMatrix(); }
    //glm::mat4 GetView() const { return viewInfo.GetViewMatrix(); }
    glm::mat4 GetViewProj() const { return projInfo.GetProjMatrix() * viewInfo.GetViewMatrix(); }
  };
}