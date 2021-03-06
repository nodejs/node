// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_SIMD_SHUFFLE_H_
#define V8_WASM_SIMD_SHUFFLE_H_

#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace wasm {

class V8_EXPORT_PRIVATE SimdShuffle {
 public:
  // Converts a shuffle into canonical form, meaning that the first lane index
  // is in the range [0 .. 15]. Set |inputs_equal| true if this is an explicit
  // swizzle. Returns canonicalized |shuffle|, |needs_swap|, and |is_swizzle|.
  // If |needs_swap| is true, inputs must be swapped. If |is_swizzle| is true,
  // the second input can be ignored.
  static void CanonicalizeShuffle(bool inputs_equal, uint8_t* shuffle,
                                  bool* needs_swap, bool* is_swizzle);

  // Tries to match an 8x16 byte shuffle to the identity shuffle, which is
  // [0 1 ... 15]. This should be called after canonicalizing the shuffle, so
  // the second identity shuffle, [16 17 .. 31] is converted to the first one.
  static bool TryMatchIdentity(const uint8_t* shuffle);

  // Tries to match a byte shuffle to a scalar splat operation. Returns the
  // index of the lane if successful.
  template <int LANES>
  static bool TryMatchSplat(const uint8_t* shuffle, int* index) {
    const int kBytesPerLane = kSimd128Size / LANES;
    // Get the first lane's worth of bytes and check that indices start at a
    // lane boundary and are consecutive.
    uint8_t lane0[kBytesPerLane];
    lane0[0] = shuffle[0];
    if (lane0[0] % kBytesPerLane != 0) return false;
    for (int i = 1; i < kBytesPerLane; ++i) {
      lane0[i] = shuffle[i];
      if (lane0[i] != lane0[0] + i) return false;
    }
    // Now check that the other lanes are identical to lane0.
    for (int i = 1; i < LANES; ++i) {
      for (int j = 0; j < kBytesPerLane; ++j) {
        if (lane0[j] != shuffle[i * kBytesPerLane + j]) return false;
      }
    }
    *index = lane0[0] / kBytesPerLane;
    return true;
  }

  // Tries to match a 32x4 rotate, only makes sense if the inputs are equal
  // (is_swizzle). A rotation is a shuffle like [1, 2, 3, 0]. This will always
  // match a Concat, but can have better codegen.
  static bool TryMatch32x4Rotate(const uint8_t* shuffle, uint8_t* shuffle32x4,
                                 bool is_swizzle);

  // Tries to match an 8x16 byte shuffle to an equivalent 32x4 shuffle. If
  // successful, it writes the 32x4 shuffle word indices. E.g.
  // [0 1 2 3 8 9 10 11 4 5 6 7 12 13 14 15] == [0 2 1 3]
  static bool TryMatch32x4Shuffle(const uint8_t* shuffle, uint8_t* shuffle32x4);

  // Tries to match an 8x16 byte shuffle to an equivalent 16x8 shuffle. If
  // successful, it writes the 16x8 shuffle word indices. E.g.
  // [0 1 8 9 2 3 10 11 4 5 12 13 6 7 14 15] == [0 4 1 5 2 6 3 7]
  static bool TryMatch16x8Shuffle(const uint8_t* shuffle, uint8_t* shuffle16x8);

  // Tries to match a byte shuffle to a concatenate operation, formed by taking
  // 16 bytes from the 32 byte concatenation of the inputs.  If successful, it
  // writes the byte offset. E.g. [4 5 6 7 .. 16 17 18 19] concatenates both
  // source vectors with offset 4. The shuffle should be canonicalized.
  static bool TryMatchConcat(const uint8_t* shuffle, uint8_t* offset);

  // Tries to match a byte shuffle to a blend operation, which is a shuffle
  // where no lanes change position. E.g. [0 9 2 11 .. 14 31] interleaves the
  // even lanes of the first source with the odd lanes of the second.  The
  // shuffle should be canonicalized.
  static bool TryMatchBlend(const uint8_t* shuffle);

  // Packs a 4 lane shuffle into a single imm8 suitable for use by pshufd,
  // pshuflw, and pshufhw.
  static uint8_t PackShuffle4(uint8_t* shuffle);
  // Gets an 8 bit lane mask suitable for 16x8 pblendw.
  static uint8_t PackBlend8(const uint8_t* shuffle16x8);
  // Gets an 8 bit lane mask suitable for 32x4 pblendw.
  static uint8_t PackBlend4(const uint8_t* shuffle32x4);
  // Packs 4 bytes of shuffle into a 32 bit immediate.
  static int32_t Pack4Lanes(const uint8_t* shuffle);
  // Packs 16 bytes of shuffle into an array of 4 uint32_t.
  static void Pack16Lanes(uint32_t* dst, const uint8_t* shuffle);
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_SIMD_SHUFFLE_H_
