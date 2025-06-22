// Copyright 2025 The Abseil Authors
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

#include "absl/container/internal/raw_hash_set_resize_impl.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

constexpr size_t kSmallSourceOffset = 17;
constexpr size_t kSmallH1 = 25;

template <class Encoder>
class EncoderTest : public testing::Test {};

using ProbedItemTypes =
    ::testing::Types<ProbedItem4Bytes, ProbedItem8Bytes, ProbedItem16Bytes>;
TYPED_TEST_SUITE(EncoderTest, ProbedItemTypes);

TYPED_TEST(EncoderTest, EncodeDecodeSmall) {
  using ProbedItem = TypeParam;
  for (uint8_t h2 = 0; h2 < 128; ++h2) {
    ProbedItem item(h2, kSmallSourceOffset, kSmallH1);
    EXPECT_EQ(item.h2, h2);
    EXPECT_EQ(item.source_offset, kSmallSourceOffset);
    EXPECT_EQ(item.h1 & ProbedItem::kMaxNewCapacity, kSmallH1);
  }
}

TYPED_TEST(EncoderTest, EncodeDecodeMax) {
  using ProbedItem = TypeParam;
  for (uint8_t h2 = 0; h2 < 128; ++h2) {
    size_t source_offset = static_cast<size_t>(std::min<uint64_t>(
        ProbedItem::kMaxOldCapacity, (std::numeric_limits<size_t>::max)()));
    size_t h1 = static_cast<size_t>(std::min<uint64_t>(
        ProbedItem::kMaxNewCapacity, (std::numeric_limits<size_t>::max)()));
    ProbedItem item(h2, source_offset, h1);
    EXPECT_EQ(item.h2, h2);
    EXPECT_EQ(item.source_offset, source_offset);
    EXPECT_EQ(item.h1 & ProbedItem::kMaxNewCapacity, h1);
  }
}

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
