// Copyright 2020 The Abseil Authors.
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

#ifndef ABSL_BASE_FAST_TYPE_ID_H_
#define ABSL_BASE_FAST_TYPE_ID_H_

#include <utility>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace base_internal {
template <typename Type>
struct FastTypeTag {
  static constexpr char kDummyVar = 0;
};
}  // namespace base_internal

// The type returned by `absl::FastTypeId<T>()`.
class FastTypeIdType {
 public:
  // Creates a value that does not correspond to any type. This value is
  // distinct from any value returned by `FastTypeId<T>()`.
  constexpr FastTypeIdType() = default;

  template <typename H>
  friend H AbslHashValue(H h, FastTypeIdType x) {
    return H::combine(std::move(h), x.ptr_);
  }

  friend constexpr bool operator==(FastTypeIdType a, FastTypeIdType b) {
    return a.ptr_ == b.ptr_;
  }
  friend constexpr bool operator!=(FastTypeIdType a, FastTypeIdType b) {
    return a.ptr_ != b.ptr_;
  }

 private:
  // `FastTypeId<T>()` is the generator method for FastTypeIdType values.
  template <typename T>
  friend constexpr FastTypeIdType FastTypeId();

  explicit constexpr FastTypeIdType(const void* ptr) : ptr_(ptr) {}

  const void* ptr_ = nullptr;
};

// `absl::FastTypeId<Type>()` evaluates at compile-time to a unique id for the
// passed-in type. These are meant to be good match for keys into maps or
// straight up comparisons.
template <typename Type>
constexpr FastTypeIdType FastTypeId() {
  return FastTypeIdType(&base_internal::FastTypeTag<Type>::kDummyVar);
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_FAST_TYPE_ID_H_
