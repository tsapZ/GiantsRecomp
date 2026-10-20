#pragma once

// A plain LZMA (LZMA1) decoder for the compressed blocks of the game's .arc / .bld archives: each
// block is 5 property bytes followed by a raw LZMA stream without end marker, and its unpacked
// size is known from the archive. Written from the format as described in the public-domain LZMA
// SDK (DOC/lzma-specification.txt); one block is at most 32 KB. Taken from SwapForceRecomp
// (src/game/alchemy/lzma_decoder.h there).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace giantsrecomp::alchemy {

// Decodes `size` bytes (props + stream) into exactly `unpacked_size` bytes; nullopt if the data is
// corrupt or ends early.
std::optional<std::vector<uint8_t>> LzmaDecode(const uint8_t* data, size_t size,
                                               size_t unpacked_size);

}  // namespace giantsrecomp::alchemy
