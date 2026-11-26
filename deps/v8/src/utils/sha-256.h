// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_SHA_256_H_
#define V8_UTILS_SHA_256_H_

#include <stddef.h>
#include <stdint.h>

#include "src/base/macros.h"

const size_t kSizeOfSha256Digest = 32;
const size_t kSizeOfFormattedSha256Digest = (kSizeOfSha256Digest * 2) + 1;

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE Sha256Stream {
 public:
  Sha256Stream();
  ~Sha256Stream() = default;
  Sha256Stream(const Sha256Stream&) = delete;
  Sha256Stream& operator=(const Sha256Stream&) = delete;

  void Update(const void* data, size_t len);
  void Final(uint8_t digest[kSizeOfSha256Digest]);

 private:
  struct Impl;
  Impl* impl() { return reinterpret_cast<Impl*>(storage_); }

  static constexpr size_t kStorageAlignment = alignof(uint64_t);
  alignas(kStorageAlignment) char storage_[112];
#ifdef DEBUG
  bool finalized_ = false;
#endif
};

// Convenience method. Returns digest address.
V8_EXPORT_PRIVATE const uint8_t* Sha256Hash(const void* data, size_t len,
                                            uint8_t* digest);

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_SHA_256_H_
