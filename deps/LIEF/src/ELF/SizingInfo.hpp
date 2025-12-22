
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
#ifndef LIEF_ELF_SIZING_INFO_H
#define LIEF_ELF_SIZING_INFO_H
#include "LIEF/visibility.h"

#include <cstdint>

namespace LIEF {
namespace ELF {
struct sizing_info_t {
  uint64_t dynsym = 0;
  uint64_t dynstr = 0;
  uint64_t dynamic = 0;
  uint64_t interpreter = 0;
  uint64_t gnu_hash = 0;
  uint64_t hash = 0;
  uint64_t rela = 0;
  uint64_t relr = 0;
  uint64_t android_rela = 0;
  uint64_t jmprel = 0;
  uint64_t versym = 0;
  uint64_t verdef = 0;
  uint64_t verneed = 0;

  uint64_t init_array = 0;
  uint64_t fini_array = 0;
  uint64_t preinit_array = 0;
};
}
}
#endif
