// Copyright 2026 The Abseil Authors.
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

#ifndef ABSL_EXTEND_INTERNAL_REFLECTION_H_
#define ABSL_EXTEND_INTERNAL_REFLECTION_H_

#include <array>
#include <cstddef>

#include "absl/base/config.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

#if defined(ABSL_EXTEND_INTERNAL_TURN_OFF_FIELD_NAMES_FOR_TEST)
#define ABSL_INTERNAL_EXTEND_PARSE_FIELD_NAMES 0
#elif defined(TARGET_OS_OSX) || defined(__ANDROID__)
// TODO(b/234010485): Implement field names for these.
#define ABSL_INTERNAL_EXTEND_PARSE_FIELD_NAMES 0
#elif ABSL_HAVE_BUILTIN(__builtin_dump_struct)
#define ABSL_INTERNAL_EXTEND_PARSE_FIELD_NAMES 1
#else
#define ABSL_INTERNAL_EXTEND_PARSE_FIELD_NAMES 0
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace extend_internal {

struct ParsingState {
  // The index of the field currently being parsed.
  int index = 0;
  // The number of unmatched '{' characters encountered so far while parsing.
  int brace_count = 0;
};

// Called by` __builtin_dump_struct` for each line of the struct representation.
// Rather than printing the struct representation, we parse `fmt` to extract the
// lines containing struct field names and store them in `T::kFieldNames`.
// Note: `__builtin_dump_struct` provides no guarantees whatsoever on its
// output, so our parsing is entirely best-effort.
int PrintfHijack(ParsingState& state, absl::Span<absl::string_view> fields,
                 const char* fmt, ...);

// Forward declaration of test helper.
template <typename T>
struct ReflectionTestingExtension;

// Not nested in ReflectionExtension to reduce template instantiations.
template <size_t N>
struct FieldNameInfo {
  static constexpr size_t kFieldCount = N;

#if ABSL_INTERNAL_EXTEND_PARSE_FIELD_NAMES
  std::array<absl::string_view, kFieldCount> field_names;
#endif
  // Indicates whether parsing was successful.
  bool success;
};

}  // namespace extend_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_REFLECTION_H_
