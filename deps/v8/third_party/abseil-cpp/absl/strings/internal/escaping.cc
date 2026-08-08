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

#include "absl/strings/internal/escaping.h"

#include <limits>

#include "absl/base/internal/endian.h"
#include "absl/base/internal/raw_logging.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

size_t CalculateBase64EscapedLenInternal(size_t input_len, bool do_padding) {
  // Base64 encodes three bytes of input at a time. If the input is not
  // divisible by three, we pad as appropriate.
  //
  // Base64 encodes each three bytes of input into four bytes of output.
  constexpr size_t kMaxSize = (std::numeric_limits<size_t>::max() - 1) / 4 * 3;
  ABSL_INTERNAL_CHECK(input_len <= kMaxSize,
                      "CalculateBase64EscapedLenInternal() overflow");
  size_t len = (input_len / 3) * 4;

  // Since all base 64 input is an integral number of octets, only the following
  // cases can arise:
  if (input_len % 3 == 0) {
    // (from https://tools.ietf.org/html/rfc3548)
    // (1) the final quantum of encoding input is an integral multiple of 24
    // bits; here, the final unit of encoded output will be an integral
    // multiple of 4 characters with no "=" padding,
  } else if (input_len % 3 == 1) {
    // (from https://tools.ietf.org/html/rfc3548)
    // (2) the final quantum of encoding input is exactly 8 bits; here, the
    // final unit of encoded output will be two characters followed by two
    // "=" padding characters, or
    len += 2;
    if (do_padding) {
      len += 2;
    }
  } else {  // (input_len % 3 == 2)
    // (from https://tools.ietf.org/html/rfc3548)
    // (3) the final quantum of encoding input is exactly 16 bits; here, the
    // final unit of encoded output will be three characters followed by one
    // "=" padding character.
    len += 3;
    if (do_padding) {
      len += 1;
    }
  }

  return len;
}

}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl
