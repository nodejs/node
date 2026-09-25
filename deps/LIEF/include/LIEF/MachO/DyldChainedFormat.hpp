/* Copyright 2024 - 2025 R. Thomas
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
#ifndef LIEF_MACHO_DYLD_CHAINED_FMT_H
#define LIEF_MACHO_DYLD_CHAINED_FMT_H
#include "LIEF/visibility.h"
namespace LIEF {
namespace MachO {

// values for dyld_chained_fixups_header.imports_format
enum class DYLD_CHAINED_FORMAT {
  IMPORT          = 1, // Originally: DYLD_CHAINED_IMPORT
  IMPORT_ADDEND   = 2, // Originally: DYLD_CHAINED_IMPORT_ADDEND
  IMPORT_ADDEND64 = 3, // Originally: DYLD_CHAINED_IMPORT_ADDEND64
};

// values for dyld_chained_starts_in_segment.pointer_format
enum class DYLD_CHAINED_PTR_FORMAT {
  NONE                       =  0,
  PTR_ARM64E                 =  1, // stride 8, unauth target is vmaddr
  PTR_64                     =  2, // target is vmaddr
  PTR_32                     =  3,
  PTR_32_CACHE               =  4,
  PTR_32_FIRMWARE            =  5,
  PTR_64_OFFSET              =  6, // target is vm offset
  PTR_ARM64E_OFFSET          =  7, // old name
  PTR_ARM64E_KERNEL          =  7, // stride 4, unauth target is vm offset
  PTR_64_KERNEL_CACHE        =  8,
  PTR_ARM64E_USERLAND        =  9, // stride 8, unauth target is vm offset
  PTR_ARM64E_FIRMWARE        = 10, // stride 4, unauth target is vmaddr
  PTR_X86_64_KERNEL_CACHE    = 11, // stride 1, x86_64 kernel caches
  PTR_ARM64E_USERLAND24      = 12, // stride 8, unauth target is vm offset, 24-bit bind
  PTR_ARM64E_SHARED_CACHE    = 13, // stride 8, regular/auth targets both vm offsets. Only A keys supported
  PTR_ARM64E_SEGMENTED       = 14, // stride 4, rebase offsets use segIndex and segOffset
};

LIEF_API const char* to_string(DYLD_CHAINED_FORMAT fmt);
LIEF_API const char* to_string(DYLD_CHAINED_PTR_FORMAT ptr_fmt);

}
}
#endif
