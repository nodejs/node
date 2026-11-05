// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/utils.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "src/base/bit-field.h"
#include "src/base/bounds.h"
#include "src/base/strings.h"
#include "src/base/unique-array.h"
#include "src/base/vector.h"
#include "src/common/checks.h"
#include "src/numbers/conversions.h"
#include "src/utils/memcopy.h"
#include "test/common/collector.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {
namespace utils_unittest {

template <typename T>
class UtilsTest : public ::testing::Test {};

using IntegerTypes =
    ::testing::Types<signed char, unsigned char,
                     short,                    // NOLINT(runtime/int)
                     unsigned short,           // NOLINT(runtime/int)
                     int, unsigned int, long,  // NOLINT(runtime/int)
                     unsigned long,            // NOLINT(runtime/int)
                     long long,                // NOLINT(runtime/int)
                     unsigned long long,       // NOLINT(runtime/int)
                     int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                     int64_t, uint64_t>;

TYPED_TEST_SUITE(UtilsTest, IntegerTypes);

TYPED_TEST(UtilsTest, SaturateSub) {
  TypeParam min = std::numeric_limits<TypeParam>::min();
  TypeParam max = std::numeric_limits<TypeParam>::max();
  EXPECT_EQ(SaturateSub<TypeParam>(min, 0), min);
  EXPECT_EQ(SaturateSub<TypeParam>(max, 0), max);
  EXPECT_EQ(SaturateSub<TypeParam>(max, min), max);
  EXPECT_EQ(SaturateSub<TypeParam>(min, max), min);
  EXPECT_EQ(SaturateSub<TypeParam>(min, max / 3), min);
  EXPECT_EQ(SaturateSub<TypeParam>(min + 1, 2), min);
  if (std::numeric_limits<TypeParam>::is_signed) {
    EXPECT_EQ(SaturateSub<TypeParam>(min, min), static_cast<TypeParam>(0));
    EXPECT_EQ(SaturateSub<TypeParam>(0, min), max);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 3, min), max);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 5, min), max);
    EXPECT_EQ(SaturateSub<TypeParam>(min / 3, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(min / 9, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max, min / 3), max);
    EXPECT_EQ(SaturateSub<TypeParam>(min, max / 3), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 3 * 2, min / 2), max);
    EXPECT_EQ(SaturateSub<TypeParam>(min / 3 * 2, max / 2), min);
  } else {
    EXPECT_EQ(SaturateSub<TypeParam>(min, min), min);
    EXPECT_EQ(SaturateSub<TypeParam>(0, min), min);
    EXPECT_EQ(SaturateSub<TypeParam>(0, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max / 3, max), min);
    EXPECT_EQ(SaturateSub<TypeParam>(max - 3, max), min);
  }
  TypeParam test_cases[] = {static_cast<TypeParam>(min / 23),
                            static_cast<TypeParam>(max / 3),
                            63,
                            static_cast<TypeParam>(min / 6),
                            static_cast<TypeParam>(max / 55),
                            static_cast<TypeParam>(min / 2),
                            static_cast<TypeParam>(max / 2),
                            0,
                            1,
                            2,
                            3,
                            4,
                            42};
  TRACED_FOREACH(TypeParam, x, test_cases) {
    TRACED_FOREACH(TypeParam, y, test_cases) {
      if (std::numeric_limits<TypeParam>::is_signed) {
        EXPECT_EQ(SaturateSub<TypeParam>(x, y), x - y);
      } else {
        EXPECT_EQ(SaturateSub<TypeParam>(x, y), y > x ? min : x - y);
      }
    }
  }
}

TYPED_TEST(UtilsTest, SaturateAdd) {
  TypeParam min = std::numeric_limits<TypeParam>::min();
  TypeParam max = std::numeric_limits<TypeParam>::max();
  EXPECT_EQ(SaturateAdd<TypeParam>(min, min), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max, max), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(min, min / 3), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max / 8 * 7, max / 3 * 2), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(min / 3 * 2, min / 8 * 7), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max / 20 * 18, max / 25 * 18), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(min / 3 * 2, min / 3 * 2), min);
  EXPECT_EQ(SaturateAdd<TypeParam>(max - 1, 2), max);
  EXPECT_EQ(SaturateAdd<TypeParam>(max - 100, 101), max);
  TypeParam test_cases[] = {static_cast<TypeParam>(min / 23),
                            static_cast<TypeParam>(max / 3),
                            63,
                            static_cast<TypeParam>(min / 6),
                            static_cast<TypeParam>(max / 55),
                            static_cast<TypeParam>(min / 2),
                            static_cast<TypeParam>(max / 2),
                            0,
                            1,
                            2,
                            3,
                            4,
                            42};
  TRACED_FOREACH(TypeParam, x, test_cases) {
    TRACED_FOREACH(TypeParam, y, test_cases) {
      EXPECT_EQ(SaturateAdd<TypeParam>(x, y), x + y);
    }
  }
}

TEST(UtilsTest, PassesFilterTest) {
  EXPECT_TRUE(PassesFilter("abcdefg", "abcdefg"));
  EXPECT_TRUE(PassesFilter("abcdefg", "abcdefg*"));
  EXPECT_TRUE(PassesFilter("abcdefg", "abc*"));
  EXPECT_TRUE(PassesFilter("abcdefg", "*"));
  EXPECT_TRUE(PassesFilter("abcdefg", "-~"));
  EXPECT_TRUE(PassesFilter("abcdefg", "-abcdefgh"));
  EXPECT_TRUE(PassesFilter("abdefg", "-"));
  EXPECT_FALSE(PassesFilter("abcdefg", "-abcdefg"));
  EXPECT_FALSE(PassesFilter("abcdefg", "-abcdefg*"));
  EXPECT_FALSE(PassesFilter("abcdefg", "-abc*"));
  EXPECT_FALSE(PassesFilter("abcdefg", "-*"));
  EXPECT_FALSE(PassesFilter("abcdefg", "~"));
  EXPECT_FALSE(PassesFilter("abcdefg", ""));
  EXPECT_FALSE(PassesFilter("abcdefg", "abcdefgh"));

  EXPECT_TRUE(PassesFilter("", ""));
  EXPECT_TRUE(PassesFilter("", "*"));
  EXPECT_FALSE(PassesFilter("", "-"));
  EXPECT_FALSE(PassesFilter("", "-*"));
  EXPECT_FALSE(PassesFilter("", "a"));

  // Copy the vectors to give ASan a chance to catch off-by-one OOB reads,
  // which are hidden when the string literal is embedded into the binary
  // and hence always null-terminated.
  base::UniqueArray<const char> name =
      base::UniqueCopyOf(std::string_view("abcdefgh"));
  base::UniqueArray<const char> filter =
      base::UniqueCopyOf(std::string_view("abcdefgh*"));
  EXPECT_TRUE(PassesFilter({name.begin(), name.size()},
                           {filter.begin(), filter.size()}));
}

TEST(UtilsTest, IsInBounds) {
// for column consistency and terseness
#define INB(x, y, z) EXPECT_TRUE(base::IsInBounds<size_t>(x, y, z))
#define OOB(x, y, z) EXPECT_FALSE(base::IsInBounds<size_t>(x, y, z))
  INB(0, 0, 1);
  INB(0, 1, 1);
  INB(1, 0, 1);

  OOB(0, 2, 1);
  OOB(2, 0, 1);

  INB(0, 0, 2);
  INB(0, 1, 2);
  INB(0, 2, 2);

  INB(0, 0, 2);
  INB(1, 0, 2);
  INB(2, 0, 2);

  OOB(0, 3, 2);
  OOB(3, 0, 2);

  INB(0, 1, 2);
  INB(1, 1, 2);

  OOB(1, 2, 2);
  OOB(2, 1, 2);

  const size_t max = std::numeric_limits<size_t>::max();
  const size_t half = max / 2;

  // limit cases.
  INB(0, 0, max);
  INB(0, 1, max);
  INB(1, 0, max);
  INB(max, 0, max);
  INB(0, max, max);
  INB(max - 1, 0, max);
  INB(0, max - 1, max);
  INB(max - 1, 1, max);
  INB(1, max - 1, max);

  INB(half, half, max);
  INB(half + 1, half, max);
  INB(half, half + 1, max);

  OOB(max, 0, 0);
  OOB(0, max, 0);
  OOB(max, 0, 1);
  OOB(0, max, 1);
  OOB(max, 0, 2);
  OOB(0, max, 2);

  OOB(max, 0, max - 1);
  OOB(0, max, max - 1);

  // wraparound cases.
  OOB(max, 1, max);
  OOB(1, max, max);
  OOB(max - 1, 2, max);
  OOB(2, max - 1, max);
  OOB(half + 1, half + 1, max);
  OOB(half + 1, half + 1, max);

#undef INB
#undef OOB
}

namespace {

static const int kAreaSize = 512;

void TestMemMove(uint8_t* area1, uint8_t* area2, int src_offset,
                 int dest_offset, int length) {
  for (int i = 0; i < kAreaSize; i++) {
    area1[i] = i & 0xFF;
    area2[i] = i & 0xFF;
  }
  MemMove(area1 + dest_offset, area1 + src_offset, length);
  memmove(area2 + dest_offset, area2 + src_offset, length);
  if (memcmp(area1, area2, kAreaSize) != 0) {
    printf("MemMove(): src_offset: %d, dest_offset: %d, length: %d\n",
           src_offset, dest_offset, length);
    for (int i = 0; i < kAreaSize; i++) {
      if (area1[i] == area2[i]) continue;
      printf("diff at offset %d (%p): is %d, should be %d\n", i,
             reinterpret_cast<void*>(area1 + i), area1[i], area2[i]);
    }
    FATAL("memmove error");
  }
}

}  // namespace

TEST(UtilsTest, Utils1) {
  CHECK_EQ(-1000000, FastD2I(-1000000.0));
  CHECK_EQ(-1, FastD2I(-1.0));
  CHECK_EQ(0, FastD2I(0.0));
  CHECK_EQ(1, FastD2I(1.0));
  CHECK_EQ(1000000, FastD2I(1000000.0));

  CHECK_EQ(-1000000, FastD2I(-1000000.123));
  CHECK_EQ(-1, FastD2I(-1.234));
  CHECK_EQ(0, FastD2I(0.345));
  CHECK_EQ(1, FastD2I(1.234));
  CHECK_EQ(1000000, FastD2I(1000000.123));
  // Check that >> is implemented as arithmetic shift right.
  // If this is not true, then ArithmeticShiftRight() must be changed,
  // There are also documented right shifts in assembler.cc of
  // int8_t and intptr_t signed integers.
  CHECK_EQ(-2, -8 >> 2);
  CHECK_EQ(-2, static_cast<int8_t>(-8) >> 2);
  CHECK_EQ(-2, static_cast<int>(static_cast<intptr_t>(-8) >> 2));

  CHECK_EQ(-1000000, FastD2IChecked(-1000000.0));
  CHECK_EQ(-1, FastD2IChecked(-1.0));
  CHECK_EQ(0, FastD2IChecked(0.0));
  CHECK_EQ(1, FastD2IChecked(1.0));
  CHECK_EQ(1000000, FastD2IChecked(1000000.0));

  CHECK_EQ(-1000000, FastD2IChecked(-1000000.123));
  CHECK_EQ(-1, FastD2IChecked(-1.234));
  CHECK_EQ(0, FastD2IChecked(0.345));
  CHECK_EQ(1, FastD2IChecked(1.234));
  CHECK_EQ(1000000, FastD2IChecked(1000000.123));

  CHECK_EQ(INT_MAX, FastD2IChecked(1.0e100));
  CHECK_EQ(INT_MIN, FastD2IChecked(-1.0e100));
  CHECK_EQ(INT_MIN, FastD2IChecked(std::numeric_limits<double>::quiet_NaN()));
}

TEST(UtilsTest, BitSetComputer) {
  using BoolComputer = base::BitSetComputer<bool, 1, kSmiValueSize, uint32_t>;
  CHECK_EQ(0, BoolComputer::word_count(0));
  CHECK_EQ(1, BoolComputer::word_count(8));
  CHECK_EQ(2, BoolComputer::word_count(50));
  CHECK_EQ(0, BoolComputer::index(0, 8));
  CHECK_EQ(100, BoolComputer::index(100, 8));
  CHECK_EQ(1, BoolComputer::index(0, 40));

  {
    uint32_t data = 0;
    data = BoolComputer::encode(data, 1, true);
    data = BoolComputer::encode(data, 4, true);
    CHECK(BoolComputer::decode(data, 1));
    CHECK(BoolComputer::decode(data, 4));
    CHECK(!BoolComputer::decode(data, 0));
    CHECK(!BoolComputer::decode(data, 2));
    CHECK(!BoolComputer::decode(data, 3));
  }

  // Lets store 2 bits per item with 3000 items and verify the values are
  // correct.
  using TwoBits = base::BitSetComputer<unsigned char, 2, 8, unsigned char>;
  const int words = 750;
  CHECK_EQ(words, TwoBits::word_count(3000));
  const int offset = 10;
  base::UniqueArray<unsigned char> buffer =
      base::UniqueArray<unsigned char>::New(offset + words);
  for (int i = 0; i < words; i++) {
    const int index = TwoBits::index(offset, i);
    unsigned char data = buffer[index];
    data = TwoBits::encode(data, i, i % 4);
    buffer[index] = data;
  }

  for (int i = 0; i < words; i++) {
    const int index = TwoBits::index(offset, i);
    unsigned char data = buffer[index];
    CHECK_EQ(i % 4, TwoBits::decode(data, i));
  }
}

TEST(UtilsTest, SNPrintF) {
  // Make sure that strings that are truncated because of too small
  // buffers are zero-terminated anyway.
  const char* s = "the quick lazy .... oh forget it!";
  int length = static_cast<int>(strlen(s));
  for (int i = 1; i < length * 2; i++) {
    static const char kMarker = static_cast<char>(42);
    base::UniqueArray<char> buffer =
        base::UniqueArray<char>::NewForOverwrite(i + 1);
    buffer[i] = kMarker;
    int n = base::SNPrintF(base::Vector<char>(buffer.begin(), i), "%s", s);
    CHECK(n <= i);
    CHECK(n == length || n == -1);
    CHECK_EQ(0, strncmp(buffer.begin(), s, i - 1));
    CHECK_EQ(kMarker, buffer[i]);
    if (i <= length) {
      CHECK_EQ(i - 1, strlen(buffer.begin()));
    } else {
      CHECK_EQ(length, strlen(buffer.begin()));
    }
  }
}

TEST(UtilsTest, MemMove) {
  uint8_t* area1 = new uint8_t[kAreaSize];
  uint8_t* area2 = new uint8_t[kAreaSize];

  static const int kMinOffset = 32;
  static const int kMaxOffset = 64;
  static const int kMaxLength = 128;
  static_assert(kMaxOffset + kMaxLength < kAreaSize);

  for (int src_offset = kMinOffset; src_offset <= kMaxOffset; src_offset++) {
    for (int dst_offset = kMinOffset; dst_offset <= kMaxOffset; dst_offset++) {
      for (int length = 0; length <= kMaxLength; length++) {
        TestMemMove(area1, area2, src_offset, dst_offset, length);
      }
    }
  }
  delete[] area1;
  delete[] area2;
}

TEST(UtilsTest, Collector) {
  Collector<int> collector(8);
  const int kLoops = 5;
  const int kSequentialSize = 1000;
  const int kBlockSize = 7;
  for (int loop = 0; loop < kLoops; loop++) {
    base::Vector<int> block = collector.AddBlock(7, 0xBADCAFE);
    for (int i = 0; i < kSequentialSize; i++) {
      collector.Add(i);
    }
    for (int i = 0; i < kBlockSize - 1; i++) {
      block[i] = i * 7;
    }
  }
  base::UniqueArray<int> result = collector.ToVector();
  CHECK_EQ(kLoops * (kBlockSize + kSequentialSize), result.size());
  for (int i = 0; i < kLoops; i++) {
    int offset = i * (kSequentialSize + kBlockSize);
    for (int j = 0; j < kBlockSize - 1; j++) {
      CHECK_EQ(j * 7, result[offset + j]);
    }
    CHECK_EQ(0xBADCAFE, result[offset + kBlockSize - 1]);
    for (int j = 0; j < kSequentialSize; j++) {
      CHECK_EQ(j, result[offset + kBlockSize + j]);
    }
  }
}

TEST(UtilsTest, SequenceCollector) {
  SequenceCollector<int> collector(8);
  const int kLoops = 5000;
  const int kMaxSequenceSize = 13;
  size_t total_length = 0;
  for (int loop = 0; loop < kLoops; loop++) {
    int seq_length = loop % kMaxSequenceSize;
    collector.StartSequence();
    for (int j = 0; j < seq_length; j++) {
      collector.Add(j);
    }
    base::Vector<int> sequence = collector.EndSequence();
    for (int j = 0; j < seq_length; j++) {
      CHECK_EQ(j, sequence[j]);
    }
    total_length += seq_length;
  }
  base::UniqueArray<int> result = collector.ToVector();
  CHECK_EQ(total_length, result.size());
  int offset = 0;
  for (int loop = 0; loop < kLoops; loop++) {
    int seq_length = loop % kMaxSequenceSize;
    for (int j = 0; j < seq_length; j++) {
      CHECK_EQ(j, result[offset]);
      offset++;
    }
  }
}

TEST(UtilsTest, SequenceCollectorRegression) {
  SequenceCollector<char> collector(16);
  collector.StartSequence();
  collector.Add('0');
  collector.AddBlock(
      base::Vector<const char>("12345678901234567890123456789012", 32));
  base::Vector<char> seq = collector.EndSequence();
  CHECK_EQ(0, strncmp("0123456789012345678901234567890123", seq.begin(),
                      seq.length()));
}

TEST(UtilsTest, CPlusPlus11Features) {
  struct S {
    bool x;
    struct T {
      double y;
      int z[3];
    } t;
  };
  S s{true, {3.1415, {1, 2, 3}}};
  CHECK_EQ(2, s.t.z[1]);

  std::vector<int> vec{11, 22, 33, 44};
  vec.push_back(55);
  vec.push_back(66);
  for (auto& i : vec) {
    ++i;
  }
  int j = 12;
  for (auto i : vec) {
    CHECK_EQ(j, i);
    j += 11;
  }
}

}  // namespace utils_unittest
}  // namespace internal
}  // namespace v8
