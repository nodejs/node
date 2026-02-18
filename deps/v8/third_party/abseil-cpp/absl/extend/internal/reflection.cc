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

#include "absl/extend/internal/reflection.h"

#include <stddef.h>

#include <cstdarg>

#include "absl/base/config.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace extend_internal {

// `PrintfHijack` is called repeatedly with each `fmt` line and corresponding
// arguments `va` via `__builtin_dump_struct`. While there is no guarantee about
// the `fmt` strings provided, it is typical that lines come in 5 flavors:
// 1. A string containing only "%s", accepting the name of the struct itself.
// 2. A string containing only "%s%s", accepting the name of a base class.
// 3. A string containing only whitespace and an open brace "{" indicating that
//    we are entering a new struct.
// 4. A string with "=" where the value passed into the format specifier which
//    immediately precedes "=" is the name of a struct field.
// 5. A string ending with a "}" indicating the conclusion of a struct.
//
// For example, for a struct
// ```
// struct Bacon {
//   int repeatability;
//   double flavor;
//   double fattiness;
//   struct Metadata {
//     bool is_pork;
//     bool is_expired;
//   } metadata;
// };
// ```
// we would expect the following values for `fmt` to be passed to `ParseLine`,
// along with the following data in `va`:
//     `fmt`           |    `va`
// --------------------|-------------
//   "%s"              | {"Bacon"}
//   " {\n"            | {}
//   "%s%s %s = %d\n"  | {"  ", "int", "repeatability", ???}
//   "%s%s %s = %f\n"  | {"  ", "double", "flavor", ???}
//   "%s%s %s = %f\n"  | {"  ", "double", "fattiness", ???}
//   "%s%s %s ="       | {"  ", "Metadata", "metadata"}
//   " {\n"            | {}
//   "%s%s %s = %d\n"  | {"  ", "_Bool", "is_pork", ???}
//   "%s%s %s = %d\n"  | {"  ", "_Bool", "is_expired", ???}
//   "%s}\n"           | {"  "}
//   "}\n"             | {}
//
// `PrintfHijack` inspects each format string and argument set, extracting field
// names for the given struct and writes them to `fields`. If an unexpected
// format string is encountered, `state.index` will be set to -1 indicating a
// failure. Parsing succeeds if after every call to `PrintfHijack`,
// `state.index == fields.size()`.
int PrintfHijack(ParsingState& state, absl::Span<absl::string_view> fields,
                 const char* fmt, ...) {
  if (state.index < 0) return 0;

  if (absl::EndsWith(fmt, "{\n")) {
    ++state.brace_count;
    return 0;
  } else if (absl::EndsWith(fmt, "}") || absl::EndsWith(fmt, "}\n")) {
    --state.brace_count;
    return 0;
  } else if (state.brace_count != 1) {
    // Ignore everything that's not at the top-level (we only care about the
    // names of fields in this type.
    return 0;
  } else if (absl::StartsWith(fmt, "%s%s %s =")) {
    std::va_list va;
    va_start(va, fmt);

    static_cast<void>(va_arg(va, const char*));  // Indentation whitespace
    static_cast<void>(va_arg(va, const char*));  // Field's type name
    fields[static_cast<size_t>(state.index)] =
        va_arg(va, const char*);  // Field name
    ++state.index;

    va_end(va);
  } else if (fmt == absl::string_view("%s%s")) {
    // Ignore base classes.
    return 0;
  } else {
    // Unexpected case. Mark parsing as failed.
    state.index = -1;
  }

  return 0;
}

}  // namespace extend_internal
ABSL_NAMESPACE_END
}  // namespace absl
