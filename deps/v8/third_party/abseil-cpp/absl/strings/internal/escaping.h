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

#ifndef ABSL_STRINGS_INTERNAL_ESCAPING_H_
#define ABSL_STRINGS_INTERNAL_ESCAPING_H_

#include <cstddef>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

// Calculates the length of a Base64 encoding (RFC 4648) of a string of length
// `input_len`, with or without padding per `do_padding`. Note that 'web-safe'
// encoding (section 5 of the RFC) does not change this length.
size_t CalculateBase64EscapedLenInternal(size_t input_len, bool do_padding);

}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_INTERNAL_ESCAPING_H_
