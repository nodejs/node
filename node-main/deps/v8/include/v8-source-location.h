// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_SOURCE_LOCATION_H_
#define INCLUDE_SOURCE_LOCATION_H_

#include <cstddef>
#include <source_location>
#include <string>

#include "v8config.h"  // NOLINT(build/include_directory)

#define V8_SUPPORTS_SOURCE_LOCATION 1

namespace v8 {

/**
 * Encapsulates source location information. Mimics C++20's
 * `std::source_location`.
 */
class V8_EXPORT SourceLocation final {
 public:
  /**
   * Construct source location information corresponding to the location of the
   * call site.
   */
  static constexpr SourceLocation Current(
      const std::source_location& loc = std::source_location::current()) {
    return SourceLocation(loc);
  }
#ifdef DEBUG
  static constexpr SourceLocation CurrentIfDebug(
      const std::source_location& loc = std::source_location::current()) {
    return SourceLocation(loc);
  }
#else
  static constexpr SourceLocation CurrentIfDebug() { return {}; }
#endif

  /**
   * Constructs unspecified source location information.
   */
  constexpr SourceLocation() = default;

  /**
   * Returns the name of the function associated with the position represented
   * by this object, if any.
   *
   * \returns the function name as cstring.
   */
  constexpr const char* Function() const { return loc_.function_name(); }

  /**
   * Returns the name of the current source file represented by this object.
   *
   * \returns the file name as cstring.
   */
  constexpr const char* FileName() const { return loc_.file_name(); }

  /**
   * Returns the line number represented by this object.
   *
   * \returns the line number.
   */
  constexpr size_t Line() const { return loc_.line(); }

  /**
   * Returns a human-readable string representing this object.
   *
   * \returns a human-readable string representing source location information.
   */
  std::string ToString() const {
    if (loc_.line() == 0) {
      return {};
    }
    return std::string(loc_.function_name()) + "@" + loc_.file_name() + ":" +
           std::to_string(loc_.line());
  }

 private:
  constexpr explicit SourceLocation(const std::source_location& loc)
      : loc_(loc) {}

  std::source_location loc_;
};

}  // namespace v8

#endif  // INCLUDE_SOURCE_LOCATION_H_
