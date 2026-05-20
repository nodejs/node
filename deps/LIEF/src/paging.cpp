/* Copyright 2021 - 2025 R. Thomas
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
#include "LIEF/config.h"
#include "paging.hpp"
#include "Object.tcc"
#include "LIEF/utils.hpp"

#if defined(LIEF_ELF_SUPPORT)
#include "LIEF/ELF/Binary.hpp"
#endif

#if defined(LIEF_MACHO_SUPPORT)
#include "LIEF/MachO/Binary.hpp"
#endif

#if defined(LIEF_PE_SUPPORT)
#include "LIEF/PE/Binary.hpp"
#endif

namespace LIEF {

using LIEF::operator""_KB;

static constexpr auto DEFAULT_PAGESZ = 4_KB;

struct page_sizes_t {
  uint32_t common = 0;
  uint32_t max = 0;
};

#if defined(LIEF_ELF_SUPPORT)
page_sizes_t get_pagesize(const ELF::Binary& elf) {
  // These page sizes are coming from lld's ELF linker.
  //
  // Some architectures can have different page sizes for which
  // LLVM define a "Common" (`defaultCommonPageSize`) page size and a "Max"
  // page size (`defaultMaxPageSize`). In LIEF we return the Common page size
  // and users can still configure another page size with
  // LIEF::ELF::ParserConfig::page_size
  switch (elf.header().machine_type()) {
    default:
      return {4_KB, 4_KB};

    case ELF::ARCH::X86_64:
    case ELF::ARCH::I386:
    case ELF::ARCH::AMDGPU:
      return {4_KB, 4_KB};

    case ELF::ARCH::ARM:
    case ELF::ARCH::AARCH64:
      return {4_KB, 64_KB};

    case ELF::ARCH::LOONGARCH:
      return {16_KB, 64_KB};

    case ELF::ARCH::SPARCV9:
      return {8_KB, 1024_KB};

    case ELF::ARCH::HEXAGON:
      return {4_KB, 64_KB};

    case ELF::ARCH::MIPS:
    case ELF::ARCH::MIPS_RS3_LE:
      return {4_KB, 64_KB};

    case ELF::ARCH::PPC:
    case ELF::ARCH::PPC64:
      return {4_KB, 64_KB};
  }
  return {4_KB, 4_KB};
}
#endif

#if defined(LIEF_PE_SUPPORT)
uint32_t get_pagesize(const PE::Binary& pe) {
  // According to: https://devblogs.microsoft.com/oldnewthing/20210510-00/?p=105200
  switch (pe.header().machine()) {
    case PE::Header::MACHINE_TYPES::I386:
    case PE::Header::MACHINE_TYPES::AMD64:
    case PE::Header::MACHINE_TYPES::SH4:
    case PE::Header::MACHINE_TYPES::MIPS16:
    case PE::Header::MACHINE_TYPES::MIPSFPU:
    case PE::Header::MACHINE_TYPES::MIPSFPU16:
    case PE::Header::MACHINE_TYPES::POWERPC:
    case PE::Header::MACHINE_TYPES::THUMB:
    case PE::Header::MACHINE_TYPES::ARM:
    case PE::Header::MACHINE_TYPES::ARMNT:
    case PE::Header::MACHINE_TYPES::ARM64:
      return 4_KB;

    case PE::Header::MACHINE_TYPES::IA64:
      return 8_KB;

    default:
      return DEFAULT_PAGESZ;
  }
  return DEFAULT_PAGESZ;
}
#endif

#if defined (LIEF_MACHO_SUPPORT)
uint32_t get_pagesize(const MachO::Binary& macho) {
  switch (macho.header().cpu_type()) {
    case MachO::Header::CPU_TYPE::X86:
    case MachO::Header::CPU_TYPE::X86_64:
      return 4_KB;

    case MachO::Header::CPU_TYPE::ARM:
    case MachO::Header::CPU_TYPE::ARM64:
      return 16_KB;

    default:
      return DEFAULT_PAGESZ;
  }
  return DEFAULT_PAGESZ;
}
#endif

uint32_t get_pagesize(const Binary& bin) {

#if defined(LIEF_ELF_SUPPORT)
  if (ELF::Binary::classof(&bin)) {
    const auto& [common, max] = get_pagesize(*bin.as<ELF::Binary>());
    return common;
  }
#endif

#if defined(LIEF_PE_SUPPORT)
  if (PE::Binary::classof(&bin)) {
    return get_pagesize(*bin.as<PE::Binary>());
  }
#endif

#if defined (LIEF_MACHO_SUPPORT)
  if (MachO::Binary::classof(&bin)) {
    return get_pagesize(*bin.as<MachO::Binary>());
  }
#endif

  return DEFAULT_PAGESZ;
}
}
