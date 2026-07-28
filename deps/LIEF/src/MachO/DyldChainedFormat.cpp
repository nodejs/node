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
#ifndef LIEF_MACHO_DYLD_CHAINED_FORMAT_H
#define LIEF_MACHO_DYLD_CHAINED_FORMAT_H
#include "LIEF/MachO/DyldChainedFormat.hpp"
#include "frozen.hpp"
namespace LIEF {
namespace MachO {

const char* to_string(DYLD_CHAINED_FORMAT e) {
  #define ENTRY(X) std::pair(DYLD_CHAINED_FORMAT::X, #X)
  STRING_MAP enums2str {
    ENTRY(IMPORT),
    ENTRY(IMPORT_ADDEND),
    ENTRY(IMPORT_ADDEND64),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
const char* to_string(DYLD_CHAINED_PTR_FORMAT e) {
  #define ENTRY(X) std::pair(DYLD_CHAINED_PTR_FORMAT::X, #X)
  STRING_MAP enums2str {
    ENTRY(NONE),
    ENTRY(PTR_ARM64E),
    ENTRY(PTR_64),
    ENTRY(PTR_32),
    ENTRY(PTR_32_CACHE),
    ENTRY(PTR_32_FIRMWARE),
    ENTRY(PTR_64_OFFSET),
    ENTRY(PTR_ARM64E_KERNEL),
    ENTRY(PTR_64_KERNEL_CACHE),
    ENTRY(PTR_ARM64E_USERLAND),
    ENTRY(PTR_ARM64E_FIRMWARE),
    ENTRY(PTR_X86_64_KERNEL_CACHE),
    ENTRY(PTR_ARM64E_USERLAND24),
    ENTRY(PTR_ARM64E_SHARED_CACHE),
    ENTRY(PTR_ARM64E_SEGMENTED),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
#endif
