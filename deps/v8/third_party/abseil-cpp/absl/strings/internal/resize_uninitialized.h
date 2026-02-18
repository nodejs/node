//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef ABSL_STRINGS_INTERNAL_RESIZE_UNINITIALIZED_H_
#define ABSL_STRINGS_INTERNAL_RESIZE_UNINITIALIZED_H_

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/base/port.h"
#include "absl/meta/type_traits.h"  //  for void_t
#include "absl/strings/resize_and_overwrite.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

// In this type trait, we look for a __resize_default_init member function, and
// we use it if available, otherwise, we use resize. We provide HasMember to
// indicate whether __resize_default_init is present.
template <typename string_type, typename = void>
struct ResizeUninitializedTraits {
  using HasMember = std::false_type;
  static void Resize(string_type* s, size_t new_size) { s->resize(new_size); }
};

// __resize_default_init is provided by libc++ >= 8.0
template <typename string_type>
struct ResizeUninitializedTraits<
    string_type, absl::void_t<decltype(std::declval<string_type&>()
                                           .__resize_default_init(237))> > {
  using HasMember = std::true_type;
  static void Resize(string_type* s, size_t new_size) {
    s->__resize_default_init(new_size);
  }
};

// Returns true if the std::string implementation supports a resize where
// the new characters added to the std::string are left untouched.
//
// (A better name might be "STLStringSupportsUninitializedResize", alluding to
// the previous function.)
template <typename string_type>
inline constexpr bool STLStringSupportsNontrashingResize(string_type*) {
  return ResizeUninitializedTraits<string_type>::HasMember::value;
}

// Like str->resize(new_size), except any new characters added to "*str" as a
// result of resizing may be left uninitialized, rather than being filled with
// '0' bytes. Typically used when code is then going to overwrite the backing
// store of the std::string with known data.
template <typename string_type, typename = void>
inline void STLStringResizeUninitialized(string_type* s, size_t new_size) {
  ResizeUninitializedTraits<string_type>::Resize(s, new_size);
}

// Like STLStringResizeUninitialized(str, new_size), except guaranteed to use
// exponential growth so that the amortized complexity of increasing the string
// size by a small amount is O(1), in contrast to O(str->size()) in the case of
// precise growth.
//
// TODO: b/446221957 - Delete this function.
template <typename string_type>
[[deprecated]]
void STLStringResizeUninitializedAmortized(string_type* s, size_t new_size) {
  if (new_size > s->size()) {
    size_t enlarged_new_size = new_size;
    if (enlarged_new_size > s->capacity()) {
      // Make sure to always grow by at least a factor of 2x. Change min_growth
      // if you want to experiment with other growth strategies.
      const auto min_growth = s->capacity();
      if (ABSL_PREDICT_FALSE(s->capacity() > s->max_size() - min_growth)) {
        enlarged_new_size = s->max_size();
      } else if (enlarged_new_size < s->capacity() + min_growth) {
        enlarged_new_size = s->capacity() + min_growth;
      }
    } else {
      enlarged_new_size = s->capacity();
    }
    // This calls absl::strings_internal::StringResizeAndOverwriteImpl() because
    // the public API absl::StringResizeAndOverwrite() verifies that the
    // required range has been initialized. No other code should be calling
    // absl::strings_internal::StringResizeAndOverwriteImpl(). Instead it should
    // be implemented correctly with absl::StringResizeAndOverwrite().
    absl::strings_internal::StringResizeAndOverwriteImpl(
        *s, enlarged_new_size,
        [new_size](typename string_type::value_type*, size_t) {
          // TODO: b/446221957 - It is undefined behavior if any character in
          // the range [0, return_value) is uninitialized, but we rely on this
          // here to implement the old STLStringResizeUninitializedAmortized()
          // API.
          return new_size;
        });
  } else {
    s->erase(new_size);
  }
}

}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_INTERNAL_RESIZE_UNINITIALIZED_H_
