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

#ifndef ABSL_HASH_INTERNAL_WEAKLY_MIXED_INTEGER_H_
#define ABSL_HASH_INTERNAL_WEAKLY_MIXED_INTEGER_H_

#include <cstddef>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

// Contains an integer that will be mixed into a hash state more weakly than
// regular integers. It is useful for cases in which an integer is a part of a
// larger object and needs to be mixed as a supplement. E.g., absl::string_view
// and absl::Span are mixing their size wrapped with WeaklyMixedInteger.
struct WeaklyMixedInteger {
  size_t value;
};

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_HASH_INTERNAL_WEAKLY_MIXED_INTEGER_H_
