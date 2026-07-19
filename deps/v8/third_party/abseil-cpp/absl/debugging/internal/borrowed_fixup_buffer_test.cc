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

#include "absl/debugging/internal/borrowed_fixup_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace internal_stacktrace {
namespace {

TEST(BorrowedFixupBuffer, ProperReuse) {
  uintptr_t first_borrowed_frame = 0;
  uintptr_t first_borrowed_size = 0;

  // Ensure that we borrow the same buffer each time, indicating proper reuse.
  // Disable loop unrolling. We need all iterations to match exactly, to coax
  // reuse of the the same underlying buffer.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC unroll 1  // <= 1 disables unrolling
#endif
  for (int i = 0; i < 100; ++i) {
    BorrowedFixupBuffer buf0(0);
    EXPECT_EQ(buf0.frames(), nullptr);
    EXPECT_EQ(buf0.sizes(), nullptr);

    BorrowedFixupBuffer buf1(1);
    EXPECT_NE(buf1.frames(), nullptr);
    EXPECT_NE(buf1.sizes(), nullptr);
    if (first_borrowed_frame == 0) {
      first_borrowed_frame = reinterpret_cast<uintptr_t>(buf1.frames());
    } else {
      EXPECT_EQ(reinterpret_cast<uintptr_t>(buf1.frames()),
                first_borrowed_frame);
    }
    if (first_borrowed_size == 0) {
      first_borrowed_size = reinterpret_cast<uintptr_t>(buf1.sizes());
    } else {
      EXPECT_EQ(reinterpret_cast<uintptr_t>(buf1.sizes()), first_borrowed_size);
    }

    BorrowedFixupBuffer buf2(2);
    EXPECT_NE(buf2.frames(), buf1.frames());
    EXPECT_NE(buf2.sizes(), buf1.sizes());
    EXPECT_NE(buf2.frames(), nullptr);
    EXPECT_NE(buf2.sizes(), nullptr);
  }
}

TEST(BorrowedFixupBuffer, NoOverlap) {
  using BufferPtr = std::unique_ptr<BorrowedFixupBuffer>;
  static constexpr std::less<const void*> less;
  static constexpr size_t kBufLen = 5;
  static constexpr size_t kNumBuffers =
      BorrowedFixupBuffer::kNumStaticBuffers * 37 + 1;

  auto bufs = std::make_unique<BufferPtr[]>(kNumBuffers);
  for (size_t i = 0; i < kNumBuffers; ++i) {
    bufs[i] = std::make_unique<BorrowedFixupBuffer>(kBufLen);
  }

  std::sort(bufs.get(), bufs.get() + kNumBuffers,
            [](const BufferPtr& a, const BufferPtr& b) {
              return less(a->frames(), b->frames());
            });

  // Verify there are no overlaps
  for (size_t i = 1; i < kNumBuffers; ++i) {
    EXPECT_FALSE(less(bufs[i]->frames(), bufs[i - 1]->frames() + kBufLen));
    EXPECT_FALSE(less(bufs[i]->sizes(), bufs[i - 1]->sizes() + kBufLen));
  }
}

}  // namespace
}  // namespace internal_stacktrace
ABSL_NAMESPACE_END
}  // namespace absl
