// Copyright 2020 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file provides the Google-internal implementation of LowLevelHash.
//
// LowLevelHash is a fast hash function for hash tables, the fastest we've
// currently (late 2020) found that passes the SMHasher tests. The algorithm
// relies on intrinsic 128-bit multiplication for speed. This is not meant to be
// secure - just fast.
//
// It is closely based on a version of wyhash, but does not maintain or
// guarantee future compatibility with it.

#ifndef ABSL_HASH_INTERNAL_LOW_LEVEL_HASH_H_
#define ABSL_HASH_INTERNAL_LOW_LEVEL_HASH_H_

#include <stdint.h>
#include <stdlib.h>

#include "absl/base/config.h"
#include "absl/base/optimization.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

// Random data taken from the hexadecimal digits of Pi's fractional component.
// https://en.wikipedia.org/wiki/Nothing-up-my-sleeve_number
ABSL_CACHELINE_ALIGNED static constexpr uint64_t kStaticRandomData[] = {
    0x243f'6a88'85a3'08d3, 0x1319'8a2e'0370'7344, 0xa409'3822'299f'31d0,
    0x082e'fa98'ec4e'6c89, 0x4528'21e6'38d0'1377,
};

// Hash function for a byte array. A 64-bit seed and a set of five 64-bit
// integers are hashed into the result. The length must be greater than 32.
//
// To allow all hashable types (including string_view and Span) to depend on
// this algorithm, we keep the API low-level, with as few dependencies as
// possible.
uint64_t LowLevelHashLenGt32(const void* data, size_t len, uint64_t seed);

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_HASH_INTERNAL_LOW_LEVEL_HASH_H_
