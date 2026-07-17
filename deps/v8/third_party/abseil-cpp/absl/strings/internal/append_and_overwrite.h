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

#ifndef ABSL_STRINGS_INTERNAL_APPEND_AND_OVERWRITE_H_
#define ABSL_STRINGS_INTERNAL_APPEND_AND_OVERWRITE_H_

#include "absl/base/config.h"
#include "absl/base/internal/throw_delegate.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/strings/resize_and_overwrite.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

// An internal-only variant similar to `absl::StringResizeAndOverwrite()`
// optimized for repeated appends to a string that uses exponential growth so
// that the amortized complexity of increasing the string size by a small amount
// is O(1), in contrast to O(str.size()) in the case of precise growth. Use of
// this function is subtle; see https://reviews.llvm.org/D102727 to understand
// the tradeoffs.
//
// Appends at most `append_n` characters to `str`, using the user-provided
// operation `append_op` to modify the possibly indeterminate
// contents. `append_op` must return the length of the buffer appended to `str`.
//
// Invalidates all iterators, pointers, and references into `str`, regardless
// of whether reallocation occurs.
//
// `append_op(value_type* buf, size_t buf_size)` is allowed to write
// `value_type{}` to `buf[buf_size]`, which facilitiates interoperation with
// functions that write a trailing NUL.
template <typename T, typename Op>
void StringAppendAndOverwrite(T& str, typename T::size_type append_n,
                              Op append_op) {
  if (ABSL_PREDICT_FALSE(append_n > str.max_size() - str.size())) {
    absl::base_internal::ThrowStdLengthError(
        "absl::strings_internal::StringAppendAndOverwrite");
  }

  auto old_size = str.size();
  auto resize = old_size + append_n;

  if (resize > str.capacity()) {
    // Make sure to always grow by at least a factor of 2x.
    const auto min_growth = str.capacity();
    if (ABSL_PREDICT_FALSE(str.capacity() > str.max_size() - min_growth)) {
      resize = str.max_size();
    } else if (resize < str.capacity() + min_growth) {
      resize = str.capacity() + min_growth;
    }
  } else {
    resize = str.capacity();
  }

  // Avoid calling StringResizeAndOverwrite() here since it does an MSAN
  // verification on the entire string. StringResizeAndOverwriteImpl() is
  // StringResizeAndOverwrite() without the MSAN verification.
  StringResizeAndOverwriteImpl(
      str, resize,
      [old_size, append_n, do_append = std::move(append_op)](
          typename T::value_type* data_ptr, typename T::size_type) mutable {
        auto num_appended =
            std::move(do_append)(data_ptr + old_size, append_n);
        ABSL_HARDENING_ASSERT(num_appended >= 0 && num_appended <= append_n);
        return old_size + num_appended;
      });

#if defined(ABSL_HAVE_MEMORY_SANITIZER)
  // Only check the region appended to. Checking the entire string would cause
  // pathological quadratic verfication on repeated small appends.
  __msan_check_mem_is_initialized(str.data() + old_size, str.size() - old_size);
#endif
}

}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl


#endif  // ABSL_STRINGS_INTERNAL_APPEND_AND_OVERWRITE_H_
