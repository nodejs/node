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

#ifndef ABSL_DEBUGGING_INTERNAL_BORROWED_FIXUP_BUFFER_H_
#define ABSL_DEBUGGING_INTERNAL_BORROWED_FIXUP_BUFFER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace internal_stacktrace {

// An RAII type that temporarily acquires a buffer for stack trace fix-ups from
// a pool of preallocated buffers, or attempts to allocate a new buffer if no
// such buffer is available.
// When destroyed, returns the buffer to the pool if it borrowed successfully,
// otherwise deallocates any previously allocated buffer.
class BorrowedFixupBuffer {
 public:
  static constexpr size_t kNumStaticBuffers = 64;
  ~BorrowedFixupBuffer();

  // The number of frames to allocate space for. Note that allocations can fail.
  explicit BorrowedFixupBuffer(size_t length);

  uintptr_t* frames() const { return frames_; }
  int* sizes() const { return sizes_; }

 private:
  struct FixupStackBuffer;

  uintptr_t* frames_;
  int* sizes_;

  // The borrowed pre-existing buffer, if any (if we haven't allocated our own)
  FixupStackBuffer* const borrowed_;

  void InitViaBorrow();
  void InitViaAllocation(size_t length);

  // Attempts to opportunistically borrow a small buffer in a thread- and
  // signal-safe manner. Returns nullptr on failure.
  [[nodiscard]] FixupStackBuffer* TryLock();

  // Returns the borrowed buffer.
  void Unlock() &&;

  BorrowedFixupBuffer(const BorrowedFixupBuffer&) = delete;
  BorrowedFixupBuffer& operator=(const BorrowedFixupBuffer&) = delete;
};

}  // namespace internal_stacktrace
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_DEBUGGING_INTERNAL_BORROWED_FIXUP_BUFFER_H_
