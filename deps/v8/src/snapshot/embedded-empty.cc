// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building without embedded data.

#include <cstdint>

namespace v8 {
namespace internal {

#ifdef V8_EMBEDDED_BUILTINS
const uint8_t* DefaultEmbeddedBlob() { return nullptr; }
uint32_t DefaultEmbeddedBlobSize() { return 0; }

#ifdef V8_MULTI_SNAPSHOTS
const uint8_t* TrustedEmbeddedBlob() { return nullptr; }
uint32_t TrustedEmbeddedBlobSize() { return 0; }
#endif

#endif

}  // namespace internal
}  // namespace v8
