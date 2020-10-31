/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/containers/bit_vector.h"

#include <random>

#include "src/trace_processor/containers/bit_vector_iterators.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(BitVectorUnittest, CreateAllTrue) {
  BitVector bv(2049, true);

  // Ensure that a selection of interesting bits are set.
  ASSERT_TRUE(bv.IsSet(0));
  ASSERT_TRUE(bv.IsSet(1));
  ASSERT_TRUE(bv.IsSet(511));
  ASSERT_TRUE(bv.IsSet(512));
  ASSERT_TRUE(bv.IsSet(2047));
  ASSERT_TRUE(bv.IsSet(2048));
}

TEST(BitVectorUnittest, CreateAllFalse) {
  BitVector bv(2049, false);

  // Ensure that a selection of interesting bits are cleared.
  ASSERT_FALSE(bv.IsSet(0));
  ASSERT_FALSE(bv.IsSet(1));
  ASSERT_FALSE(bv.IsSet(511));
  ASSERT_FALSE(bv.IsSet(512));
  ASSERT_FALSE(bv.IsSet(2047));
  ASSERT_FALSE(bv.IsSet(2048));
}

TEST(BitVectorUnittest, Set) {
  BitVector bv(2049, false);
  bv.Set(0);
  bv.Set(1);
  bv.Set(511);
  bv.Set(512);
  bv.Set(2047);

  // Ensure the bits we touched are set.
  ASSERT_TRUE(bv.IsSet(0));
  ASSERT_TRUE(bv.IsSet(1));
  ASSERT_TRUE(bv.IsSet(511));
  ASSERT_TRUE(bv.IsSet(512));
  ASSERT_TRUE(bv.IsSet(2047));

  // Ensure that a selection of other interestinng bits are
  // still cleared.
  ASSERT_FALSE(bv.IsSet(2));
  ASSERT_FALSE(bv.IsSet(63));
  ASSERT_FALSE(bv.IsSet(64));
  ASSERT_FALSE(bv.IsSet(510));
  ASSERT_FALSE(bv.IsSet(513));
  ASSERT_FALSE(bv.IsSet(1023));
  ASSERT_FALSE(bv.IsSet(1024));
  ASSERT_FALSE(bv.IsSet(2046));
  ASSERT_FALSE(bv.IsSet(2048));
  ASSERT_FALSE(bv.IsSet(2048));
}

TEST(BitVectorUnittest, Clear) {
  BitVector bv(2049, true);
  bv.Clear(0);
  bv.Clear(1);
  bv.Clear(511);
  bv.Clear(512);
  bv.Clear(2047);

  // Ensure the bits we touched are cleared.
  ASSERT_FALSE(bv.IsSet(0));
  ASSERT_FALSE(bv.IsSet(1));
  ASSERT_FALSE(bv.IsSet(511));
  ASSERT_FALSE(bv.IsSet(512));
  ASSERT_FALSE(bv.IsSet(2047));

  // Ensure that a selection of other interestinng bits are
  // still set.
  ASSERT_TRUE(bv.IsSet(2));
  ASSERT_TRUE(bv.IsSet(63));
  ASSERT_TRUE(bv.IsSet(64));
  ASSERT_TRUE(bv.IsSet(510));
  ASSERT_TRUE(bv.IsSet(513));
  ASSERT_TRUE(bv.IsSet(1023));
  ASSERT_TRUE(bv.IsSet(1024));
  ASSERT_TRUE(bv.IsSet(2046));
  ASSERT_TRUE(bv.IsSet(2048));
}

TEST(BitVectorUnittest, AppendToEmpty) {
  BitVector bv;
  bv.AppendTrue();
  bv.AppendFalse();

  ASSERT_EQ(bv.size(), 2u);
  ASSERT_TRUE(bv.IsSet(0));
  ASSERT_FALSE(bv.IsSet(1));
}

TEST(BitVectorUnittest, AppendToExisting) {
  BitVector bv(2046, false);
  bv.AppendTrue();
  bv.AppendFalse();
  bv.AppendTrue();
  bv.AppendTrue();

  ASSERT_EQ(bv.size(), 2050u);
  ASSERT_TRUE(bv.IsSet(2046));
  ASSERT_FALSE(bv.IsSet(2047));
  ASSERT_TRUE(bv.IsSet(2048));
  ASSERT_TRUE(bv.IsSet(2049));
}

TEST(BitVectorUnittest, GetNumBitsSet) {
  BitVector bv(2049, false);
  bv.Set(0);
  bv.Set(1);
  bv.Set(511);
  bv.Set(512);
  bv.Set(2047);
  bv.Set(2048);

  ASSERT_EQ(bv.GetNumBitsSet(), 6u);

  ASSERT_EQ(bv.GetNumBitsSet(0), 0u);
  ASSERT_EQ(bv.GetNumBitsSet(1), 1u);
  ASSERT_EQ(bv.GetNumBitsSet(2), 2u);
  ASSERT_EQ(bv.GetNumBitsSet(3), 2u);
  ASSERT_EQ(bv.GetNumBitsSet(511), 2u);
  ASSERT_EQ(bv.GetNumBitsSet(512), 3u);
  ASSERT_EQ(bv.GetNumBitsSet(1023), 4u);
  ASSERT_EQ(bv.GetNumBitsSet(1024), 4u);
  ASSERT_EQ(bv.GetNumBitsSet(2047), 4u);
  ASSERT_EQ(bv.GetNumBitsSet(2048), 5u);
  ASSERT_EQ(bv.GetNumBitsSet(2049), 6u);
}

TEST(BitVectorUnittest, IndexOfNthSet) {
  BitVector bv(2050, false);
  bv.Set(0);
  bv.Set(1);
  bv.Set(511);
  bv.Set(512);
  bv.Set(2047);
  bv.Set(2048);

  ASSERT_EQ(bv.IndexOfNthSet(0), 0u);
  ASSERT_EQ(bv.IndexOfNthSet(1), 1u);
  ASSERT_EQ(bv.IndexOfNthSet(2), 511u);
  ASSERT_EQ(bv.IndexOfNthSet(3), 512u);
  ASSERT_EQ(bv.IndexOfNthSet(4), 2047u);
  ASSERT_EQ(bv.IndexOfNthSet(5), 2048u);
}

TEST(BitVectorUnittest, Resize) {
  BitVector bv(1, false);

  bv.Resize(2, true);
  ASSERT_EQ(bv.size(), 2u);
  ASSERT_EQ(bv.IsSet(1), true);

  bv.Resize(2049, false);
  ASSERT_EQ(bv.size(), 2049u);
  ASSERT_EQ(bv.IsSet(2), false);
  ASSERT_EQ(bv.IsSet(2047), false);
  ASSERT_EQ(bv.IsSet(2048), false);

  // Set these two bits; the first should be preserved and the
  // second should disappear.
  bv.Set(512);
  bv.Set(513);

  bv.Resize(513, false);
  ASSERT_EQ(bv.size(), 513u);
  ASSERT_EQ(bv.IsSet(1), true);
  ASSERT_EQ(bv.IsSet(512), true);
  ASSERT_EQ(bv.GetNumBitsSet(), 2u);

  // When we resize up, we need to be sure that the set bit from
  // before we resized down is not still present as a garbage bit.
  bv.Resize(514, false);
  ASSERT_EQ(bv.size(), 514u);
  ASSERT_EQ(bv.IsSet(513), false);
  ASSERT_EQ(bv.GetNumBitsSet(), 2u);
}

TEST(BitVectorUnittest, ResizeHasCorrectCount) {
  BitVector bv(1, false);
  ASSERT_EQ(bv.GetNumBitsSet(), 0u);

  bv.Resize(1024, true);
  ASSERT_EQ(bv.GetNumBitsSet(), 1023u);
}

TEST(BitVectorUnittest, AppendAfterResizeDown) {
  BitVector bv(2049, false);
  bv.Set(2048);

  bv.Resize(2048);
  bv.AppendFalse();
  ASSERT_EQ(bv.IsSet(2048), false);
  ASSERT_EQ(bv.GetNumBitsSet(), 0u);
}

TEST(BitVectorUnittest, UpdateSetBits) {
  BitVector bv(6, false);
  bv.Set(1);
  bv.Set(2);
  bv.Set(4);

  BitVector picker(3u, true);
  picker.Clear(1);

  bv.UpdateSetBits(picker);

  ASSERT_TRUE(bv.IsSet(1));
  ASSERT_FALSE(bv.IsSet(2));
  ASSERT_TRUE(bv.IsSet(4));
}

TEST(BitVectorUnittest, UpdateSetBitsSmallerPicker) {
  BitVector bv(6, false);
  bv.Set(1);
  bv.Set(2);
  bv.Set(4);

  BitVector picker(2u, true);
  picker.Clear(1);

  bv.UpdateSetBits(picker);

  ASSERT_TRUE(bv.IsSet(1));
  ASSERT_FALSE(bv.IsSet(2));
  ASSERT_FALSE(bv.IsSet(4));
}

TEST(BitVectorUnittest, IterateAllBitsConst) {
  BitVector bv;
  for (uint32_t i = 0; i < 12345; ++i) {
    if (i % 7 == 0 || i % 13 == 0) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }
  }

  uint32_t i = 0;
  for (auto it = bv.IterateAllBits(); it; it.Next(), ++i) {
    ASSERT_EQ(it.IsSet(), i % 7 == 0 || i % 13 == 0);
    ASSERT_EQ(it.index(), i);
  }
}

TEST(BitVectorUnittest, IterateAllBitsSet) {
  BitVector bv;
  for (uint32_t i = 0; i < 12345; ++i) {
    if (i % 7 == 0 || i % 13 == 0) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }
  }

  // Unset every 15th bit.
  for (auto it = bv.IterateAllBits(); it; it.Next()) {
    if (it.index() % 15 == 0) {
      it.Set();
    }
  }

  // Go through the iterator manually and check it has updated
  // to not have every 15th bit set.
  uint32_t count = 0;
  for (uint32_t i = 0; i < 12345; ++i) {
    bool is_set = i % 15 == 0 || i % 7 == 0 || i % 13 == 0;

    ASSERT_EQ(bv.IsSet(i), is_set);
    ASSERT_EQ(bv.GetNumBitsSet(i), count);

    if (is_set) {
      ASSERT_EQ(bv.IndexOfNthSet(count++), i);
    }
  }
}

TEST(BitVectorUnittest, IterateAllBitsClear) {
  BitVector bv;
  for (uint32_t i = 0; i < 12345; ++i) {
    if (i % 7 == 0 || i % 13 == 0) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }
  }

  // Unset every 15th bit.
  for (auto it = bv.IterateAllBits(); it; it.Next()) {
    if (it.index() % 15 == 0) {
      it.Clear();
    }
  }

  // Go through the iterator manually and check it has updated
  // to not have every 15th bit set.
  uint32_t count = 0;
  for (uint32_t i = 0; i < 12345; ++i) {
    bool is_set = i % 15 != 0 && (i % 7 == 0 || i % 13 == 0);

    ASSERT_EQ(bv.IsSet(i), is_set);
    ASSERT_EQ(bv.GetNumBitsSet(i), count);

    if (is_set) {
      ASSERT_EQ(bv.IndexOfNthSet(count++), i);
    }
  }
}

TEST(BitVectorUnittest, IterateSetBitsConst) {
  BitVector bv;
  std::vector<uint32_t> set_indices;
  for (uint32_t i = 0; i < 12345; ++i) {
    if (i % 7 == 0 || i % 13 == 0) {
      bv.AppendTrue();
      set_indices.emplace_back(i);
    } else {
      bv.AppendFalse();
    }
  }

  uint32_t i = 0;
  for (auto it = bv.IterateSetBits(); it; it.Next(), ++i) {
    ASSERT_EQ(it.IsSet(), true);
    ASSERT_EQ(it.index(), set_indices[i]);
  }
  ASSERT_EQ(i, set_indices.size());
}

TEST(BitVectorUnittest, IterateSetBitsClear) {
  BitVector bv;
  for (uint32_t i = 0; i < 12345; ++i) {
    if (i % 7 == 0 || i % 13 == 0) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }
  }

  for (auto it = bv.IterateSetBits(); it; it.Next()) {
    if (it.index() % 15 == 0) {
      it.Clear();
    }
  }

  // Go through the iterator manually and check it has updated
  // to not have every 15th bit set.
  uint32_t count = 0;
  for (uint32_t i = 0; i < 12345; ++i) {
    bool is_set = i % 15 != 0 && (i % 7 == 0 || i % 13 == 0);

    ASSERT_EQ(bv.IsSet(i), is_set);
    ASSERT_EQ(bv.GetNumBitsSet(i), count);

    if (is_set) {
      ASSERT_EQ(bv.IndexOfNthSet(count++), i);
    }
  }
}

TEST(BitVectorUnittest, IterateSetBitsStartsCorrectly) {
  BitVector bv;
  bv.AppendFalse();
  bv.AppendTrue();

  auto it = bv.IterateSetBits();
  ASSERT_TRUE(it);
  ASSERT_EQ(it.index(), 1u);
  ASSERT_TRUE(it.IsSet());

  it.Next();
  ASSERT_FALSE(it);
}

TEST(BitVectorUnittest, Range) {
  BitVector bv =
      BitVector::Range(1, 1025, [](uint32_t t) { return t % 3 == 0; });

  ASSERT_FALSE(bv.IsSet(0));
  for (uint32_t i = 1; i < 1025; ++i) {
    ASSERT_EQ(i % 3 == 0, bv.IsSet(i));
  }
  ASSERT_EQ(bv.size(), 1025u);
  ASSERT_EQ(bv.GetNumBitsSet(), 341u);
}

TEST(BitVectorUnittest, QueryStressTest) {
  BitVector bv;
  std::vector<bool> bool_vec;
  std::vector<uint32_t> int_vec;

  static constexpr uint32_t kCount = 4096;
  std::minstd_rand0 rand;
  for (uint32_t i = 0; i < kCount; ++i) {
    bool res = rand() % 2u;
    if (res) {
      bv.AppendTrue();
    } else {
      bv.AppendFalse();
    }
    bool_vec.push_back(res);
    if (res)
      int_vec.emplace_back(i);
  }

  auto all_it = bv.IterateAllBits();
  for (uint32_t i = 0; i < kCount; ++i) {
    uint32_t count = static_cast<uint32_t>(std::count(
        bool_vec.begin(), bool_vec.begin() + static_cast<int32_t>(i), true));
    ASSERT_EQ(bv.IsSet(i), bool_vec[i]);
    ASSERT_EQ(bv.GetNumBitsSet(i), count);

    ASSERT_TRUE(all_it);
    ASSERT_EQ(all_it.IsSet(), bool_vec[i]);
    ASSERT_EQ(all_it.index(), i);
    all_it.Next();
  }
  ASSERT_FALSE(all_it);

  auto set_it = bv.IterateSetBits();
  for (uint32_t i = 0; i < int_vec.size(); ++i) {
    ASSERT_EQ(bv.IndexOfNthSet(i), int_vec[i]);

    ASSERT_TRUE(set_it);
    ASSERT_EQ(set_it.IsSet(), true);
    ASSERT_EQ(set_it.index(), int_vec[i]);
    set_it.Next();
  }
  ASSERT_FALSE(set_it);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
