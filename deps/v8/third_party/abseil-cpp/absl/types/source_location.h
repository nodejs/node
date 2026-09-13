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
// -----------------------------------------------------------------------------
// File: source_location.h
// -----------------------------------------------------------------------------
//
// absl::SourceLocation provides source-code location info for C++17 and later.
//
// Critically, it is a **view type** (like std::string_view). Unlike
// std::source_location, it is not permanently valid and must not outlive its
// source. Using an invalid location is an error and may result in warnings
// or crashes.
//
// Additionally, it does not guarantee the retention of all caller information
// (e.g. column or function name) and may e.g. return unspecified values for
// performance reasons.
//
// To define a function that has access to the source location of the
// callsite, define it with a parameter of type `absl::SourceLocation`. The
// caller can then invoke the function, passing
// `absl::SourceLocation::current()` as the argument.
//
// If at all possible, make the `absl::SourceLocation` parameter be the
// function's last parameter. That way, when `std::source_location` is
// available, you will be able to switch to it, and give the parameter a default
// argument of `std::source_location::current()`. Users will then be able to
// omit that argument, and the default will automatically capture the location
// of the callsite.

#ifndef ABSL_TYPES_SOURCE_LOCATION_H_
#define ABSL_TYPES_SOURCE_LOCATION_H_

#include <cstdint>
#include <type_traits>

#include "absl/base/config.h"
#include "absl/base/nullability.h"

// This needs to come after absl/base/config.h, which is responsible for
// defining ABSL_HAVE_STD_SOURCE_LOCATION
#ifdef ABSL_HAVE_STD_SOURCE_LOCATION
#include <source_location>  // NOLINT(build/c++20)
#endif

// For OSS release, whether to alias to std::source_location is configurable via
// config.h/options.h, similar to std::string_view/variant/etc.
#if defined(ABSL_USES_STD_SOURCE_LOCATION) && \
    defined(ABSL_HAVE_STD_SOURCE_LOCATION)
namespace absl {
ABSL_NAMESPACE_BEGIN
using SourceLocation = std::source_location;
ABSL_NAMESPACE_END
}  // namespace absl

#else  // ABSL_HAVE_STD_SOURCE_LOCATION

namespace absl {
ABSL_NAMESPACE_BEGIN

// C++17-compatible class representing a specific location in the source code of
// a program. Similar to std::source_location, but with a few key differences
// explained above.
class SourceLocation {
  struct PrivateTag {
   private:
    explicit PrivateTag() = default;
    friend class SourceLocation;
  };

 public:
  // Avoid this constructor; it populates the object with dummy values.
  SourceLocation() = default;

#ifdef ABSL_HAVE_STD_SOURCE_LOCATION
  constexpr SourceLocation(  // NOLINT(google-explicit-constructor)
      std::source_location loc)
      : SourceLocation(loc.line(), loc.file_name()) {}
#endif

#ifdef ABSL_INTERNAL_HAVE_BUILTIN_LINE_FILE
  // SourceLocation::current
  //
  // Creates a `SourceLocation` based on the current source location. Currently,
  // it only captures file and line information for efficiency purposes, but
  // that is subject to change. APIs that accept a `SourceLocation` as a default
  // parameter can use this to capture their caller's locations.
  //
  // Example:
  //
  //   void TracedAdd(int i, SourceLocation loc = SourceLocation::current()) {
  //     std::cout << loc.file_name() << ":" << loc.line() << " added " << i;
  //     ...
  //   }
  //
  //   void UserCode() {
  //     TracedAdd(1);
  //     TracedAdd(2);
  //   }
  static constexpr SourceLocation current(
      PrivateTag = PrivateTag{}, std::uint_least32_t line = __builtin_LINE(),
      const char* absl_nonnull file_name = __builtin_FILE()) {
    return SourceLocation(line, file_name);
  }
#else
  // Creates a dummy `SourceLocation` of "<source_location>" at line number 1,
  // if no `SourceLocation::current()` implementation is available.
  static constexpr SourceLocation current() {
    return SourceLocation(1, "<source_location>");
  }
#endif
  // The line number of the captured source location, or an unspecified value
  // if this information is not available.
  constexpr std::uint_least32_t line() const noexcept { return line_; }

  // The column number of the captured source location, or an unspecified value
  // if this information is not available.
  constexpr std::uint_least32_t column() const noexcept { return 0; }

  // The file name of the captured source location, or an unspecified string
  // if this information is not available. Guaranteed to never be NULL.
  constexpr const char* absl_nonnull file_name() const noexcept {
    return file_name_;
  }

  // The function name of the captured source location, or an unspecified string
  // if this information is not available. Guaranteed to never be NULL.
  //
  // NOTE: Currently, we deliberately avoid providing the function name, as it
  // can bloat binary sizes and is non-critical. This may change in the future.
  constexpr const char* absl_nonnull function_name() const noexcept {
    return "";
  }

 private:
  // `file_name` must outlive all copies of the `absl::SourceLocation` object,
  // so in practice it should be a string literal.
  constexpr SourceLocation(std::uint_least32_t line,
                           const char* absl_nonnull file_name)
      : line_(line), file_name_(file_name) {}

  // We would use [[maybe_unused]] here, but it doesn't work on all supported
  // toolchains at the moment.
  friend constexpr int UseUnused() {
    static_assert(SourceLocation(0, nullptr).unused_column_ == 0,
                  "Use the otherwise-unused member.");
    return 0;
  }

  // "unused" members are present to minimize future changes in the size of this
  // type.
  std::uint_least32_t line_ = 0;
  std::uint_least32_t unused_column_ = 0;
  const char* absl_nonnull file_name_ = "";
};

ABSL_NAMESPACE_END
}  // namespace absl
#endif  // ABSL_HAVE_STD_SOURCE_LOCATION

#endif  // ABSL_TYPES_SOURCE_LOCATION_H_
