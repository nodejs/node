// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_FLAGS_H_
#define V8_REGEXP_REGEXP_FLAGS_H_

#include <optional>
#include <ostream>

#include "src/base/flags.h"

namespace v8::internal::regexp {

// TODO(jgruber,pthier): Decouple more parts of the codebase from
// JSRegExp::Flags. Consider removing JSRegExp::Flags.

// Order is important! Sorted in alphabetic order by the flag char. Note this
// means that flag bits are shuffled. Take care to keep them contiguous when
// adding/removing flags.
#define REGEXP_FLAG_LIST(V)                         \
  V(has_indices, HasIndices, hasIndices, 'd', 7)    \
  V(global, Global, global, 'g', 0)                 \
  V(ignore_case, IgnoreCase, ignoreCase, 'i', 1)    \
  V(linear, Linear, linear, 'l', 6)                 \
  V(multiline, Multiline, multiline, 'm', 2)        \
  V(dot_all, DotAll, dotAll, 's', 5)                \
  V(unicode, Unicode, unicode, 'u', 4)              \
  V(unicode_sets, UnicodeSets, unicodeSets, 'v', 8) \
  V(sticky, Sticky, sticky, 'y', 3)

#define V(Lower, Camel, LowerCamel, Char, Bit) k##Camel = 1 << Bit,
enum class Flag { REGEXP_FLAG_LIST(V) };
#undef V

#define V(...) +1
constexpr int kFlagCount = REGEXP_FLAG_LIST(V);
#undef V

// Assert alpha-sorted chars.
#define V(Lower, Camel, LowerCamel, Char, Bit) < Char) && (Char
static_assert((('a' - 1) REGEXP_FLAG_LIST(V) <= 'z'), "alpha-sort chars");
#undef V

// Assert contiguous indices.
#define V(Lower, Camel, LowerCamel, Char, Bit) | (1 << Bit)
static_assert(((1 << kFlagCount) - 1) == (0 REGEXP_FLAG_LIST(V)),
              "contiguous bits");
#undef V

using Flags = base::Flags<Flag>;
DEFINE_OPERATORS_FOR_FLAGS(Flags)

#define V(Lower, Camel, ...) \
  constexpr bool Is##Camel(Flags f) { return (f & Flag::k##Camel) != 0; }
REGEXP_FLAG_LIST(V)
#undef V

constexpr bool IsEitherUnicode(Flags f) {
  return IsUnicode(f) || IsUnicodeSets(f);
}

// Whether to rewind the index when it initially points into the middle of a
// surrogate pair. See also OptionallyStepBackToLeadSurrogate().
constexpr bool ShouldOptionallyStepBackToLeadSurrogate(Flags f) {
  return IsEitherUnicode(f) && (IsGlobal(f) || IsSticky(f));
}

// clang-format off
#define V(Lower, Camel, LowerCamel, Char, Bit) \
  c == Char ? Flag::k##Camel :
constexpr std::optional<Flag> TryFlagFromChar(char c) {
  return REGEXP_FLAG_LIST(V) std::optional<Flag>{};
}
#undef V
// clang-format on

std::ostream& operator<<(std::ostream& os, Flags flags);

}  // namespace v8::internal::regexp

#endif  // V8_REGEXP_REGEXP_FLAGS_H_
