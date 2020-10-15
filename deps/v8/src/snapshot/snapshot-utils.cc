// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/snapshot-utils.h"

#include "src/sanitizer/msan.h"
#include "third_party/zlib/zlib.h"

namespace v8 {
namespace internal {

uint32_t Checksum(Vector<const byte> payload) {
#ifdef MEMORY_SANITIZER
  // Computing the checksum includes padding bytes for objects like strings.
  // Mark every object as initialized in the code serializer.
  MSAN_MEMORY_IS_INITIALIZED(payload.begin(), payload.length());
#endif  // MEMORY_SANITIZER
  // Priming the adler32 call so it can see what CPU features are available.
  adler32(0, nullptr, 0);
  return static_cast<uint32_t>(adler32(0, payload.begin(), payload.length()));
}

V8_EXPORT_PRIVATE uint32_t Checksum(Vector<const byte> payload1,
                                    Vector<const byte> payload2) {
#ifdef MEMORY_SANITIZER
  // Computing the checksum includes padding bytes for objects like strings.
  // Mark every object as initialized in the code serializer.
  MSAN_MEMORY_IS_INITIALIZED(payload1.begin(), payload1.length());
  MSAN_MEMORY_IS_INITIALIZED(payload2.begin(), payload2.length());
#endif  // MEMORY_SANITIZER
  // Priming the adler32 call so it can see what CPU features are available.
  adler32(0, nullptr, 0);
  auto sum = adler32(0, payload1.begin(), payload1.length());
  sum = adler32(sum, payload2.begin(), payload2.length());
  return static_cast<uint32_t>(sum);
}

}  // namespace internal
}  // namespace v8
