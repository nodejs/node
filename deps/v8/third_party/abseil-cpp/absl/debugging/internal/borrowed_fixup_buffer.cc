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

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <iterator>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/low_level_alloc.h"
#include "absl/hash/hash.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace internal_stacktrace {

// A buffer for holding fix-up information for stack traces of common sizes_.
struct BorrowedFixupBuffer::FixupStackBuffer {
  static constexpr size_t kMaxStackElements = 128;  // Can be reduced if needed
  std::atomic_flag in_use{};
  uintptr_t frames[kMaxStackElements];
  int sizes[kMaxStackElements];

  ABSL_CONST_INIT static FixupStackBuffer g_instances[kNumStaticBuffers];
};

ABSL_CONST_INIT BorrowedFixupBuffer::FixupStackBuffer
    BorrowedFixupBuffer::FixupStackBuffer::g_instances[kNumStaticBuffers] = {};

BorrowedFixupBuffer::~BorrowedFixupBuffer() {
  if (borrowed_) {
    std::move(*this).Unlock();
  } else {
    base_internal::LowLevelAlloc::Free(frames_);
  }
}

BorrowedFixupBuffer::BorrowedFixupBuffer(size_t length)
    : borrowed_(0 < length && length <= FixupStackBuffer::kMaxStackElements
                    ? TryLock()
                    : nullptr) {
  if (borrowed_) {
    InitViaBorrow();
  } else {
    InitViaAllocation(length);
  }
}

void BorrowedFixupBuffer::InitViaBorrow() {
  assert(borrowed_);
  frames_ = borrowed_->frames;
  sizes_ = borrowed_->sizes;
}

void BorrowedFixupBuffer::InitViaAllocation(size_t length) {
  static_assert(alignof(decltype(*frames_)) >= alignof(decltype(*sizes_)),
                "contiguous layout assumes decreasing alignment, otherwise "
                "padding may be needed in the middle");
  assert(!borrowed_);

  base_internal::InitSigSafeArena();
  void* buf = base_internal::LowLevelAlloc::AllocWithArena(
      length * (sizeof(*frames_) + sizeof(*sizes_)),
      base_internal::SigSafeArena());

  if (buf == nullptr) {
    frames_ = nullptr;
    sizes_ = nullptr;
    return;
  }

  frames_ = new (buf) uintptr_t[length];
  sizes_ = new (static_cast<void*>(static_cast<unsigned char*>(buf) +
                                   length * sizeof(*frames_))) int[length];
}

[[nodiscard]] BorrowedFixupBuffer::FixupStackBuffer*
BorrowedFixupBuffer::TryLock() {
  constexpr size_t kNumSlots = std::size(FixupStackBuffer::g_instances);
  const size_t i = absl::Hash<const void*>()(this) % kNumSlots;
  for (size_t k = 0; k < kNumSlots; ++k) {
    auto* instance = &FixupStackBuffer::g_instances[(i + k) % kNumSlots];
    // Use memory_order_acquire to ensure that no reads and writes on the
    // borrowed buffer are reordered before the borrowing.
    if (!instance->in_use.test_and_set(std::memory_order_acquire)) {
      return instance;
    }
  }
  return nullptr;
}

void BorrowedFixupBuffer::Unlock() && {
  // Use memory_order_release to ensure that no reads and writes on the borrowed
  // buffer are reordered after the borrowing.
  borrowed_->in_use.clear(std::memory_order_release);
}

}  // namespace internal_stacktrace
ABSL_NAMESPACE_END
}  // namespace absl
