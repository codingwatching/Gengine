#include "../../PCH.h"
#include "../../Scene.h"
#include <glad/glad.h>
#include "GraphicsSystem.h"
#include <engine/Input.h>
#include <engine/Application.h>
#include <engine/Camera.h>
#include <engine/gfx/Renderer.h>
#include <engine/gfx/Mesh.h>
#include <execution>
#include <glm/gtx/norm.hpp>

#include <imgui/imgui.h>

// TODO: TEMP GARBAGE
#include <engine/Context.h>

#include "../component/Rendering.h"
#include "../component/Camera.h"
#include "../component/ParticleEmitter.h"

void GraphicsSystem::Init()
{
  window = init_glfw_context();
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  CameraSystem::Init();
  Renderer::Init();
}

void GraphicsSystem::Shutdown()
{
  glfwDestroyWindow(window);
}

void GraphicsSystem::StartFrame()
{
  Renderer::StartFrame();

  ImGui::Begin("Graphics");
  ImGui::Text("Loaded Meshes");
  for (const auto& [id, info] : MeshManager::handleMap_)
  {
    ImGui::Text("%s, ID: %u", id.data(), id.value());
  }
  ImGui::Separator();
  ImGui::Text("Loaded Materials");
  for (const auto& [id, info] : MaterialManager::Get()->materials_)
  {
    ImGui::Text("ID: %u", id);
  }
  ImGui::End();

  const auto& camList = CameraSystem::GetCameraList();
  for (Component::Camera* camera : camList)
  {
    if (!camera->skyboxTexture)
    {
      continue;
    }
    CameraSystem::ActiveCamera = camera;
    CameraSystem::Update(0);
    Renderer::DrawSkybox();
  }
}

void GraphicsSystem::DrawOpaque(Scene& scene, float dt)
{
  const auto& camList = CameraSystem::GetCameraList();
  for (Component::Camera* camera : camList)
  {
    CameraSystem::ActiveCamera = camera;
    CameraSystem::Update(dt);

    // draw batched objects in the scene
    using namespace Component;
    auto group = scene.GetRegistry().group<BatchedMesh>(entt::get<Transform, Material>);
    Renderer::BeginBatch(group.size());
    std::for_each(std::execution::par, group.begin(), group.end(),
      [&group](entt::entity entity)
      {
        auto [mesh, transform, material] = group.get<BatchedMesh, Transform, Material>(entity);
        if ((CameraSystem::ActiveCamera->cullingMask & mesh.renderFlag) != mesh.renderFlag) // Mesh not set to be culled
        {
          Renderer::Submit(transform, mesh, material);
        }
      });

    Renderer::RenderBatch();
  }
}

void GraphicsSystem::DrawTransparent(Scene& scene, [[maybe_unused]] float dt)
{
  const auto& camList = CameraSystem::GetCameraList();
  for (Component::Camera* camera : camList)
  {
    CameraSystem::ActiveCamera = camera;

    // draw particles from back to front
    using namespace Component;
    auto view = scene.GetRegistry().view<ParticleEmitter, Transform>();
    auto compare = [&camera](const auto& p1, const auto& p2)
    {
      if (p1.second->GetTranslation() != p2.second->GetTranslation())
      {
        auto len = glm::length2(p1.second->GetTranslation() - camera->GetWorldPos()) -
          glm::length2(p2.second->GetTranslation() - camera->GetWorldPos());
        if (glm::abs(len) > .001f)
        {
          return len > 0.0f;
        }
      }
      return p1.first < p2.first;
    };
    std::vector<std::pair<ParticleEmitter*, Transform*>> emitters;
    for (auto entity : view)
    {
      auto [emitter, transform] = view.get<ParticleEmitter, Transform>(entity);
      if ((CameraSystem::ActiveCamera->cullingMask & emitter.renderFlag) != emitter.renderFlag) // Emitter not set to be culled
      {
        emitters.push_back(std::pair<ParticleEmitter*, Transform*>(&emitter, &transform));
      }
    }
    std::sort(emitters.begin(), emitters.end(), compare);
    Renderer::BeginRenderParticleEmitter();
    for (const auto& [emitter, transform] : emitters)
    {
      Renderer::RenderParticleEmitter(*emitter, *transform);
    }
  }
}

void GraphicsSystem::EndFrame(float dt)
{
  Renderer::EndFrame(dt);
}

void GraphicsSystem::SwapBuffers()
{
  glfwSwapBuffers(window);
}