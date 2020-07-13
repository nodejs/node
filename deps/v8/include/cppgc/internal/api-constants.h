// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_API_CONSTANTS_H_
#define INCLUDE_CPPGC_INTERNAL_API_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

// Embedders should not rely on this code!

// Internal constants to avoid exposing internal types on the API surface.
namespace api_constants {
// Offset of the uint16_t bitfield from the payload contaning the
// in-construction bit. This is subtracted from the payload pointer to get
// to the right bitfield.
static constexpr size_t kFullyConstructedBitFieldOffsetFromPayload =
    2 * sizeof(uint16_t);
// Mask for in-construction bit.
static constexpr size_t kFullyConstructedBitMask = size_t{1};

// Page constants used to align pointers to page begin.
static constexpr size_t kPageSize = size_t{1} << 17;
static constexpr size_t kPageAlignment = kPageSize;
static constexpr size_t kPageBaseMask = ~(kPageAlignment - 1);
static constexpr size_t kGuardPageSize = 4096;

// Offset of the Heap backref.
static constexpr size_t kHeapOffset = 0;

static constexpr size_t kLargeObjectSizeThreshold = kPageSize / 2;

}  // namespace api_constants

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_API_CONSTANTS_H_
