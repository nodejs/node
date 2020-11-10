// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_SOURCE_LOCATION_H_
#define INCLUDE_CPPGC_SOURCE_LOCATION_H_

#include <string>

#include "v8config.h"  // NOLINT(build/include_directory)

#if defined(__has_builtin)
#define CPPGC_SUPPORTS_SOURCE_LOCATION                                   \
  (__has_builtin(__builtin_FUNCTION) && __has_builtin(__builtin_FILE) && \
   __has_builtin(__builtin_LINE))  // NOLINT
#elif defined(V8_CC_GNU) && __GNUC__ >= 7
#define CPPGC_SUPPORTS_SOURCE_LOCATION 1
#elif defined(V8_CC_INTEL) && __ICC >= 1800
#define CPPGC_SUPPORTS_SOURCE_LOCATION 1
#else
#define CPPGC_SUPPORTS_SOURCE_LOCATION 0
#endif

namespace cppgc {

// Encapsulates source location information. Mimics C++20's
// std::source_location.
class V8_EXPORT SourceLocation final {
 public:
#if CPPGC_SUPPORTS_SOURCE_LOCATION
  static constexpr SourceLocation Current(
      const char* function = __builtin_FUNCTION(),
      const char* file = __builtin_FILE(), size_t line = __builtin_LINE()) {
    return SourceLocation(function, file, line);
  }
#else
  static constexpr SourceLocation Current() { return SourceLocation(); }
#endif  // CPPGC_SUPPORTS_SOURCE_LOCATION

  constexpr SourceLocation() = default;

  constexpr const char* Function() const { return function_; }
  constexpr const char* FileName() const { return file_; }
  constexpr size_t Line() const { return line_; }

  std::string ToString() const;

 private:
  constexpr SourceLocation(const char* function, const char* file, size_t line)
      : function_(function), file_(file), line_(line) {}

  const char* function_ = nullptr;
  const char* file_ = nullptr;
  size_t line_ = 0u;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_SOURCE_LOCATION_H_
