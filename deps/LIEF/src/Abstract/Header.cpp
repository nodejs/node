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
#include "LIEF/Visitor.hpp"
#include "LIEF/Abstract/Header.hpp"

#include "LIEF/ELF/Binary.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/PE/Binary.hpp"

#include <spdlog/fmt/fmt.h>

#include "frozen.hpp"
#include "fmt_formatter.hpp"
#include "logging.hpp"
#include "LIEF/config.h"

FMT_FORMATTER(LIEF::Header::MODES, LIEF::to_string);

namespace LIEF {

static constexpr auto ARRAY_MODES = {
  Header::MODES::NONE, Header::MODES::ARM64E,
  Header::MODES::BITS_16, Header::MODES::BITS_32,
  Header::MODES::BITS_64, Header::MODES::THUMB,
};

Header Header::from(const ELF::Binary& elf) {
  if constexpr (!lief_elf_support) {
    return {};
  }

  Header hdr;
  hdr.entrypoint_ = elf.entrypoint();

  switch (elf.header().identity_class()) {
    case ELF::Header::CLASS::ELF32:
      hdr.modes_ |= MODES::BITS_32;
      break;

    case ELF::Header::CLASS::ELF64:
      hdr.modes_ |= MODES::BITS_64;
      break;

    default:
      break;
  }

  switch (elf.header().machine_type()) {
    case ELF::ARCH::X86_64:
      hdr.architecture_ = ARCHITECTURES::X86_64;
      break;

    case ELF::ARCH::I386:
      hdr.architecture_ = ARCHITECTURES::X86;
      break;

    case ELF::ARCH::ARM:
      hdr.architecture_ = ARCHITECTURES::ARM;
      break;

    case ELF::ARCH::AARCH64:
      hdr.architecture_ = ARCHITECTURES::ARM64;
      break;

    case ELF::ARCH::MIPS:
      hdr.architecture_ = ARCHITECTURES::MIPS;
      break;

    case ELF::ARCH::PPC:
      hdr.architecture_ = ARCHITECTURES::PPC;
      break;

    case ELF::ARCH::PPC64:
      hdr.architecture_ = ARCHITECTURES::PPC64;
      break;

    case ELF::ARCH::RISCV:
      hdr.architecture_ = ARCHITECTURES::RISCV;
      break;

    case ELF::ARCH::LOONGARCH:
      hdr.architecture_ = ARCHITECTURES::LOONGARCH;
      break;

    default:
      break;
  }

  switch (elf.header().identity_data()) {
    case ELF::Header::ELF_DATA::LSB:
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;
    case ELF::Header::ELF_DATA::MSB:
      hdr.endianness_ = ENDIANNESS::BIG;
      break;
    case ELF::Header::ELF_DATA::NONE:
      hdr.endianness_ = ENDIANNESS::UNKNOWN;
      break;
  }

  switch (elf.header().file_type()) {
    case ELF::Header::FILE_TYPE::EXEC:
      hdr.object_type_ = OBJECT_TYPES::EXECUTABLE;
      break;

    case ELF::Header::FILE_TYPE::DYN:
      hdr.object_type_ = elf.has_interpreter() ? OBJECT_TYPES::EXECUTABLE :
                                                 OBJECT_TYPES::LIBRARY;
      break;

    case ELF::Header::FILE_TYPE::REL:
      hdr.object_type_ = OBJECT_TYPES::OBJECT;
      break;

    default:
      break;
  }

  return hdr;
}

Header Header::from(const PE::Binary& pe) {
  if constexpr (!lief_pe_support) {
    return {};
  }

  Header hdr;
  hdr.entrypoint_ = pe.entrypoint();
  const PE::Header& pe_header = pe.header();

  if (pe.type() == LIEF::PE::PE_TYPE::PE32) {
    hdr.modes_ |= MODES::BITS_32;
  }

  if (pe.type() == LIEF::PE::PE_TYPE::PE32_PLUS) {
    hdr.modes_ |= MODES::BITS_64;
  }

  switch (pe_header.machine()) {
    case PE::Header::MACHINE_TYPES::AMD64:
      hdr.architecture_ = ARCHITECTURES::X86_64;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case PE::Header::MACHINE_TYPES::ARMNT:
    case PE::Header::MACHINE_TYPES::ARM:
      hdr.architecture_ = ARCHITECTURES::ARM;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case PE::Header::MACHINE_TYPES::ARM64:
      hdr.architecture_ = ARCHITECTURES::ARM64;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case PE::Header::MACHINE_TYPES::I386:
      hdr.architecture_ = ARCHITECTURES::X86;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case PE::Header::MACHINE_TYPES::RISCV32:
    case PE::Header::MACHINE_TYPES::RISCV64:
      hdr.architecture_ = ARCHITECTURES::RISCV;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case PE::Header::MACHINE_TYPES::THUMB:
      hdr.architecture_ = ARCHITECTURES::ARM;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      hdr.modes_ |= MODES::THUMB;
      break;

    default:
      break;
  }

  if (pe_header.has_characteristic(PE::Header::CHARACTERISTICS::DLL)) {
    hdr.object_type_ = OBJECT_TYPES::LIBRARY;
  }
  else if (pe_header.has_characteristic(PE::Header::CHARACTERISTICS::EXECUTABLE_IMAGE)) {
    hdr.object_type_ = OBJECT_TYPES::EXECUTABLE;
  }
  return hdr;
}

Header Header::from(const MachO::Binary& macho) {
  if constexpr (!lief_macho_support) {
    return {};
  }

  Header hdr;
  {
    logging::Scoped scope(logging::LEVEL::OFF);
    // Disable the warning message
    hdr.entrypoint_ = macho.entrypoint();
  }
  const MachO::Header& macho_hdr = macho.header();
  const MachO::MACHO_TYPES magic = macho_hdr.magic();
  if (magic == MachO::MACHO_TYPES::MAGIC_64 ||
      magic == MachO::MACHO_TYPES::CIGAM_64)
  {
    hdr.modes_ |= MODES::BITS_64;
  }

  if (magic == MachO::MACHO_TYPES::MAGIC ||
      magic == MachO::MACHO_TYPES::CIGAM)
  {
    hdr.modes_ |= MODES::BITS_32;
  }

  switch (macho_hdr.cpu_type()) {
    case MachO::Header::CPU_TYPE::ARM:
      hdr.architecture_ = ARCHITECTURES::ARM;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case MachO::Header::CPU_TYPE::ARM64:
      {
        hdr.architecture_ = ARCHITECTURES::ARM64;
        hdr.endianness_ = ENDIANNESS::LITTLE;
        if (macho.support_arm64_ptr_auth()) {
          hdr.modes_ |= MODES::ARM64E;
        }
        break;
      }

    case MachO::Header::CPU_TYPE::X86:
      hdr.architecture_ = ARCHITECTURES::X86;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case MachO::Header::CPU_TYPE::X86_64:
      hdr.architecture_ = ARCHITECTURES::X86_64;
      hdr.endianness_ = ENDIANNESS::LITTLE;
      break;

    case MachO::Header::CPU_TYPE::MIPS:
      hdr.architecture_ = ARCHITECTURES::MIPS;
      break;

    case MachO::Header::CPU_TYPE::POWERPC64:
      hdr.architecture_ = ARCHITECTURES::PPC64;
      hdr.endianness_ = ENDIANNESS::BIG;
      break;

    case MachO::Header::CPU_TYPE::POWERPC:
      hdr.architecture_ = ARCHITECTURES::PPC;
      hdr.endianness_ = ENDIANNESS::BIG;
      break;

    case MachO::Header::CPU_TYPE::SPARC:
      hdr.architecture_ = ARCHITECTURES::SPARC;
      hdr.endianness_ = ENDIANNESS::BIG;
      break;

    default:
      break;
  }


  switch (macho_hdr.file_type()) {
    case MachO::Header::FILE_TYPE::EXECUTE:
      hdr.object_type_ = OBJECT_TYPES::EXECUTABLE;
      break;

    case MachO::Header::FILE_TYPE::DYLIB:
      hdr.object_type_ = OBJECT_TYPES::LIBRARY;
      break;

    case MachO::Header::FILE_TYPE::OBJECT:
      hdr.object_type_ = OBJECT_TYPES::OBJECT;
      break;

    default:
      break;
  }

  return hdr;
}

std::vector<Header::MODES> Header::modes_list() const {
  std::vector<MODES> flags;
  std::copy_if(std::begin(ARRAY_MODES), std::end(ARRAY_MODES),
               std::back_inserter(flags),
               [this] (MODES f) { return is(f); });
  return flags;
}

void Header::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Header& hdr) {
  const auto& modes = hdr.modes_list();
  os << fmt::format("[{}] {} (endianness={}) {}",
                    to_string(hdr.object_type()), to_string(hdr.architecture()),
                    to_string(hdr.endianness()), fmt::to_string(modes));
  return os;
}

const char* to_string(Header::ARCHITECTURES e) {
  #define ENTRY(X) std::pair(Header::ARCHITECTURES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(ARM),
    ENTRY(ARM64),
    ENTRY(MIPS),
    ENTRY(X86),
    ENTRY(X86_64),
    ENTRY(PPC),
    ENTRY(SPARC),
    ENTRY(SYSZ),
    ENTRY(XCORE),
    ENTRY(RISCV),
    ENTRY(LOONGARCH),
    ENTRY(PPC64),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
const char* to_string(Header::OBJECT_TYPES e) {
  #define ENTRY(X) std::pair(Header::OBJECT_TYPES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(EXECUTABLE),
    ENTRY(LIBRARY),
    ENTRY(OBJECT),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
const char* to_string(Header::MODES e) {
  #define ENTRY(X) std::pair(Header::MODES::X, #X)
  STRING_MAP enums2str {
    ENTRY(NONE),
    ENTRY(BITS_16),
    ENTRY(BITS_32),
    ENTRY(BITS_64),
    ENTRY(THUMB),
    ENTRY(ARM64E),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
const char* to_string(Header::ENDIANNESS e) {
  #define ENTRY(X) std::pair(Header::ENDIANNESS::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(BIG),
    ENTRY(LITTLE),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}


}
