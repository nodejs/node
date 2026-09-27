/* Copyright 2017 - 2026 R. Thomas
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
#pragma once
#include "LIEF/optional.hpp"

#include <spdlog/fmt/fmt.h>
#include <cassert>
#include <cstdint>

namespace LIEF {
class BinaryStream;
}

namespace LIEF::ELF {
struct elf_file_info_t {
  uintptr_t imagebase = -1llu;
  uint64_t end_address = 0;
  uintptr_t phdr_off = 0;
  uintptr_t phnum = 0;
  uintptr_t vsize() const {
    return end_address >= imagebase ? end_address - imagebase : 0;
  }

  std::string to_string() const {
    return fmt::format("imagebase={:#010x}, end_address={:#010x}, "
                       "phdr_off={:#06x} virtual_size={:#06x}",
                       imagebase, end_address, phdr_off, vsize());
  }
};

optional<elf_file_info_t> get_info(BinaryStream& strm);
}
