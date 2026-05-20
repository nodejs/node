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
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/DWARF/enums.hpp"
#include "third-party/utfcpp.hpp"

#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>

namespace LIEF {


result<int64_t> BinaryStream::read_dwarf_encoded(uint8_t encoding) const {
  const auto encodevalue =  static_cast<LIEF::dwarf::EH_ENCODING>(encoding & 0x0F);

  switch (encodevalue) {
    case LIEF::dwarf::EH_ENCODING::ULEB128:
      {
        return read_uleb128();
      }

    case LIEF::dwarf::EH_ENCODING::SDATA2:
    case LIEF::dwarf::EH_ENCODING::UDATA2:
      {
        return read<int16_t>();
      }

    case LIEF::dwarf::EH_ENCODING::SDATA4:
    case LIEF::dwarf::EH_ENCODING::UDATA4:
      {
        return read<int32_t>();
      }

    case LIEF::dwarf::EH_ENCODING::SDATA8:
    case LIEF::dwarf::EH_ENCODING::UDATA8:
      {
        return read<int64_t>();
      }

    case LIEF::dwarf::EH_ENCODING::SLEB128:
      {
        return read_sleb128();
      }

    default:
      {
        return 0;
      }
  }

}

result<uint64_t> BinaryStream::read_uleb128(size_t* size) const {
  uint64_t value = 0;
  unsigned shift = 0;
  result<uint8_t> byte_read = 0;

  const uint64_t opos = pos();

  do {
    byte_read = read<uint8_t>();
    if (!byte_read) {
      return make_error_code(lief_errors::read_error);
    }
    value += static_cast<uint64_t>(*byte_read & 0x7f) << shift;
    shift += 7;
  } while (byte_read && *byte_read >= 128);

  if (size != nullptr) {
    *size = pos() - opos;
  }

  return value;
}

result<uint64_t> BinaryStream::read_sleb128(size_t* size) const {
  int64_t  value = 0;
  unsigned shift = 0;
  result<uint8_t> byte_read = 0;
  const uint64_t opos = pos();
  do {
    byte_read = read<uint8_t>();
    if (!byte_read) {
      return make_error_code(lief_errors::read_error);
    }
    value += static_cast<int64_t>(*byte_read & 0x7f) << shift;
    shift += 7;
  } while (byte_read && *byte_read >= 128);


  // Sign extend
  if ((*byte_read & 0x40) != 0) {
    value |= -1llu << shift;
  }

  if (size != nullptr) {
    *size = pos() - opos;
  }

  return value;
}

result<std::string> BinaryStream::read_string(size_t maxsize) const {
  result<std::string> str = peek_string(maxsize);
  if (!str) {
    return str;
  }
  increment_pos(str->size() + 1); // +1 for'\0'
  return str;
}

result<std::string> BinaryStream::peek_string(size_t maxsize) const {
  std::string str_result;
  str_result.reserve(10);
  result<char> c = '\0';
  size_t off = pos();

  if (!can_read<char>()) {
    return str_result;
  }

  size_t count = 0;
  do {
    c = peek<char>(off);
    if (!c) {
      return make_error_code(c.error());
    }
    off += sizeof(char);
    str_result.push_back(*c);
    ++count;
  } while (count < maxsize && c && *c != '\0' && off < size());
  str_result.back() = '\0';
  return str_result.c_str();

}

result<std::string> BinaryStream::peek_string_at(size_t offset, size_t maxsize) const {
  size_t saved_offset = pos();
  setpos(offset);
  result<std::string> tmp = peek_string(maxsize);
  setpos(saved_offset);
  return tmp;
}

result<std::u16string> BinaryStream::read_u16string() const {
  result<std::u16string> str = peek_u16string();
  if (!str) {
    return str;
  }
  increment_pos((str->size() + 1) * sizeof(uint16_t)); // +1 for'\0'
  return str.value();
}

result<std::u16string> BinaryStream::peek_u16string() const {
  std::u16string u16_str;
  u16_str.reserve(10);
  result<char16_t> c = char16_t{0};
  size_t off = pos();

  if (!can_read<char16_t>()) {
    return u16_str;
  }

  do {
    c = peek<char16_t>(off);
    if (!c) {
      return make_error_code(c.error());
    }
    off += sizeof(char16_t);
    u16_str.push_back(*c);
  } while (c && *c != 0 && off < size());
  u16_str.back() = '\0';
  return u16_str.c_str();
}


result<std::u16string> BinaryStream::read_u16string(size_t length) const {
  result<std::u16string> str = peek_u16string(length);
  increment_pos(length * sizeof(uint16_t)); // +1 for'\0'
  return str;
}

result<std::u16string> BinaryStream::peek_u16string(size_t length) const {
  if (length == static_cast<size_t>(SIZE_MAX)) {
    return peek_u16string();
  }

  std::vector<char16_t> raw_u16str;
  raw_u16str.resize(length, 0);
  if (peek_in(raw_u16str.data(), pos(), length * sizeof(char16_t))) {
    if (endian_swap_) {
      for (char16_t& x : raw_u16str) {
        LIEF::swap_endian(&x);
      }
    }
    return std::u16string{std::begin(raw_u16str), std::end(raw_u16str)};
  }
  return make_error_code(lief_errors::read_error);
}

result<std::u16string> BinaryStream::peek_u16string_at(size_t offset, size_t length) const {
  size_t saved_offset = pos();
  setpos(offset);
  result<std::u16string> tmp = peek_u16string(length);
  setpos(saved_offset);
  return tmp;
}


size_t BinaryStream::align(size_t align_on) const {
  if (align_on == 0) {
    return 0;
  }
  const size_t r = pos() % align_on;
  if (r == 0) {
    return 0;
  }
  size_t padding = align_on - r;
  increment_pos(padding);
  return padding;
}


result<std::string> BinaryStream::read_mutf8(size_t maxsize) const {
  std::u32string u32str;

  for (size_t i = 0; i < maxsize; ++i) {
    result<uint8_t> res_a = read<char>();
    if (!res_a) {
      return make_error_code(res_a.error());
    }
    uint8_t a = *res_a;

    if (static_cast<uint8_t>(a) < 0x80) {
      if (a == 0) {
        break;
      }
      u32str.push_back(a);
    } else if ((a & 0xe0) == 0xc0) {

      result<uint8_t> res_b = read<int8_t>();
      if (!res_b) {
        return make_error_code(res_b.error());
      }
      uint8_t b = *res_b & 0xFF;

      if ((b & 0xC0) != 0x80) {
        break;
      }
      u32str.push_back(static_cast<char32_t>((((a & 0x1F) << 6) | (b & 0x3F))));
    } else if ((a & 0xf0) == 0xe0) {
        result<uint8_t> res_b = read<uint8_t>();
        result<uint8_t> res_c = read<uint8_t>();
        if (!res_b) {
          return make_error_code(res_b.error());
        }
        if (!res_c) {
          return make_error_code(res_c.error());
        }
        uint8_t b = *res_b;
        uint8_t c = *res_c;

        if (((b & 0xC0) != 0x80) || ((c & 0xC0) != 0x80)) {
          break;
        }
        u32str.push_back(static_cast<char32_t>(((a & 0x1F) << 12) | ((b & 0x3F) << 6) | (c & 0x3F)));
    } else {
      break;
    }
  }

  std::string u8str;

  std::replace_if(std::begin(u32str), std::end(u32str),
      [] (const char32_t c) {
        return !utf8::internal::is_code_point_valid(c);
      }, '.');

  utf8::unchecked::utf32to8(std::begin(u32str), std::end(u32str),
                            std::back_inserter(u8str));
  return u8str;
}

}
