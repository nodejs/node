// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/snapshot-utils.h"

#include "src/base/sanitizer/msan.h"

#ifdef V8_USE_ZLIB
#include "third_party/zlib/zlib.h"
#endif

namespace v8 {
namespace internal {

uint32_t Checksum(base::Vector<const uint8_t> payload) {
#ifdef MEMORY_SANITIZER
  // Computing the checksum includes padding bytes for objects like strings.
  // Mark every object as initialized in the code serializer.
  MSAN_MEMORY_IS_INITIALIZED(payload.begin(), payload.length());
#endif  // MEMORY_SANITIZER

#ifdef V8_USE_ZLIB
  // Priming the adler32 call so it can see what CPU features are available.
  adler32(0, nullptr, 0);
  return static_cast<uint32_t>(adler32(0, payload.begin(), payload.length()));
#else
  // Simple Fletcher-32.
  uint32_t sum1 = 0, sum2 = 0;
  for (auto data : payload) {
    sum1 = (sum1 + data) % 65535;
    sum2 = (sum2 + sum1) % 65535;
  }
  return (sum2 << 16 | sum1);
#endif
}

}  // namespace internal
}  // namespace v8
