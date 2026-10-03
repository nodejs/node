#include "zstd_blob.h"

#include <cstdlib>

#include "zstd.h"

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace node {

uint8_t* ZstdDecompressAligned(const uint8_t* src,
                               size_t src_size,
                               size_t* raw_size) {
  uint64_t content = ZSTD_getFrameContentSize(src, src_size);
  if (content == ZSTD_CONTENTSIZE_ERROR ||
      content == ZSTD_CONTENTSIZE_UNKNOWN || content == 0) {
    return nullptr;
  }
  size_t size = static_cast<size_t>(content);
  size_t alloc = (size + 15u) & ~size_t{15};
  void* buf = nullptr;
#if defined(_WIN32)
  buf = _aligned_malloc(alloc, 16);
#else
  if (posix_memalign(&buf, 16, alloc) != 0) {
    buf = nullptr;
  }
#endif
  if (buf == nullptr) {
    return nullptr;
  }
  size_t got = ZSTD_decompress(buf, size, src, src_size);
  if (ZSTD_isError(got) || got != size) {
#if defined(_WIN32)
    _aligned_free(buf);
#else
    free(buf);
#endif
    return nullptr;
  }
  *raw_size = size;
  return static_cast<uint8_t*>(buf);
}

}  // namespace node
