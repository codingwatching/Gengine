#pragma once
#include "../Scene.h"
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <engine/GAssert.h>

// "lightweight object"
class Entity
{
public:
  Entity() = default;
  Entity(entt::entity handle, Scene* scene)
    : entityHandle_(handle), scene_(scene)
  {
  }

  Entity(const Entity& other) = default;

  void Destroy();

  template<typename T, typename... Args>
  T& AddComponent(Args&&... args)
  {
    ASSERT_MSG(!HasComponent<T>(), "Entity already has component!");
    return scene_->GetRegistry().emplace<T>(entityHandle_, std::forward<Args>(args)...);
  }

  template<typename T, typename... Args>
  T& GetOrAddComponent(Args&&... args)
  {
    return scene_->GetRegistry().get_or_emplace<T>(entityHandle_, std::forward<Args>(args)...);
  }

  template<typename T>
  [[nodiscard]] T& GetComponent()
  {
    ASSERT_MSG(HasComponent<T>(), "Entity missing component!");
    return scene_->GetRegistry().get<T>(entityHandle_);
  }

  template<typename T>
  [[nodiscard]] const T& GetComponent() const
  {
    ASSERT_MSG(HasComponent<T>(), "Entity missing component!");
    return scene_->GetRegistry().get<T>(entityHandle_);
  }

  template<typename T>
  [[nodiscard]] T* TryGetComponent()
  {
    return scene_->GetRegistry().try_get<T>(entityHandle_);
  }

  template<typename T>
  [[nodiscard]] T* TryGetComponent() const
  {
    return scene_->GetRegistry().try_get<T>(entityHandle_);
  }

  template<typename T>
  [[nodiscard]] bool HasComponent() const
  {
    return scene_->GetRegistry().has<T>(entityHandle_);
  }

  template<typename T>
  void RemoveComponent()
  {
    ASSERT_MSG(HasComponent<T>(), "Entity missing component!");
    scene_->GetRegistry().remove<T>(entityHandle_);
  }

  operator bool() const { return entityHandle_ != entt::null; }
  operator uint32_t() const { return static_cast<uint32_t>(entityHandle_); }
  operator entt::entity() const { return entityHandle_; }


  [[nodiscard]] bool operator==(const Entity& other) const
  {
    return entityHandle_ == other.entityHandle_ && scene_ == other.scene_;
  }

  [[nodiscard]] bool operator!=(const Entity& other) const
  {
    return !(*this == other);
  }

  // sets the parent entity of this entity
  void SetParent(Entity parent);
  // adds a child entity of this entity
  void AddChild(Entity child);

  [[nodiscard]] unsigned GetHierarchyHeight() const;

private:
  Scene* scene_ = nullptr;
  entt::entity entityHandle_ = entt::null;
  friend class ScriptableEntity;
};