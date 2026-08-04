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
#include "LIEF/iostream.hpp"

namespace LIEF {

size_t vector_iostream::uleb128_size(uint64_t value) {
  size_t size = 0;
  do {
    value >>= 7;
    size += sizeof(int8_t);
  } while(value != 0);
  return size;
}

size_t vector_iostream::sleb128_size(int64_t value) {
  size_t size = 0;
  int sign = value >> (8 * sizeof(value) - 1);
  bool is_more;
  do {
    size_t byte = value & 0x7F;
    value >>= 7;
    is_more = value != sign || ((byte ^ sign) & 0x40) != 0;
    size += sizeof(int8_t);
  } while (is_more);
  return size;
}

vector_iostream& vector_iostream::put(uint8_t c) {
  if (raw_->size() < (static_cast<size_t>(tellp()) + 1)) {
    raw_->resize(static_cast<size_t>(tellp()) + 1);
  }
  (*raw_)[tellp()] = c;
  current_pos_ += 1;
  return *this;
}

vector_iostream& vector_iostream::write(const uint8_t* s, std::streamsize n) {
  const auto pos = static_cast<size_t>(tellp());
  if (raw_->size() < (pos + n)) {
    raw_->resize(pos + n);
  }
  std::copy(s, s + n, raw_->data() + pos);
  current_pos_ += n;

  return *this;
}

vector_iostream& vector_iostream::write_uleb128(uint64_t value) {
  uint8_t byte;
  do {
    byte = value & 0x7F;
    value &= ~0x7F;
    if (value != 0) {
      byte |= 0x80;
    }
    write<uint8_t>(byte);
    value = value >> 7;
  } while (byte >= 0x80);

  return *this;
}

vector_iostream& vector_iostream::write_sleb128(int64_t value) {

  bool is_neg = (value < 0);
  uint8_t byte;
  bool more;
  do {
    byte = value & 0x7F;
    value = value >> 7;

    if (is_neg) {
      more = ((value != -1) || ((byte & 0x40) == 0));
    } else {
      more = ((value != 0) || ((byte & 0x40) != 0));
    }
    if (more) {
      byte |= 0x80;
    }
    write<uint8_t>(byte);
  } while (more);

  return *this;
}

vector_iostream& vector_iostream::seekp(vector_iostream::off_type p, std::ios_base::seekdir dir) {
  switch (dir) {
    case std::ios_base::beg: current_pos_ = p; return *this;
    case std::ios_base::end: return *this;
    case std::ios_base::cur: current_pos_ += p; return *this;
    default: return *this;
  }

  return *this;
}

vector_iostream& vector_iostream::write(const std::u16string& s, bool with_null_char) {
  const size_t nullchar = with_null_char ? 1 : 0;
  const auto pos = static_cast<size_t>(tellp());
  if (raw_->size() < (pos + s.size() + nullchar)) {
    raw_->resize(pos + (s.size() + nullchar) * sizeof(char16_t));
  }

  std::copy(reinterpret_cast<const char16_t*>(s.data()),
            reinterpret_cast<const char16_t*>(s.data()) + s.size(),
            reinterpret_cast<char16_t*>(raw_->data() + current_pos_));
  current_pos_ += (s.size() + nullchar) * sizeof(char16_t);
  return *this;
}

vector_iostream& vector_iostream::align(size_t alignment, uint8_t fill) {
  if (raw_->size() % alignment == 0) {
    return *this;
  }

  while (raw_->size() % alignment != 0) {
    write<uint8_t>(fill);
  }

  return *this;
}
}

