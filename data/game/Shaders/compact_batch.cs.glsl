#version 460 core

#define VISIBILITY_NONE    0
#define VISIBILITY_PARTIAL 1
#define VISIBILITY_FULL    2

#include "indirect.h.glsl"

// only first 5 of 6 planes used- don't set 6th plane uniform
struct Frustum
{
  float data_[6][4];
};

struct AABB16
{
  vec4 min;
  vec4 max;
  // renderdoc thinks 8 xints of padding is here- that is not the case
};

struct VerticesDrawInfo
{
  uvec2 data01; // handle
  double data02;// allocation time
  uvec2 _pad01;
  uint offset;
  uint size;
  AABB16 box;
};

struct IndicesDrawInfo
{
  uvec2 data01; // handle
  double data02;// allocation time
  uvec2 _pad01;
  uint offset;
  uint size;
};

layout(std430, binding = 0) readonly buffer ssbo_0
{
  VerticesDrawInfo inDrawData[];
};

layout(std430, binding = 1) readonly buffer ssbo_1
{
  IndicesDrawInfo indicesInfos[];
};

layout(std430, binding = 2) readonly buffer ssbo_2
{
  uvec2 allocsIndices[];
};

layout(std430, binding = 3) writeonly buffer dib_3
{
  DrawElementsCommand outDrawCommands[];
};

// parameter buffer style
layout(std430, binding = 4) buffer parameterBuffer_4
{
  uint nextIdx;
};

// atomic style
//layout(binding = 0, offset = 0) uniform atomic_uint nextIdx;

uniform vec3 u_viewpos;
uniform Frustum u_viewfrustum;
uniform uint u_vertexSize = 8; // size of vertex in bytes
uniform uint u_indexSize = 4; // size of index in bytes
uniform float u_cullMinDist;
uniform float u_cullMaxDist;
uniform uint u_reservedVertices; // amt of reserved space (in vertices) before vertices for instanced attributes 

float GetDistance(in AABB16 box, in vec3 pos);
bool CullDistance(float dist, float minDist, float maxDist);
int CullFrustum(in AABB16 box, in Frustum frustum);

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
  uvec2 allocIndices = allocsIndices[gl_GlobalInvocationID.x];
  uint vertindex = allocIndices[0];
  uint indindex = allocIndices[1];

  if (gl_GlobalInvocationID.x >= allocsIndices.length())
    return;

  VerticesDrawInfo verticesAlloc = inDrawData[vertindex];
  IndicesDrawInfo indicesAlloc = indicesInfos[indindex];
#if 0
  bool condition = verticesAlloc.data01.xy != uvec2(0);
#else
  float dist = GetDistance(verticesAlloc.box, u_viewpos);
  bool condition = // all conditions must be true to draw chunk
    verticesAlloc.data01.xy != uvec2(0) &&
    CullDistance(dist, u_cullMinDist, u_cullMaxDist) &&
    CullFrustum(verticesAlloc.box, u_viewfrustum) >= VISIBILITY_PARTIAL &&
    verticesAlloc.size > u_vertexSize * u_reservedVertices;
#endif
  if (condition)
  {
    DrawElementsCommand cmd;
    //cmd.count = (verticesAlloc.size / u_vertexSize) - u_reservedVertices;
    cmd.count = indicesAlloc.size / u_indexSize; // sizeof(uint)
    cmd.instanceCount = 0;
    if (dist < 32)
      cmd.instanceCount = 1;
    cmd.firstIndex = indicesAlloc.offset / u_indexSize; // sizeof(uint)
    cmd.baseVertex = verticesAlloc.offset / u_vertexSize;
    cmd.baseInstance = cmd.baseVertex;

    uint insert = atomicAdd(nextIdx, 1);
    outDrawCommands[insert] = cmd;
  }
}


float GetDistance(in AABB16 box, in vec3 pos)
{
  vec3 bp = (box.max.xyz + box.min.xyz) / 2.0;
  float dist = distance(bp, pos);
  return dist;
}


bool CullDistance(float dist, float minDist, float maxDist)
{
  return dist >= minDist && dist <= maxDist;
}


vec4 GetPlane(int plane, in Frustum frustum)
{
  return vec4(frustum.data_[plane][0], frustum.data_[plane][1],
    frustum.data_[plane][2], frustum.data_[plane][3]);
}


int GetVisibility(in vec4 clip, in AABB16 box)
{
  float x0 = box.min.x * clip.x;
  float x1 = box.max.x * clip.x;
  float y0 = box.min.y * clip.y;
  float y1 = box.max.y * clip.y;
  float z0 = box.min.z * clip.z + clip.w;
  float z1 = box.max.z * clip.z + clip.w;
  float p1 = x0 + y0 + z0;
  float p2 = x1 + y0 + z0;
  float p3 = x1 + y1 + z0;
  float p4 = x0 + y1 + z0;
  float p5 = x0 + y0 + z1;
  float p6 = x1 + y0 + z1;
  float p7 = x1 + y1 + z1;
  float p8 = x0 + y1 + z1;

  if (p1 <= 0 && p2 <= 0 && p3 <= 0 && p4 <= 0 && p5 <= 0 && p6 <= 0 && p7 <= 0 && p8 <= 0)
  {
    return VISIBILITY_NONE;
  }
  if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0 && p5 > 0 && p6 > 0 && p7 > 0 && p8 > 0)
  {
    return VISIBILITY_FULL;
  }

  return VISIBILITY_PARTIAL;
}


int CullFrustum(in AABB16 box, in Frustum frustum)
{
  int v0 = GetVisibility(GetPlane(0, frustum), box);
  if (v0 == VISIBILITY_NONE)
  {
    return VISIBILITY_NONE;
  }

  int v1 = GetVisibility(GetPlane(1, frustum), box);
  if (v1 == VISIBILITY_NONE)
  {
    return VISIBILITY_NONE;
  }

  int v2 = GetVisibility(GetPlane(2, frustum), box);
  if (v2 == VISIBILITY_NONE)
  {
    return VISIBILITY_NONE;
  }

  int v3 = GetVisibility(GetPlane(3, frustum), box);
  if (v3 == VISIBILITY_NONE)
  {
    return VISIBILITY_NONE;
  }

  int v4 = GetVisibility(GetPlane(4, frustum), box);
  if (v4 == VISIBILITY_NONE)
  {
    return VISIBILITY_NONE;
  }

  if (v0 == VISIBILITY_FULL && v1 == VISIBILITY_FULL &&
    v2 == VISIBILITY_FULL && v3 == VISIBILITY_FULL &&
    v4 == VISIBILITY_FULL)
  {
    return VISIBILITY_FULL;
  }

  return VISIBILITY_PARTIAL;
}