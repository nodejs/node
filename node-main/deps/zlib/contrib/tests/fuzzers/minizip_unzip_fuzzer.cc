// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "unzip.h"

// Fuzzer builds often have NDEBUG set, so roll our own assert macro.
#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d Assert failed: %s\n", __FILE__, __LINE__, #cond); \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Mock read-only filesystem with only one file, file_data. In the calls
  // below, 'opaque' points to file_data, and 'strm' points to the file's seek
  // position, which is heap allocated so that failing to "close" it triggers a
  // leak error.
  std::vector<uint8_t> file_data(data, data + size);
  zlib_filefunc64_def file_func = {
      .zopen64_file = [](void* opaque, const void* filename,
                         int mode) -> void* {
        ASSERT(mode == (ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_EXISTING));
        return new size_t(0);
      },
      .zread_file = [](void* opaque, void* strm, void* buf,
                       uLong size) -> uLong {
        std::vector<uint8_t>* vec = static_cast<std::vector<uint8_t>*>(opaque);
        size_t* pos = static_cast<size_t*>(strm);
        if (*pos >= vec->size()) {
          return 0;
        }
        size = std::min(static_cast<size_t>(size), vec->size() - *pos);
        memcpy(buf, vec->data() + *pos, size);
        (*pos) += size;
        return size;
      },
      .zwrite_file = [](void*, void*, const void*, uLong) -> uLong {
        ASSERT(0 && "Writing is not supported.");
        return 0;
      },
      .ztell64_file = [](void*, void* strm) -> ZPOS64_T {
        return *static_cast<size_t*>(strm);
      },
      .zseek64_file = [](void* opaque, void* strm, ZPOS64_T offset,
                         int origin) -> long {
        std::vector<uint8_t>* vec = static_cast<std::vector<uint8_t>*>(opaque);
        size_t* pos = static_cast<size_t*>(strm);
        switch (origin) {
          case ZLIB_FILEFUNC_SEEK_SET:
            *pos = offset;
            break;
          case ZLIB_FILEFUNC_SEEK_CUR:
            *pos = *pos + offset;
            break;
          case ZLIB_FILEFUNC_SEEK_END:
            *pos = vec->size() + offset;
            break;
          default:
            ASSERT(0 && "Invalid origin");
        }
        return 0;
      },
      .zclose_file = [](void*, void* strm) -> int {
        delete static_cast<size_t*>(strm);
        return 0;
      },
      .zerror_file = [](void*, void*) -> int { return 0; },
      .opaque = &file_data};

  unzFile uzf = unzOpen2_64("foo.zip", &file_func);
  if (uzf == NULL) {
    return 0;
  }

  while (true) {
    unz_file_info64 info = {0};

    // TODO: Pass nullptrs and different buffer sizes to cover more code.
    char filename[UINT16_MAX + 1];  // +1 for the null terminator.
    char extra[UINT16_MAX];         // No null terminator.
    char comment[UINT16_MAX + 1];   // +1 for the null terminator.

    if (unzGetCurrentFileInfo64(uzf, &info, filename, sizeof(filename), extra,
                                sizeof(extra), comment, sizeof(comment)) == UNZ_OK) {
      ASSERT(info.size_filename <= UINT16_MAX);
      ASSERT(info.size_file_extra <= UINT16_MAX);
      ASSERT(info.size_file_comment <= UINT16_MAX);

      ASSERT(filename[info.size_filename] == '\0');
      ASSERT(comment[info.size_file_comment] == '\0');
    }

    if (unzOpenCurrentFile(uzf) == UNZ_OK) {
      while (true) {
        char buffer[4096];
        int num_read = unzReadCurrentFile(uzf, buffer, sizeof(buffer));
        if (num_read <= 0) {
          break;
        }
      }
      unzCloseCurrentFile(uzf);
    }

    if (unzGoToNextFile(uzf) != UNZ_OK) {
      break;
    }
  }

  unzClose(uzf);
  return 0;
}
