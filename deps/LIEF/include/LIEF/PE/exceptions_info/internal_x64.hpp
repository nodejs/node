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
#ifndef LIEF_PE_UNWIND_CODE_X64_INTERNAL_H
#define LIEF_PE_UNWIND_CODE_X64_INTERNAL_H
#include <cstdint>

namespace LIEF::PE::details {
union unwind_code_t {
  struct {
    uint8_t code_offset;
    uint8_t op_info;
  } opcode;
  uint16_t raw;

  uint8_t op() const {
    return opcode.op_info & 0x0F;
  }
  uint8_t op_info() const {
    return (opcode.op_info >> 4) & 0x0F;
  }
};

static_assert(sizeof(unwind_code_t) == sizeof(uint16_t));

}
#endif
