#ifndef SRC_ZSTD_BLOB_H_
#define SRC_ZSTD_BLOB_H_

#include <cstddef>
#include <cstdint>

namespace node {

// Decompress one zstd frame into a 16-byte-aligned buffer. The returned
// pointer is process-lifetime storage; callers must not free it. `*raw_size`
// is the decompressed size. Returns nullptr on failure.
uint8_t* ZstdDecompressAligned(const uint8_t* src,
                               size_t src_size,
                               size_t* raw_size);

}  // namespace node

#endif  // SRC_ZSTD_BLOB_H_
