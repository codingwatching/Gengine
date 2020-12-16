#pragma once
#include <ostream>
#include <glm/glm.hpp>
#include "Flags.h"

namespace Utils
{
  struct djb2hash
  {
    size_t operator()(const char* cp) const
    {
      size_t hash = 5381;
      while (*cp)
        hash = 33 * hash ^ (unsigned char)*cp++;
      return hash;
    }
  };


  struct charPtrKeyEq
  {
    bool operator()(const char* first, const char* second) const
    {
      return !strcmp(first, second);
    }
  };


  struct ivec3Hash
  {
    // condenses an ivec3 into a singular number that can be used in a hash
    size_t operator()(const glm::ivec3& vec) const
    {
      //return ID3D(vec.x, vec.y, vec.z, Chunk::GetChunkSize(), Chunk::GetChunkSize());
      using std::size_t;
      using std::hash;

      return ((vec.x * 5209) ^ (vec.y * 1811)) ^ (vec.z * 7297);
    }
  };


  struct ivec3KeyEq
  {
    bool operator()(const glm::ivec3& first, const glm::ivec3& second) const
    {
      return first == second;
    }
  };



  /**
   * Converts an RGB color value to HSL. Conversion formula
   * adapted from http://en.wikipedia.org/wiki/HSL_color_space.
   * Assumes r, g, and b are contained in the set [0, 255] and
   * returns h, s, and l in the set [0, 1].
   *
   * @param   {number}  r       The red color value
   * @param   {number}  g       The green color value
   * @param   {number}  b       The blue color value
   * @return  {Array}           The HSL representation
   */
  inline glm::vec3 RGBtoHSL(glm::vec3 rgb)
  {
    rgb.r /= 255.f;
    rgb.g /= 255.f;
    rgb.b /= 255.f;
    float max = glm::max(rgb.r, glm::max(rgb.g, rgb.b));
    float min = glm::min(rgb.r, glm::min(rgb.g, rgb.b));

    float h{};
    float s{};
    float l = (max + min) / 2.f;

    if (max == min)
    {
      h = s = 0; // achromatic
    }
    else
    {
      float d = max - min;
      s = l > 0.5f ? d / (2.f - max - min) : d / (max + min);

      if (max == rgb.r)
        h = (rgb.g - rgb.b) / d + (rgb.g < rgb.b ? 6.f : 0.f);
      else if (max == rgb.g)
        h = (rgb.b - rgb.r) / d + 2.f;
      else if (max == rgb.b)
        h = (rgb.r - rgb.g) / d + 4.f;

      h /= 6.f;
    }

    return glm::vec3(h, s, l);
  }

  namespace detail
  {
    // helper for HSLtoRGB
    inline float hue2rgb(float p, float q, float t)
    {
      if (t < 0) t += 1;
      if (t > 1) t -= 1;
      if (t < 1.f / 6.f) return p + (q - p) * 6.f * t;
      if (t < 1.f / 2.f) return q;
      if (t < 2.f / 3.f) return p + (q - p) * (2.f / 3.f - t) * 6.f;
      return p;
    }
  }

  /**
   * Converts an HSL color value to RGB. Conversion formula
   * adapted from http://en.wikipedia.org/wiki/HSL_color_space.
   * Assumes h, s, and l are contained in the set [0, 1] and
   * returns r, g, and b in the set [0, 255].
   *
   * @param   {number}  h       The hue
   * @param   {number}  s       The saturation
   * @param   {number}  l       The lightness
   * @return  {Array}           The RGB representation
   */
  inline glm::vec3 HSLtoRGB(const glm::vec3& hsl)
  {
    glm::vec3 rgb;

    if (hsl.y == 0)
    {
      rgb.r = rgb.g = rgb.b = hsl.z; // achromatic
    }
    else
    {
      float q = hsl.z < 0.5f ? hsl.z * (1 + hsl.y) : hsl.z + hsl.y - hsl.z * hsl.y;
      float p = 2 * hsl.z - q;
      rgb.r = detail::hue2rgb(p, q, hsl.x + 1.f / 3.f);
      rgb.g = detail::hue2rgb(p, q, hsl.x);
      rgb.b = detail::hue2rgb(p, q, hsl.x - 1.f / 3.f);
    }

    return glm::vec3(rgb.r * 255, rgb.g * 255, rgb.b * 255);
  }

  inline glm::mat4 MakeInfReversedZProjRH(float fovY_radians, float aspectWbyH, float zNear)
  {
    float f = 1.0f / tan(fovY_radians / 2.0f);
    return glm::mat4(
      f / aspectWbyH, 0.0f, 0.0f, 0.0f,
      0.0f, f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, -1.0f,
      0.0f, 0.0f, zNear, 0.0f);
  }


}


static std::ostream& operator<<(std::ostream& o, glm::ivec3 v)
{
  return o << '('
    << v.x << ", "
    << v.y << ", "
    << v.z << ')';
}