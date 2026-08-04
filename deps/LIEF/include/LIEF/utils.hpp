/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_UTILS_HEADER
#define LIEF_UTILS_HEADER
#include <ostream>
#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/errors.hpp"

namespace LIEF {
inline uint64_t align(uint64_t value, uint64_t align_on) {
  if (align_on == 0) {
    return value;
  }
  const auto r = value % align_on;
  if (r > 0) {
    return value + (align_on - r);
  }
  return value;
}

inline uint64_t align_down(uint64_t value, uint64_t align_on) {
  if (align_on == 0) {
    return value;
  }
  const auto r = value % align_on;
  if (r > 0) {
    return value - r;
  }
  return value;
}

template<typename T>
inline constexpr T round(T x) {
  return static_cast<T>(round<uint64_t>(x));
}


template<>
inline uint64_t round<uint64_t>(uint64_t x) {
  //From http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  x--;
  x |= x >> 1;  // handle  2 bit numbers
  x |= x >> 2;  // handle  4 bit numbers
  x |= x >> 4;  // handle  8 bit numbers
  x |= x >> 8;  // handle 16 bit numbers
  x |= x >> 16; // handle 32 bit numbers
  x |= x >> 32; // handle 64 bit numbers
  x++;
  return x;
}


constexpr size_t operator ""_KB(unsigned long long kbs)
{
    return 1024LLU * kbs;
}

constexpr size_t operator ""_MB(unsigned long long mbs)
{
    return 1024LLU * 1024LLU * mbs;
}

constexpr size_t operator ""_GB(unsigned long long gbs)
{
    return 1024LLU * 1024LLU * 1024LLU * gbs;
}

struct lief_version_t {
  uint64_t major = 0;
  uint64_t minor = 0;
  uint64_t patch = 0;
  uint64_t id    = 0;

  LIEF_API std::string to_string() const;

  friend LIEF_API
    std::ostream& operator<<(std::ostream& os, const lief_version_t& version)
  {
    os << version.to_string();
    return os;
  }
};

/// Convert a UTF-16 string to a UTF-8 one
LIEF_API std::string u16tou8(const std::u16string& string, bool remove_null_char = false);

/// Convert a UTF-8 string to a UTF-16 one
LIEF_API result<std::u16string> u8tou16(const std::string& string);

/// Whether this version of LIEF includes extended features
LIEF_API bool is_extended();

/// Details about the extended version
LIEF_API std::string extended_version_info();

/// Return the extended version
LIEF_API lief_version_t extended_version();

/// Return the current version
LIEF_API lief_version_t version();

/// Demangle the given input.
///
/// This function only works with the extended version of LIEF
LIEF_API result<std::string> demangle(const std::string& mangled);

/// Hexdump the provided buffer.
///
/// For instance:
///
/// ```text
/// +---------------------------------------------------------------------+
/// | 88 56 05 00 00 00 00 00 00 00 00 00 22 58 05 00  | .V.........."X.. |
/// | 10 71 02 00 78 55 05 00 00 00 00 00 00 00 00 00  | .q..xU.......... |
/// | 68 5c 05 00 00 70 02 00 00 00 00 00 00 00 00 00  | h\...p.......... |
/// | 00 00 00 00 00 00 00 00 00 00 00 00              | ............     |
/// +---------------------------------------------------------------------+
/// ```
LIEF_API std::string dump(
  const uint8_t* buffer, size_t size, const std::string& title = "",
  const std::string& prefix = "", size_t limit = 0);

inline std::string dump(span<const uint8_t> data, const std::string& title = "",
                        const std::string& prefix = "", size_t limit = 0)
{
  return dump(data.data(), data.size(), title, prefix, limit);
}

inline std::string dump(const std::vector<uint8_t>& data,
                        const std::string& title = "",
                        const std::string& prefix = "",
                        size_t limit = 0)
{
  return dump(data.data(), data.size(), title, prefix, limit);
}
}


#endif
