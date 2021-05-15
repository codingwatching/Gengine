#pragma once
#include <Voxels/block.h>
#include <Voxels/light.h>
#include <mutex>
#include <CoreEngine/Shapes.h>
#include <concurrent_vector.h>
#include <Voxels/ChunkHelpers.h>
#include <Voxels/BlockStorage.h>
#include <Voxels/ChunkMesh.h>
#include <CoreEngine/GAssert.h>
#include <cereal/archives/binary.hpp>

// TODO: make these constexpr functions!
#define ID3D(x, y, z, h, w) (x + h * (y + w * z))
#define ID2D(x, y, w) (w * y + x)


//typedef std::pair<glm::ivec3, glm::ivec3> localpos;

/*
  0: -x-y+z
  1: +x-y+z
  2: +x-y-z
  3: -x-y-z
  4: -x+y+z
  5: +x+y+z
  6: +x+y-z
  7: -x+y-z
*/

class VoxelManager;

struct Chunk
{
private:
public:
  Chunk(const VoxelManager& vm) : pos_(0), mesh(this, vm) { ASSERT(0); }
  Chunk(const glm::ivec3& p, const VoxelManager& vm) : pos_(p), mesh(this, vm) {}
  ~Chunk() {};
  Chunk(const Chunk& other);
  Chunk& operator=(const Chunk& rhs);

  /*################################
            Global Chunk Info
  ################################*/
  static constexpr int CHUNK_SIZE       = 32;
  static constexpr int CHUNK_SIZE_SQRED = CHUNK_SIZE * CHUNK_SIZE;
  static constexpr int CHUNK_SIZE_CUBED = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
  static constexpr int CHUNK_SIZE_LOG2  = 5; // log2(32) = 5
  static constexpr int BLOCKS_PER_X = 1;
  static constexpr int BLOCKS_PER_Y = CHUNK_SIZE;
  static constexpr int BLOCKS_PER_Z = CHUNK_SIZE_SQRED;
  static inline const glm::ivec3 BLOCKS_PER_DIM{ BLOCKS_PER_X, BLOCKS_PER_Y, BLOCKS_PER_Z };

  /*################################
          Query Functions
  ################################*/
  inline const glm::ivec3& GetPos() const { return pos_; }

  Block BlockAt(const glm::ivec3& p) const;
  Block BlockAt(int index) const;
  BlockType BlockTypeAt(const glm::ivec3& p) const;
  BlockType BlockTypeAt(int index) const;
  void SetBlockTypeAt(const glm::ivec3& lpos, BlockType type);
  void SetLightAt(const glm::ivec3& lpos, Light light);
  Light LightAt(const glm::ivec3& p) const;
  Light LightAt(int index) const;

  void SetBlockTypeAtNoLock(const glm::ivec3& localPos, BlockType type);
  void SetLightAtNoLock(const glm::ivec3& localPos, Light light);

  inline void Lock()
  {
    mutex_.lock();
  }

  inline void Unlock()
  {
    mutex_.unlock();
  }

  void BuildMesh()
  {
    mesh.BuildMesh();
  }

  void BuildBuffers()
  {
    mesh.BuildBuffers();
  }

  ChunkMesh& GetMesh()
  {
    return mesh;
  }

  AABB GetAABB() const
  {
    return {
      glm::vec3(pos_ * CHUNK_SIZE),
      glm::vec3(pos_ * CHUNK_SIZE + CHUNK_SIZE) };
  }

  // Serialization
  template <class Archive>
  void serialize(Archive& ar)
  {
    ar(pos_, storage);
  }

  const auto& GetStorage() const { return storage; }

private:
  glm::ivec3 pos_;  // position relative to other chunks (1 chunk = 1 index)

  //ArrayBlockStorage<CHUNK_SIZE_CUBED> storage;
  PaletteBlockStorage<CHUNK_SIZE_CUBED> storage;
  ChunkMesh mesh;

  mutable std::shared_mutex mutex_;
};



inline Block Chunk::BlockAt(const glm::ivec3& p) const
{
  return storage.GetBlock(ID3D(p.x, p.y, p.z, CHUNK_SIZE, CHUNK_SIZE));
}

inline Block Chunk::BlockAt(int index) const
{
  return storage.GetBlock(index);
}

inline BlockType Chunk::BlockTypeAt(const glm::ivec3& p) const
{
  return storage.GetBlockType(ID3D(p.x, p.y, p.z, CHUNK_SIZE, CHUNK_SIZE));
}

inline BlockType Chunk::BlockTypeAt(int index) const
{
  return storage.GetBlockType(index);
}

inline void Chunk::SetBlockTypeAt(const glm::ivec3& lpos, BlockType type)
{
  int index = ID3D(lpos.x, lpos.y, lpos.z, CHUNK_SIZE, CHUNK_SIZE);
  storage.SetBlock(index, type);
}

inline void Chunk::SetLightAt(const glm::ivec3& lpos, Light light)
{
  int index = ID3D(lpos.x, lpos.y, lpos.z, CHUNK_SIZE, CHUNK_SIZE);
  storage.SetLight(index, light);
}

inline Light Chunk::LightAt(const glm::ivec3& p) const
{
  return storage.GetLight(ID3D(p.x, p.y, p.z, CHUNK_SIZE, CHUNK_SIZE));
}

inline Light Chunk::LightAt(int index) const
{
  return storage.GetLight(index);
}

inline void Chunk::SetBlockTypeAtNoLock(const glm::ivec3& localPos, BlockType type)
{
  int index = ID3D(localPos.x, localPos.y, localPos.z, CHUNK_SIZE, CHUNK_SIZE);
  storage.SetBlock(index, type);
}

inline void Chunk::SetLightAtNoLock(const glm::ivec3& localPos, Light light)
{
  int index = ID3D(localPos.x, localPos.y, localPos.z, CHUNK_SIZE, CHUNK_SIZE);
  storage.SetLight(index, light);
}
