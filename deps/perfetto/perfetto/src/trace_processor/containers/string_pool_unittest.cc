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

#include "src/trace_processor/containers/string_pool.h"

#include <array>
#include <random>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {

class StringPoolTest : public testing::Test {
 protected:
  static constexpr size_t kNumBlockOffsetBits = StringPool::kNumBlockOffsetBits;
  static constexpr size_t kBlockIndexBitMask = StringPool::kBlockIndexBitMask;
  static constexpr size_t kBlockSizeBytes = StringPool::kBlockSizeBytes;
  static constexpr size_t kMinLargeStringSizeBytes =
      StringPool::kMinLargeStringSizeBytes;

  StringPool pool_;
};

namespace {

TEST_F(StringPoolTest, EmptyPool) {
  ASSERT_EQ(pool_.Get(StringPool::Id::Null()).c_str(), nullptr);

  auto it = pool_.CreateIterator();
  ASSERT_TRUE(it);
  ASSERT_EQ(it.StringView().c_str(), nullptr);
  ASSERT_FALSE(++it);
}

TEST_F(StringPoolTest, InternAndRetrieve) {
  static char kString[] = "Test String";
  auto id = pool_.InternString(kString);
  ASSERT_STREQ(pool_.Get(id).c_str(), kString);
  ASSERT_EQ(pool_.Get(id), kString);
  ASSERT_EQ(id, pool_.InternString(kString));
}

TEST_F(StringPoolTest, NullPointerHandling) {
  auto id = pool_.InternString(NullTermStringView());
  ASSERT_TRUE(id.is_null());
  ASSERT_EQ(pool_.Get(id).c_str(), nullptr);
}

TEST_F(StringPoolTest, Iterator) {
  auto it = pool_.CreateIterator();
  ASSERT_TRUE(it);
  ASSERT_EQ(it.StringView().c_str(), nullptr);
  ASSERT_FALSE(++it);

  static char kString[] = "Test String";
  pool_.InternString(kString);

  it = pool_.CreateIterator();
  ASSERT_TRUE(++it);
  ASSERT_STREQ(it.StringView().c_str(), kString);
  ASSERT_FALSE(++it);
}

TEST_F(StringPoolTest, ConstIterator) {
  static char kString[] = "Test String";
  pool_.InternString(kString);

  const StringPool& const_pool = pool_;

  auto it = const_pool.CreateIterator();
  ASSERT_TRUE(it);
  ASSERT_TRUE(++it);
  ASSERT_STREQ(it.StringView().c_str(), kString);
  ASSERT_FALSE(++it);
}

TEST_F(StringPoolTest, StressTest) {
  // First create a buffer with 33MB of random characters, so that we insert
  // into at least two chunks.
  constexpr size_t kBufferSize = 33 * 1024 * 1024;
  std::minstd_rand0 rnd_engine(0);
  std::unique_ptr<char[]> buffer(new char[kBufferSize]);
  for (size_t i = 0; i < kBufferSize; i++)
    buffer.get()[i] = 'A' + (rnd_engine() % 26);

  // Next create strings of length 0 to 16k in length from this buffer and
  // intern them, storing their ids.
  std::multimap<StringPool::Id, base::StringView> string_map;
  constexpr uint16_t kMaxStrSize = 16u * 1024u - 1;
  for (size_t i = 0;;) {
    size_t length = static_cast<uint64_t>(rnd_engine()) % (kMaxStrSize + 1);
    if (i + length > kBufferSize)
      break;

    auto str = base::StringView(&buffer.get()[i], length);
    string_map.emplace(pool_.InternString(str), str);
    i += length;
  }

  // Finally, iterate through each string in the string pool, check that all ids
  // that match in the multimap are equal, and finish by checking we've removed
  // every item in the multimap.
  for (auto it = pool_.CreateIterator(); it; ++it) {
    ASSERT_EQ(it.StringView(), pool_.Get(it.StringId()));

    auto it_pair = string_map.equal_range(it.StringId());
    for (auto in_it = it_pair.first; in_it != it_pair.second; ++in_it) {
      ASSERT_EQ(it.StringView(), in_it->second)
          << it.StringId().raw_id() << ": " << it.StringView().Hash() << " vs "
          << in_it->second.Hash();
    }
    string_map.erase(it_pair.first, it_pair.second);
  }
  ASSERT_EQ(string_map.size(), 0u);
}

TEST_F(StringPoolTest, BigString) {
  // Two of these should fit into one block, but the third one should go into
  // the |large_strings_| list.
  constexpr size_t kBigStringSize = 15 * 1024 * 1024;
  // Will fit into block 1 after two kBigStringSize strings.
  constexpr size_t kSmallStringSize = 16 * 1024;
  // Will not fit into block 1 anymore after 2*kBigStringSize and
  // 2*kSmallStringSize, but is smaller than kMinLargeStringSizeBytes, so will
  // start a new block.
  constexpr size_t kMediumStringSize = 2 * 1024 * 1024;
  // Would not fit into a block at all, so ahs to go into |large_strings_|.
  constexpr size_t kEnormousStringSize = 33 * 1024 * 1024;

  constexpr std::array<size_t, 8> kStringSizes = {
      kBigStringSize,       // block 1
      kBigStringSize,       // block 1
      kBigStringSize,       // large strings
      kSmallStringSize,     // block 1
      kSmallStringSize,     // block 1
      kMediumStringSize,    // block 2
      kEnormousStringSize,  // large strings
      kBigStringSize        // block 2
  };

  static_assert(kBigStringSize * 2 + kSmallStringSize * 2 + kMediumStringSize >
                    kBlockSizeBytes,
                "medium string shouldn't fit into block 1 for this test");
  static_assert(kMediumStringSize < kMinLargeStringSizeBytes,
                "medium string should cause a new block for this test");

  std::array<std::unique_ptr<char[]>, kStringSizes.size()> big_strings;
  for (size_t i = 0; i < big_strings.size(); i++) {
    big_strings[i].reset(new char[kStringSizes[i] + 1]);
    for (size_t j = 0; j < kStringSizes[i]; j++) {
      big_strings[i].get()[j] = 'A' + static_cast<char>((j + i) % 26);
    }
    big_strings[i].get()[kStringSizes[i]] = '\0';
  }

  std::array<StringPool::Id, kStringSizes.size()> string_ids;
  for (size_t i = 0; i < big_strings.size(); i++) {
    string_ids[i] = pool_.InternString(
        base::StringView(big_strings[i].get(), kStringSizes[i]));
    // Interning it a second time should return the original id.
    ASSERT_EQ(string_ids[i], pool_.InternString(base::StringView(
                                 big_strings[i].get(), kStringSizes[i])));
  }

  ASSERT_FALSE(string_ids[0].is_large_string());
  ASSERT_FALSE(string_ids[1].is_large_string());
  ASSERT_TRUE(string_ids[2].is_large_string());
  ASSERT_FALSE(string_ids[3].is_large_string());
  ASSERT_FALSE(string_ids[4].is_large_string());
  ASSERT_FALSE(string_ids[5].is_large_string());
  ASSERT_TRUE(string_ids[6].is_large_string());
  ASSERT_FALSE(string_ids[7].is_large_string());

  ASSERT_EQ(string_ids[0].block_index(), 0u);
  ASSERT_EQ(string_ids[1].block_index(), 0u);
  ASSERT_EQ(string_ids[3].block_index(), 0u);
  ASSERT_EQ(string_ids[4].block_index(), 0u);
  ASSERT_EQ(string_ids[5].block_index(), 1u);
  ASSERT_EQ(string_ids[7].block_index(), 1u);

  for (size_t i = 0; i < big_strings.size(); i++) {
    ASSERT_EQ(big_strings[i].get(), pool_.Get(string_ids[i]));
  }
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
