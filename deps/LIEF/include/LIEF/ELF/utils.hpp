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
#ifndef LIEF_ELF_UTILS_H
#define LIEF_ELF_UTILS_H

#include <string>
#include <vector>

#include "LIEF/types.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class BinaryStream;

namespace ELF {

/// Check if given stream wraps an ELF file
LIEF_API bool is_elf(BinaryStream& stream);

/// Check if the given file is an ELF one.
LIEF_API bool is_elf(const std::string& file);

/// check if the raw data is an ELF file
LIEF_API bool is_elf(const std::vector<uint8_t>& raw);

LIEF_API unsigned long hash32(const char* name);
LIEF_API unsigned long hash64(const char* name);
LIEF_API uint32_t dl_new_hash(const char* name);
}
}


#endif
