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
#include <algorithm>
#include <iterator>
#include <string>

#include <spdlog/fmt/fmt.h>

#include "LIEF/utils.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/version.h"

#include "third-party/utfcpp.hpp"

#include "LIEF/config.h"

#include "logging.hpp"
#include "messages.hpp"
#include "internal_utils.hpp"

namespace LIEF {

template <typename octet_iterator>
result<uint32_t> next(octet_iterator& it, octet_iterator end) {
  using namespace utf8;
  utfchar32_t cp = 0;
  internal::utf_error err_code = internal::validate_next(it, end, cp);
  switch (err_code) {
    case internal::UTF8_OK :
      break;
    case internal::NOT_ENOUGH_ROOM :
      return make_error_code(lief_errors::data_too_large);
    case internal::INVALID_LEAD :
    case internal::INCOMPLETE_SEQUENCE :
    case internal::OVERLONG_SEQUENCE :
      return make_error_code(lief_errors::read_error);
    case internal::INVALID_CODE_POINT :
      return make_error_code(lief_errors::read_error);
  }
  return cp;
}

std::string u16tou8(const std::u16string& string, bool remove_null_char) {
  std::string name;

  std::u16string clean_string;
  std::copy_if(std::begin(string), std::end(string),
               std::back_inserter(clean_string),
               utf8::internal::is_code_point_valid);

  utf8::unchecked::utf16to8(std::begin(clean_string), std::end(clean_string),
                            std::back_inserter(name));

  if (remove_null_char) {
    return std::string{name.c_str()};
  }
  return name;
}

result<std::u16string> u8tou16(const std::string& string) {
  std::u16string name;
  auto start = string.begin();
  auto end   = string.end();
  auto res   = std::back_inserter(name);
  while (start < end) {
    auto cp = next(start, end);
    if (!cp) {
      return make_error_code(lief_errors::conversion_error);
    }
    uint32_t cp_val = *cp;
    if (cp_val > 0xffff) {
      *res++ = static_cast<uint16_t>((cp_val >> 10)   + utf8::internal::LEAD_OFFSET);
      *res++ = static_cast<uint16_t>((cp_val & 0x3ff) + utf8::internal::TRAIL_SURROGATE_MIN);
    } else {
      *res++ = static_cast<uint16_t>(cp_val);
    }
  }
  return name;
}

inline std::string pretty_hex(char c) {
  if (is_printable(c)) {
    return std::string("") + c;
  }
  return ".";
}

std::string dump(const uint8_t* buffer, size_t size, const std::string& title,
                 const std::string& prefix, size_t limit)
{
  if (size < 2) {
    return "";
  }

  std::string out;
  std::string banner;

  if (!title.empty()) {
    banner  = prefix + "+" + std::string(22 * 3, '-') + "---+" + "\n" + prefix + "| ";
    banner += title;
    if (title.size() < 68) {
      banner += std::string(68 - title.size(), ' ');
    }
    banner += "|\n";
  }

  out = std::string(22 * 3, '-') + "\n";
  std::string lhs, rhs;
  if (limit > 0) {
    size = std::min<size_t>(size, limit);
  }

  for (size_t i = 0; i < size; ++i) {
    if (i == 0) {
      out = prefix + "+" + std::string(22 * 3, '-') + "---+" + "\n" + prefix + "| ";
    }

    if (i > 0 && i % 16 == 0) {
      out += "\n" + prefix + "| ";
    }

    rhs += pretty_hex((char)(buffer[i]));
    out += fmt::format("{:02x} ", buffer[i]);

    if (i % 16 == 15 || i == (size - 1)) {
      if (i == (size - 1)) {
        out += std::string(((16 - ((size - 1) % 16) - 1)) * 3, ' ');
        rhs += std::string(((16 - ((size - 1) % 16) - 1)) * 1, ' ');
      }
      out += " | ";
      out += rhs + " |";
      rhs = "";
    }

  }

  out += std::string("\n") + prefix + '+' + std::string(22 * 3, '-') + "---+";
  return banner + out;
}


bool is_extended() {
  return lief_extended;
}

std::string lief_version_t::to_string() const {
  if (id == 0) {
    return fmt::format("{}.{}.{}", major, minor, patch);
  }
  return fmt::format("{}.{}.{}.{}", major, minor, patch, id);
}

lief_version_t version() {
  if (lief_extended) {
    return extended_version();
  }
  return {LIEF_VERSION_MAJOR, LIEF_VERSION_MINOR, LIEF_VERSION_PATCH, 0};
}

#if !defined(LIEF_EXTENDED)
result<std::string> demangle(const std::string&/*mangled*/) {
  logging::needs_lief_extended();
  return make_error_code(lief_errors::require_extended_version);
}
#endif

#if !defined(LIEF_EXTENDED)
std::string extended_version_info() {
  return "";
}
#endif

#if !defined(LIEF_EXTENDED)
lief_version_t extended_version() {
  return {};
}
#endif

} // namespace LIEF
