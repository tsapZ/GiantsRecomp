#pragma once

// Xbox 360 (Xenos) DXT textures to RGBA, for the game's own pictures (toy_portraits.cpp). Taken
// from SwapForceRecomp's IGZ texture reader (src/game/alchemy/igz_file.cpp there), via SSARecomp.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace giantsrecomp {

enum class Dxt { kDxt1, kDxt3, kDxt5 };

struct RgbaImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;  // width * height * 4
};

// Decodes `width` x `height` texels of DXT data stored as the GPU reads it: 16-bit words
// big-endian, tiled (or linear, rows padded to 32 blocks). nullopt if `size` is too small.
std::optional<RgbaImage> DecodeXenosDxt(const uint8_t* data, size_t size, int width, int height,
                                        Dxt kind, bool tiled);

}  // namespace giantsrecomp
