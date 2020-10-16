#pragma once
#include <span>

class TextureArray
{
public:
  TextureArray(std::span<std::string> textures, glm::ivec2 xyDim);
  ~TextureArray();

  void Bind(GLuint slot = 0) const;

private:

  GLuint rendererID_ = 0;
  glm::ivec2 dim{};
  static inline const std::string texPath = "./Resources/Textures/";
};