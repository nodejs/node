/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#ifndef LIEF_MACHO_DYLD_OPCODES_H
#define LIEF_MACHO_DYLD_OPCODES_H
#include <cstdint>

#include "logging.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"

namespace LIEF::MachO {

template<class T>
inline T safe_dyld_opcode_count(const SegmentCommand* segment,
                                uint64_t segment_offset, uint64_t pointer_size,
                                uint64_t count) {
  if (segment == nullptr || pointer_size == 0) {
    return count;
  }
  const uint64_t seg_size = segment->content().size();
  uint64_t remaining = 0;
  if (segment_offset < seg_size) {
    remaining = seg_size - segment_offset;
  }

  const uint64_t max_count = remaining / pointer_size + 1;

  if (count > max_count) {
    LIEF_WARN("dyld opcode repeat count ({}) is too large ({})", count, max_count);
    return static_cast<T>(max_count);
  }

  return static_cast<T>(count);
}

}
#endif
