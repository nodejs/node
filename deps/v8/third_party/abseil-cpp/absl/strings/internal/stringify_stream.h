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
//
// -----------------------------------------------------------------------------
// File: stringify_stream.h
// -----------------------------------------------------------------------------

#ifndef ABSL_STRINGS_INTERNAL_STRINGIFY_STREAM_H_
#define ABSL_STRINGS_INTERNAL_STRINGIFY_STREAM_H_

// StringifyStream is an adaptor for any std::ostream, that provides a
// stream insertion (<<) operator with the following behavior when inserting
// some value of type T:
//
//   - If there is a suitable overload of operator<< already defined for T, it
//     will be used.
//
//   - If there is no operator<< overload, but there is an AbslStringify defined
//     for T, it will be used as a fallback.
//
//   - Otherwise it is a compilation error.
//
// For reference, AbslStringify typically has the form:
//
//   struct Foo {
//     template <typename Sink>
//     friend void AbslStringify(Sink& sink, const Foo& foo) { ... }
//   };
//
// This permits the following usage, for example:
//
//   StringifyStream(std::cout) << Foo();
//

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace strings_internal {

class StringifyStream {
 public:
  // Constructor: adapts (but does not take ownership of) some underlying
  // std::ostream instance.
  explicit StringifyStream(std::ostream& os) : sink_{os} {}

  // Stream insertion: delegate to an overload of operator<< if defined for type
  // T, otherwise fall back to AbslStringify for T.
  template <typename T>
  friend StringifyStream& operator<<(StringifyStream& stream, const T& t) {
    if constexpr (HasStreamInsertion<T>::value) {
      stream.sink_.os << t;
    } else {
      AbslStringify(stream.sink_, t);
    }
    return stream;
  }

  // Rvalue-ref overload, required when the StringifyStream parameter hasn't
  // been bound to a variable.
  template <typename T>
  friend StringifyStream& operator<<(StringifyStream&& stream, const T& t) {
    return stream << t;
  }

  // Overload for things like << std::endl which need an explicit type in order
  // to resolve to the appropriate overload or template instantiation.
  StringifyStream& operator<<(std::ostream& (*func)(std::ostream&)) {
    sink_.os << func;
    return *this;
  }

 private:
  // Abseil "stringify sink" concept (stringify_sink.h)
  struct Sink {
    std::ostream& os;
    void Append(size_t count, char ch) { os << std::string(count, ch); }
    void Append(absl::string_view v) { os << v; }
    friend void AbslFormatFlush(Sink* sink, absl::string_view v) {
      sink->Append(v);
    }
  } sink_;

  // SFINAE helper to identify types with a defined operator<< overload.
  template <typename T, typename = void>
  struct HasStreamInsertion : std::false_type {};

  template <typename T>
  struct HasStreamInsertion<T,
                            std::void_t<decltype(std::declval<std::ostream&>()
                                                 << std::declval<const T&>())>>
      : std::true_type {};
};

}  // namespace strings_internal

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_INTERNAL_STRINGIFY_STREAM_H_
