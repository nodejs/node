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
#ifndef LIEF_PE_UNWIND_CODE_ARM64_INTERNAL_H
#define LIEF_PE_UNWIND_CODE_ARM64_INTERNAL_H
#include <cstdint>
#include <array>

namespace LIEF::PE::details {
inline uint32_t xdata_unpacked_rva(uint32_t value) {
  return value & ~0x3;
}

struct arm64_packed_t {
  uint32_t raw;

  uint8_t flags() const {
    return raw & 0x03;
  }

  uint32_t function_length() const {
    return ((raw & 0x00001ffc) >> 2) << 2;
  }

  uint8_t RF() const {
    return ((raw & 0x0000e000) >> 13);
  }

  uint8_t RI() const {
    return ((raw & 0x000f0000) >> 16);
  }

  bool H() const {
    return ((raw & 0x00100000) >> 20);
  }

  uint8_t CR() const {
    return ((raw & 0x600000) >> 21);
  }

  uint8_t frame_size() const {
    return ((raw & 0xff800000) >> 23);
  }
};

static_assert(sizeof(arm64_packed_t) == sizeof(uint32_t));

struct arm64_unpacked_t {
  static constexpr auto MAX_WORDS = 2;
  std::array<uint32_t, MAX_WORDS> data = {0};

  uint32_t function_length() const {
    return (data[0] & 0x0003ffff) << 2;
  }

  uint8_t version() const {
    return (data[0] & 0x000C0000) >> 18;
  }

  bool X() const {
    return (data[0] & 0x00100000) >> 20;
  }

  uint8_t E() const {
    return (data[0] & 0x00200000) >> 21;
  }

  uint16_t epilog_count() const {
    return is_extended() ? (data[1] & 0x0000ffff) :
                           (data[0] & 0x07C00000) >> 22;
  }

  uint8_t code_words() const {
    return is_extended() ? (data[1] & 0x00ff0000) >> 16 :
                           (data[0] & 0xf8000000) >> 27;
  }

  bool is_extended() const {
    return (data[0] & 0xffc00000) == 0;
  }
};

struct arm64_epilog_scope_t {
  uint32_t raw;

  uint32_t start_offset() const {
    return raw & 0x0003ffff;
  }

  uint8_t reserved() const {
    return (raw & 0x000f0000) >> 18;
  }

  uint16_t start_index() const {
    return (raw & 0xffc00000) >> 22;
  }
};

}
#endif
