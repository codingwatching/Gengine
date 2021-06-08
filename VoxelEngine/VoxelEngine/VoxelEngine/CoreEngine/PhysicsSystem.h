#pragma once
#include <CoreEngine/Scene.h>

class PhysicsSystem
{
public:
  PhysicsSystem();
  ~PhysicsSystem();
  void InitScene(Scene& scene);
  void Update(Scene& scene, float dt);

private:
};