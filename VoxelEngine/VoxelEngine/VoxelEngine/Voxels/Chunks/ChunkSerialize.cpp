#include "ChunkSerialize.h"
#include <zlib.h>
#include <cereal/cereal.hpp>
#include <Utilities/DeltaEncoder.h>
#include <Utilities/RunLengthEncoder.h>
#include <Utilities/CompressBuffer.h>
#include <Utilities/serialize.h>

#include <cereal/types/vector.hpp>
#include <cereal/archives/binary.hpp>
#include <sstream>


template<typename T>
struct CompressedMaterialInfo
{
  CompressedMaterialInfo(Palette<T, Chunk::CHUNK_SIZE_CUBED> p) :
    indices(Chunk::CHUNK_SIZE_CUBED / 8, UINT16_MAX),
    bitmasks(Chunk::CHUNK_SIZE_CUBED / 8, 0),
    palette(p) {}

  std::vector<uint16_t> indices;
  std::vector<uint8_t> bitmasks;
  Palette<T, Chunk::CHUNK_SIZE_CUBED> palette;

  void MakeIndicesAndBitmasks(const T& emptyVal)
  {
    // records positions of blocks in the chunk
    // indices represent start of sequence of 8 blocks, at least one of which is NOT empty
    // bitmasks represent which blocks in an 8-block sequence is NOT empty
    for (int i = 0; i < Chunk::CHUNK_SIZE_CUBED; i++)
    {
      if (palette.GetVal(i) != emptyVal)
      {
        indices[i / 8] = i - (i % 8);
        bitmasks[i / 8] |= 1 << (i % 8);
      }
    }
    std::erase(indices, UINT16_MAX);
    std::erase(bitmasks, 0);
  }

  void RemoveEmptyPaletteData(const T& emptyVal)
  {
    // get indices of empty entries in palettes
    int indexLen = palette.paletteEntryLength_;
    int emptyIndex = std::find(palette.palette_.begin(), palette.palette_.end(), emptyVal) - palette.palette_.begin();

    // remove empty entries from palettes and from data
    const int toRemove = palette.palette_[emptyIndex].refcount;
    palette.palette_.erase(palette.palette_.begin() + emptyIndex);
    auto prevSize = palette.data_.size();
    palette.data_ = palette.data_.FindAll(indexLen, [emptyIndex](auto n) { return n != emptyIndex; });
    auto remdb = (prevSize - palette.data_.size()) / indexLen;
    ASSERT(remdb == toRemove);
  }
};

CompressedChunkData CompressChunk(PaletteBlockStorage<Chunk::CHUNK_SIZE_CUBED> data)
{
  auto blocks = data.pblock_;
  auto lights = data.plight_;

  CompressedMaterialInfo<BlockType> blockData(blocks);
  CompressedMaterialInfo<Light> lightData(lights);
  blockData.MakeIndicesAndBitmasks(BlockType::bAir);
  blockData.RemoveEmptyPaletteData(BlockType::bAir);
  lightData.MakeIndicesAndBitmasks(Light{});
  lightData.RemoveEmptyPaletteData(Light{});
  auto bytesA = blockData.palette.GetData().ByteRepresentation();

  auto deltaA = Compression::EncodeDelta(std::span(blockData.indices.data(), blockData.indices.size()));
  auto deltaB = Compression::EncodeDelta(std::span(lightData.indices.data(), lightData.indices.size()));

  auto rleA = Compression::EncodeRLE(std::span(deltaA.data(), deltaA.size()));
  auto rleB = Compression::EncodeRLE(std::span(deltaB.data(), deltaB.size()));

  auto compressedA = Compression::Compress(std::span(rleA.data(), rleA.size()));
  auto compressedB = Compression::Compress(std::span(rleB.data(), rleB.size()));

#if 1
  // tests
  auto bitsA = BitArray(bytesA);
  ASSERT(bitsA == blockData.palette.GetData());
  auto ddataA = Compression::DecodeDelta(std::span(deltaA.data(), deltaA.size()));
  ASSERT(ddataA == blockData.indices);
  auto rdataA = Compression::DecodeRLE(std::span(rleA.data(), rleA.size()));
  ASSERT(deltaA == rdataA);
  auto uncompressA = Compression::Uncompress(compressedA);
  ASSERT(uncompressA == rleA);
#endif

  std::stringstream binaryData;
  cereal::BinaryOutputArchive archive(binaryData);
  archive(compressedA, compressedB);

  CompressedChunkData ret;
  return ret;
}
