/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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
#include "LIEF/endianness_support.hpp"

#include "intmem.h"

#define TMPL_DECL(T)              \
  template<> void                 \
  swap_endian<T>(T* u) {          \
    *u = swap_integer_endian(*u); \
  }

namespace LIEF {

template<typename T>
T swap_integer_endian(T u) {
  return intmem::bswap(u);
}

template<>
char16_t swap_integer_endian<char16_t>(char16_t u) {
  return intmem::bswap((uint16_t)u);
}

template<>
char swap_integer_endian<char>(char u) {
  return intmem::bswap((uint8_t)u);
}

TMPL_DECL(char)
TMPL_DECL(char16_t)

TMPL_DECL(uint8_t)
TMPL_DECL(uint16_t)
TMPL_DECL(uint32_t)
TMPL_DECL(uint64_t)

TMPL_DECL(int8_t)
TMPL_DECL(int16_t)
TMPL_DECL(int32_t)
TMPL_DECL(int64_t)

}
