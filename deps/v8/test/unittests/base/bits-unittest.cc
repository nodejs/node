// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "testing/gtest-support.h"

#ifdef DEBUG
#define DISABLE_IN_RELEASE(Name) Name
#else
#define DISABLE_IN_RELEASE(Name) DISABLED_##Name
#endif

namespace v8 {
namespace base {
namespace bits {

TEST(Bits, CountPopulation32) {
  EXPECT_EQ(0u, CountPopulation32(0));
  EXPECT_EQ(1u, CountPopulation32(1));
  EXPECT_EQ(8u, CountPopulation32(0x11111111));
  EXPECT_EQ(16u, CountPopulation32(0xf0f0f0f0));
  EXPECT_EQ(24u, CountPopulation32(0xfff0f0ff));
  EXPECT_EQ(32u, CountPopulation32(0xffffffff));
}


TEST(Bits, CountPopulation64) {
  EXPECT_EQ(0u, CountPopulation64(0));
  EXPECT_EQ(1u, CountPopulation64(1));
  EXPECT_EQ(2u, CountPopulation64(0x8000000000000001));
  EXPECT_EQ(8u, CountPopulation64(0x11111111));
  EXPECT_EQ(16u, CountPopulation64(0xf0f0f0f0));
  EXPECT_EQ(24u, CountPopulation64(0xfff0f0ff));
  EXPECT_EQ(32u, CountPopulation64(0xffffffff));
  EXPECT_EQ(16u, CountPopulation64(0x1111111111111111));
  EXPECT_EQ(32u, CountPopulation64(0xf0f0f0f0f0f0f0f0));
  EXPECT_EQ(48u, CountPopulation64(0xfff0f0fffff0f0ff));
  EXPECT_EQ(64u, CountPopulation64(0xffffffffffffffff));
}


TEST(Bits, CountLeadingZeros32) {
  EXPECT_EQ(32u, CountLeadingZeros32(0));
  EXPECT_EQ(31u, CountLeadingZeros32(1));
  TRACED_FORRANGE(uint32_t, shift, 0, 31) {
    EXPECT_EQ(31u - shift, CountLeadingZeros32(1u << shift));
  }
  EXPECT_EQ(4u, CountLeadingZeros32(0x0f0f0f0f));
}


TEST(Bits, CountLeadingZeros64) {
  EXPECT_EQ(64u, CountLeadingZeros64(0));
  EXPECT_EQ(63u, CountLeadingZeros64(1));
  TRACED_FORRANGE(uint32_t, shift, 0, 63) {
    EXPECT_EQ(63u - shift, CountLeadingZeros64(V8_UINT64_C(1) << shift));
  }
  EXPECT_EQ(36u, CountLeadingZeros64(0x0f0f0f0f));
  EXPECT_EQ(4u, CountLeadingZeros64(0x0f0f0f0f00000000));
}


TEST(Bits, CountTrailingZeros32) {
  EXPECT_EQ(32u, CountTrailingZeros32(0));
  EXPECT_EQ(31u, CountTrailingZeros32(0x80000000));
  TRACED_FORRANGE(uint32_t, shift, 0, 31) {
    EXPECT_EQ(shift, CountTrailingZeros32(1u << shift));
  }
  EXPECT_EQ(4u, CountTrailingZeros32(0xf0f0f0f0));
}


TEST(Bits, CountTrailingZeros64) {
  EXPECT_EQ(64u, CountTrailingZeros64(0));
  EXPECT_EQ(63u, CountTrailingZeros64(0x8000000000000000));
  TRACED_FORRANGE(uint32_t, shift, 0, 63) {
    EXPECT_EQ(shift, CountTrailingZeros64(V8_UINT64_C(1) << shift));
  }
  EXPECT_EQ(4u, CountTrailingZeros64(0xf0f0f0f0));
  EXPECT_EQ(36u, CountTrailingZeros64(0xf0f0f0f000000000));
}


TEST(Bits, IsPowerOfTwo32) {
  EXPECT_FALSE(IsPowerOfTwo32(0U));
  TRACED_FORRANGE(uint32_t, shift, 0, 31) {
    EXPECT_TRUE(IsPowerOfTwo32(1U << shift));
    EXPECT_FALSE(IsPowerOfTwo32((1U << shift) + 5U));
    EXPECT_FALSE(IsPowerOfTwo32(~(1U << shift)));
  }
  TRACED_FORRANGE(uint32_t, shift, 2, 31) {
    EXPECT_FALSE(IsPowerOfTwo32((1U << shift) - 1U));
  }
  EXPECT_FALSE(IsPowerOfTwo32(0xffffffff));
}


TEST(Bits, IsPowerOfTwo64) {
  EXPECT_FALSE(IsPowerOfTwo64(0U));
  TRACED_FORRANGE(uint32_t, shift, 0, 63) {
    EXPECT_TRUE(IsPowerOfTwo64(V8_UINT64_C(1) << shift));
    EXPECT_FALSE(IsPowerOfTwo64((V8_UINT64_C(1) << shift) + 5U));
    EXPECT_FALSE(IsPowerOfTwo64(~(V8_UINT64_C(1) << shift)));
  }
  TRACED_FORRANGE(uint32_t, shift, 2, 63) {
    EXPECT_FALSE(IsPowerOfTwo64((V8_UINT64_C(1) << shift) - 1U));
  }
  EXPECT_FALSE(IsPowerOfTwo64(V8_UINT64_C(0xffffffffffffffff)));
}


TEST(Bits, RoundUpToPowerOfTwo32) {
  TRACED_FORRANGE(uint32_t, shift, 0, 31) {
    EXPECT_EQ(1u << shift, RoundUpToPowerOfTwo32(1u << shift));
  }
  EXPECT_EQ(0u, RoundUpToPowerOfTwo32(0));
  EXPECT_EQ(4u, RoundUpToPowerOfTwo32(3));
  EXPECT_EQ(0x80000000u, RoundUpToPowerOfTwo32(0x7fffffffu));
}


TEST(BitsDeathTest, DISABLE_IN_RELEASE(RoundUpToPowerOfTwo32)) {
  ASSERT_DEATH_IF_SUPPORTED({ RoundUpToPowerOfTwo32(0x80000001u); },
                            "0x80000000");
}


TEST(Bits, RoundDownToPowerOfTwo32) {
  TRACED_FORRANGE(uint32_t, shift, 0, 31) {
    EXPECT_EQ(1u << shift, RoundDownToPowerOfTwo32(1u << shift));
  }
  EXPECT_EQ(0u, RoundDownToPowerOfTwo32(0));
  EXPECT_EQ(4u, RoundDownToPowerOfTwo32(5));
  EXPECT_EQ(0x80000000u, RoundDownToPowerOfTwo32(0x80000001u));
}


TEST(Bits, RotateRight32) {
  TRACED_FORRANGE(uint32_t, shift, 0, 31) {
    EXPECT_EQ(0u, RotateRight32(0u, shift));
  }
  EXPECT_EQ(1u, RotateRight32(1, 0));
  EXPECT_EQ(1u, RotateRight32(2, 1));
  EXPECT_EQ(0x80000000u, RotateRight32(1, 1));
}


TEST(Bits, RotateRight64) {
  TRACED_FORRANGE(uint64_t, shift, 0, 63) {
    EXPECT_EQ(0u, RotateRight64(0u, shift));
  }
  EXPECT_EQ(1u, RotateRight64(1, 0));
  EXPECT_EQ(1u, RotateRight64(2, 1));
  EXPECT_EQ(V8_UINT64_C(0x8000000000000000), RotateRight64(1, 1));
}


TEST(Bits, SignedAddOverflow32) {
  int32_t val = 0;
  EXPECT_FALSE(SignedAddOverflow32(0, 0, &val));
  EXPECT_EQ(0, val);
  EXPECT_TRUE(
      SignedAddOverflow32(std::numeric_limits<int32_t>::max(), 1, &val));
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), val);
  EXPECT_TRUE(
      SignedAddOverflow32(std::numeric_limits<int32_t>::min(), -1, &val));
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), val);
  EXPECT_TRUE(SignedAddOverflow32(std::numeric_limits<int32_t>::max(),
                                  std::numeric_limits<int32_t>::max(), &val));
  EXPECT_EQ(-2, val);
  TRACED_FORRANGE(int32_t, i, 1, 50) {
    TRACED_FORRANGE(int32_t, j, 1, i) {
      EXPECT_FALSE(SignedAddOverflow32(i, j, &val));
      EXPECT_EQ(i + j, val);
    }
  }
}


TEST(Bits, SignedSubOverflow32) {
  int32_t val = 0;
  EXPECT_FALSE(SignedSubOverflow32(0, 0, &val));
  EXPECT_EQ(0, val);
  EXPECT_TRUE(
      SignedSubOverflow32(std::numeric_limits<int32_t>::min(), 1, &val));
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), val);
  EXPECT_TRUE(
      SignedSubOverflow32(std::numeric_limits<int32_t>::max(), -1, &val));
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), val);
  TRACED_FORRANGE(int32_t, i, 1, 50) {
    TRACED_FORRANGE(int32_t, j, 1, i) {
      EXPECT_FALSE(SignedSubOverflow32(i, j, &val));
      EXPECT_EQ(i - j, val);
    }
  }
}


TEST(Bits, SignedMulHigh32) {
  EXPECT_EQ(0, SignedMulHigh32(0, 0));
  TRACED_FORRANGE(int32_t, i, 1, 50) {
    TRACED_FORRANGE(int32_t, j, 1, i) { EXPECT_EQ(0, SignedMulHigh32(i, j)); }
  }
  EXPECT_EQ(-1073741824, SignedMulHigh32(std::numeric_limits<int32_t>::max(),
                                         std::numeric_limits<int32_t>::min()));
  EXPECT_EQ(-1073741824, SignedMulHigh32(std::numeric_limits<int32_t>::min(),
                                         std::numeric_limits<int32_t>::max()));
  EXPECT_EQ(1, SignedMulHigh32(1024 * 1024 * 1024, 4));
  EXPECT_EQ(2, SignedMulHigh32(8 * 1024, 1024 * 1024));
}


TEST(Bits, SignedMulHighAndAdd32) {
  TRACED_FORRANGE(int32_t, i, 1, 50) {
    EXPECT_EQ(i, SignedMulHighAndAdd32(0, 0, i));
    TRACED_FORRANGE(int32_t, j, 1, i) {
      EXPECT_EQ(i, SignedMulHighAndAdd32(j, j, i));
    }
    EXPECT_EQ(i + 1, SignedMulHighAndAdd32(1024 * 1024 * 1024, 4, i));
  }
}


TEST(Bits, SignedDiv32) {
  EXPECT_EQ(std::numeric_limits<int32_t>::min(),
            SignedDiv32(std::numeric_limits<int32_t>::min(), -1));
  EXPECT_EQ(std::numeric_limits<int32_t>::max(),
            SignedDiv32(std::numeric_limits<int32_t>::max(), 1));
  TRACED_FORRANGE(int32_t, i, 0, 50) {
    EXPECT_EQ(0, SignedDiv32(i, 0));
    TRACED_FORRANGE(int32_t, j, 1, i) {
      EXPECT_EQ(1, SignedDiv32(j, j));
      EXPECT_EQ(i / j, SignedDiv32(i, j));
      EXPECT_EQ(-i / j, SignedDiv32(i, -j));
    }
  }
}


TEST(Bits, SignedMod32) {
  EXPECT_EQ(0, SignedMod32(std::numeric_limits<int32_t>::min(), -1));
  EXPECT_EQ(0, SignedMod32(std::numeric_limits<int32_t>::max(), 1));
  TRACED_FORRANGE(int32_t, i, 0, 50) {
    EXPECT_EQ(0, SignedMod32(i, 0));
    TRACED_FORRANGE(int32_t, j, 1, i) {
      EXPECT_EQ(0, SignedMod32(j, j));
      EXPECT_EQ(i % j, SignedMod32(i, j));
      EXPECT_EQ(i % j, SignedMod32(i, -j));
    }
  }
}


TEST(Bits, UnsignedDiv32) {
  TRACED_FORRANGE(uint32_t, i, 0, 50) {
    EXPECT_EQ(0u, UnsignedDiv32(i, 0));
    TRACED_FORRANGE(uint32_t, j, i + 1, 100) {
      EXPECT_EQ(1u, UnsignedDiv32(j, j));
      EXPECT_EQ(i / j, UnsignedDiv32(i, j));
    }
  }
}


TEST(Bits, UnsignedMod32) {
  TRACED_FORRANGE(uint32_t, i, 0, 50) {
    EXPECT_EQ(0u, UnsignedMod32(i, 0));
    TRACED_FORRANGE(uint32_t, j, i + 1, 100) {
      EXPECT_EQ(0u, UnsignedMod32(j, j));
      EXPECT_EQ(i % j, UnsignedMod32(i, j));
    }
  }
}

}  // namespace bits
}  // namespace base
}  // namespace v8
