// Copyright 2025 The Abseil Authors.
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

#include "absl/strings/internal/generic_printer.h"

#include <cstddef>
#include <cstdlib>
#include <ostream>
#include <string>

#include "absl/base/config.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace internal_generic_printer {

// Out-of-line helper for PrintAsStringWithEscaping.
std::ostream& PrintEscapedString(std::ostream& os, absl::string_view v) {
  return os << "\"" << absl::CHexEscape(v) << "\"";
}

// Retuns a string representation of 'v', shortened if possible.
template <class T, class F>
std::string TryShorten(T v, F strtox) {
  std::string printed =
      absl::StrFormat("%.*g", std::numeric_limits<T>::max_digits10 / 2, v);
  T parsed = strtox(printed.data());
  if (parsed != v) {
    printed =
        absl::StrFormat("%.*g", std::numeric_limits<T>::max_digits10 + 1, v);
  }
  return printed;
}

// Out-of-line helpers for floating point values. These don't necessarily
// ensure that values are precise, but rather that they are wide enough to
// represent distinct values. go/c++17std/numeric.limits.members.html
std::ostream& PrintPreciseFP(std::ostream& os, float v) {
  // TryShorten formats with absl::StrFormat(), which is locale-independent and
  // always emits a '.' radix. Use absl::SimpleAtof() for locale-independent
  // parsing.
  return os << TryShorten(v, [](const char* buf) {
           float out = 0;
           static_cast<void>(absl::SimpleAtof(buf, &out));
           return out;
         }) << "f";
}
std::ostream& PrintPreciseFP(std::ostream& os, double v) {
  // TryShorten formats with absl::StrFormat(), which is locale-independent and
  // always emits a '.' radix. Use absl::SimpleAtod() for locale-independent
  // parsing.
  return os << TryShorten(v, [](const char* buf) {
           double out = 0;
           static_cast<void>(absl::SimpleAtod(buf, &out));
           return out;
         });
}
std::ostream& PrintPreciseFP(std::ostream& os, long double v) {
  // No locale-independent long double parser is available, so this path keeps
  // std::strtold and remains locale-sensitive.
  return os << TryShorten(v, [](const char* buf) {
           char* unused;
           return std::strtold(buf, &unused);
         }) << "L";
}

// Prints a nibble of 'v' in hexadecimal.
inline char hexnib(int v) {
  return static_cast<char>((v < 10 ? '0' : ('a' - 10)) + v);
}

template <typename T>
static std::ostream& PrintCharImpl(std::ostream& os, T v) {
  // Specialization for chars: print as 'c' if printable, otherwise
  // hex-escaped.
  return (absl::ascii_isprint(static_cast<unsigned char>(v))
              ? (os << (v == '\'' ? "'\\" : "'") << v)
              : (os << "'\\x" << hexnib((v >> 4) & 0xf) << hexnib(v & 0xf)))
         << "' (0x" << hexnib((v >> 4) & 0xf) << hexnib(v & 0xf) << " "
         << static_cast<int>(v) << ")";
}

std::ostream& PrintChar(std::ostream& os, char c) {
  return PrintCharImpl(os, c);
}

std::ostream& PrintChar(std::ostream& os, signed char c) {
  return PrintCharImpl(os, c);
}

std::ostream& PrintChar(std::ostream& os, unsigned char c) {
  return PrintCharImpl(os, c);
}

std::ostream& PrintByte(std::ostream& os, std::byte b) {
  auto v = std::to_integer<int>(b);
  os << "0x" << hexnib((v >> 4) & 0xf) << hexnib(v & 0xf);
  return os;
}

}  // namespace internal_generic_printer
ABSL_NAMESPACE_END
}  // namespace absl
