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
#include <sstream>
#include <numeric>
#include <algorithm>

#include "frozen.hpp"

#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/Header.hpp"
#include "LIEF/ELF/EnumToString.hpp"
#include "ELF/Structures.hpp"

#include "logging.hpp"

namespace LIEF {
namespace ELF {

static constexpr uint64_t ARM_EABIMASK       = 0xFF000000U;
static constexpr uint64_t MIPS_ABI_MASK      = 0x0000f000U;
static constexpr uint64_t MIPS_MACH_MASK     = 0x00ff0000U;
static constexpr uint64_t MIPS_ARCH_ASE_MASK = 0x0f000000U;
static constexpr uint64_t MIPS_ARCH_MASK     = 0xf0000000U;
static constexpr uint64_t RISCV_FLOAT_ABI_MASK = 0x0006; // EF_RISCV_FLOAT_ABI

static constexpr auto PFLAGS_LIST = {
  PROCESSOR_FLAGS::ARM_EABI_UNKNOWN, PROCESSOR_FLAGS::ARM_SOFT_FLOAT,
  PROCESSOR_FLAGS::ARM_VFP_FLOAT, PROCESSOR_FLAGS::ARM_EABI_VER1,
  PROCESSOR_FLAGS::ARM_EABI_VER2, PROCESSOR_FLAGS::ARM_EABI_VER3,
  PROCESSOR_FLAGS::ARM_EABI_VER4, PROCESSOR_FLAGS::ARM_EABI_VER5,
  PROCESSOR_FLAGS::HEXAGON_MACH_V2, PROCESSOR_FLAGS::HEXAGON_MACH_V3,
  PROCESSOR_FLAGS::HEXAGON_MACH_V4, PROCESSOR_FLAGS::HEXAGON_MACH_V5,
  PROCESSOR_FLAGS::HEXAGON_ISA_V2, PROCESSOR_FLAGS::HEXAGON_ISA_V3,
  PROCESSOR_FLAGS::HEXAGON_ISA_V4, PROCESSOR_FLAGS::HEXAGON_ISA_V5,
  PROCESSOR_FLAGS::LOONGARCH_ABI_SOFT_FLOAT, PROCESSOR_FLAGS::LOONGARCH_ABI_SINGLE_FLOAT,
  PROCESSOR_FLAGS::LOONGARCH_ABI_DOUBLE_FLOAT, PROCESSOR_FLAGS::MIPS_NOREORDER,
  PROCESSOR_FLAGS::MIPS_PIC, PROCESSOR_FLAGS::MIPS_CPIC,
  PROCESSOR_FLAGS::MIPS_ABI2, PROCESSOR_FLAGS::MIPS_32BITMODE,
  PROCESSOR_FLAGS::MIPS_FP64, PROCESSOR_FLAGS::MIPS_NAN2008,
  PROCESSOR_FLAGS::MIPS_ABI_O32, PROCESSOR_FLAGS::MIPS_ABI_O64,
  PROCESSOR_FLAGS::MIPS_ABI_EABI32, PROCESSOR_FLAGS::MIPS_ABI_EABI64,
  PROCESSOR_FLAGS::MIPS_MACH_3900,
  PROCESSOR_FLAGS::MIPS_MACH_4010, PROCESSOR_FLAGS::MIPS_MACH_4100,
  PROCESSOR_FLAGS::MIPS_MACH_4650, PROCESSOR_FLAGS::MIPS_MACH_4120,
  PROCESSOR_FLAGS::MIPS_MACH_4111, PROCESSOR_FLAGS::MIPS_MACH_SB1,
  PROCESSOR_FLAGS::MIPS_MACH_OCTEON, PROCESSOR_FLAGS::MIPS_MACH_XLR,
  PROCESSOR_FLAGS::MIPS_MACH_OCTEON2, PROCESSOR_FLAGS::MIPS_MACH_OCTEON3,
  PROCESSOR_FLAGS::MIPS_MACH_5400, PROCESSOR_FLAGS::MIPS_MACH_5900,
  PROCESSOR_FLAGS::MIPS_MACH_5500, PROCESSOR_FLAGS::MIPS_MACH_9000,
  PROCESSOR_FLAGS::MIPS_MACH_LS2E, PROCESSOR_FLAGS::MIPS_MACH_LS2F,
  PROCESSOR_FLAGS::MIPS_MACH_LS3A, PROCESSOR_FLAGS::MIPS_MICROMIPS,
  PROCESSOR_FLAGS::MIPS_ARCH_ASE_M16, PROCESSOR_FLAGS::MIPS_ARCH_ASE_MDMX,
  PROCESSOR_FLAGS::MIPS_ARCH_1, PROCESSOR_FLAGS::MIPS_ARCH_2,
  PROCESSOR_FLAGS::MIPS_ARCH_3, PROCESSOR_FLAGS::MIPS_ARCH_4,
  PROCESSOR_FLAGS::MIPS_ARCH_5, PROCESSOR_FLAGS::MIPS_ARCH_32,
  PROCESSOR_FLAGS::MIPS_ARCH_64, PROCESSOR_FLAGS::MIPS_ARCH_32R2,
  PROCESSOR_FLAGS::MIPS_ARCH_64R2, PROCESSOR_FLAGS::MIPS_ARCH_32R6,
  PROCESSOR_FLAGS::MIPS_ARCH_64R6,
  PROCESSOR_FLAGS::RISCV_RVC,
  PROCESSOR_FLAGS::RISCV_FLOAT_ABI_SOFT,
  PROCESSOR_FLAGS::RISCV_FLOAT_ABI_SINGLE,
  PROCESSOR_FLAGS::RISCV_FLOAT_ABI_DOUBLE,
  PROCESSOR_FLAGS::RISCV_FLOAT_ABI_QUAD,
  PROCESSOR_FLAGS::RISCV_FLOAT_ABI_RVE,
  PROCESSOR_FLAGS::RISCV_FLOAT_ABI_TSO,

};

template<class T>
Header::Header(const T& header):
  file_type_(FILE_TYPE(header.e_type)),
  machine_type_(ARCH(header.e_machine)),
  object_file_version_(VERSION(header.e_version)),
  entrypoint_(header.e_entry),
  program_headers_offset_(header.e_phoff),
  section_headers_offset_(header.e_shoff),
  processor_flags_(header.e_flags),
  header_size_(header.e_ehsize),
  program_header_size_(header.e_phentsize),
  numberof_segments_(header.e_phnum),
  section_header_size_(header.e_shentsize),
  numberof_sections_(header.e_shnum),
  section_string_table_idx_(header.e_shstrndx)
{
  std::copy(std::begin(header.e_ident), std::end(header.e_ident),
           std::begin(identity_));
}

template Header::Header(const details::Elf32_Ehdr& header);
template Header::Header(const details::Elf64_Ehdr& header);

void Header::identity(const std::string& identity) {
  std::copy(std::begin(identity), std::end(identity),
            std::begin(identity_));
}

void Header::identity(const Header::identity_t& identity) {
  std::copy(std::begin(identity), std::end(identity),
            std::begin(identity_));
}

void Header::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

bool Header::has(PROCESSOR_FLAGS flag) const {
  const auto ID = uint64_t(flag) >> PFLAGS_BIT;
  const ARCH arch = machine_type();
  uint32_t raw_flag = uint64_t(flag) & PFLAGS_MASK;

  if (ID == PF_ARM_ID) {
    if (arch != ARCH::ARM) {
      return false;
    }

    switch (flag) {
      case PROCESSOR_FLAGS::ARM_EABI_VER1:
      case PROCESSOR_FLAGS::ARM_EABI_VER2:
      case PROCESSOR_FLAGS::ARM_EABI_VER3:
      case PROCESSOR_FLAGS::ARM_EABI_VER4:
      case PROCESSOR_FLAGS::ARM_EABI_VER5:
        return (processor_flag() & ARM_EABIMASK) == raw_flag;
      default:
        return (processor_flag() & raw_flag) != 0;
    }

    return false;
  }


  if (ID == PF_RISCV_ID) {
    if (arch != ARCH::RISCV) {
      return false;
    }

    switch (flag) {
      case PROCESSOR_FLAGS::RISCV_FLOAT_ABI_SOFT:
      case PROCESSOR_FLAGS::RISCV_FLOAT_ABI_SINGLE:
      case PROCESSOR_FLAGS::RISCV_FLOAT_ABI_DOUBLE:
      case PROCESSOR_FLAGS::RISCV_FLOAT_ABI_QUAD:
      case PROCESSOR_FLAGS::ARM_EABI_VER5:
        return (processor_flag() & RISCV_FLOAT_ABI_MASK) == raw_flag;
      default:
        return (processor_flag() & raw_flag) != 0;
    }

  }

  if (ID == PF_HEX_ID) {
    if (arch != ARCH::HEXAGON) {
      return false;
    }
    return (processor_flag() & raw_flag) != 0;
  }

  if (ID == PF_LOONGARCH_ID) {
    if (arch != ARCH::LOONGARCH) {
      return false;
    }
    return (processor_flag() & raw_flag) != 0;
  }

  if (ID == PF_MIPS_ID) {
    if (arch != ARCH::MIPS && arch != ARCH::MIPS_RS3_LE && arch != ARCH::MIPS_X) {
      return false;
    }

    switch(flag) {
      case PROCESSOR_FLAGS::MIPS_NOREORDER:
      case PROCESSOR_FLAGS::MIPS_PIC:
      case PROCESSOR_FLAGS::MIPS_CPIC:
      case PROCESSOR_FLAGS::MIPS_ABI2:
      case PROCESSOR_FLAGS::MIPS_32BITMODE:
      case PROCESSOR_FLAGS::MIPS_FP64:
      case PROCESSOR_FLAGS::MIPS_NAN2008:
          return (processor_flag() & raw_flag) != 0;

      case PROCESSOR_FLAGS::MIPS_ABI_O32:
      case PROCESSOR_FLAGS::MIPS_ABI_O64:
      case PROCESSOR_FLAGS::MIPS_ABI_EABI32:
      case PROCESSOR_FLAGS::MIPS_ABI_EABI64:
        return (processor_flag() & MIPS_ABI_MASK) == (raw_flag & MIPS_ABI_MASK);

      case PROCESSOR_FLAGS::MIPS_MACH_3900:
      case PROCESSOR_FLAGS::MIPS_MACH_4010:
      case PROCESSOR_FLAGS::MIPS_MACH_4100:
      case PROCESSOR_FLAGS::MIPS_MACH_4650:
      case PROCESSOR_FLAGS::MIPS_MACH_4120:
      case PROCESSOR_FLAGS::MIPS_MACH_4111:
      case PROCESSOR_FLAGS::MIPS_MACH_SB1:
      case PROCESSOR_FLAGS::MIPS_MACH_OCTEON:
      case PROCESSOR_FLAGS::MIPS_MACH_XLR:
      case PROCESSOR_FLAGS::MIPS_MACH_OCTEON2:
      case PROCESSOR_FLAGS::MIPS_MACH_OCTEON3:
      case PROCESSOR_FLAGS::MIPS_MACH_5400:
      case PROCESSOR_FLAGS::MIPS_MACH_5900:
      case PROCESSOR_FLAGS::MIPS_MACH_5500:
      case PROCESSOR_FLAGS::MIPS_MACH_9000:
      case PROCESSOR_FLAGS::MIPS_MACH_LS2E:
      case PROCESSOR_FLAGS::MIPS_MACH_LS2F:
      case PROCESSOR_FLAGS::MIPS_MACH_LS3A:
          return (processor_flag() & MIPS_MACH_MASK) == (raw_flag & MIPS_MACH_MASK);

      case PROCESSOR_FLAGS::MIPS_MICROMIPS:
      case PROCESSOR_FLAGS::MIPS_ARCH_ASE_M16:
      case PROCESSOR_FLAGS::MIPS_ARCH_ASE_MDMX:
        return (processor_flag() & MIPS_ARCH_ASE_MASK) == (raw_flag & MIPS_ARCH_ASE_MASK);

      case PROCESSOR_FLAGS::MIPS_ARCH_1:
      case PROCESSOR_FLAGS::MIPS_ARCH_2:
      case PROCESSOR_FLAGS::MIPS_ARCH_3:
      case PROCESSOR_FLAGS::MIPS_ARCH_4:
      case PROCESSOR_FLAGS::MIPS_ARCH_5:
      case PROCESSOR_FLAGS::MIPS_ARCH_32:
      case PROCESSOR_FLAGS::MIPS_ARCH_64:
      case PROCESSOR_FLAGS::MIPS_ARCH_32R2:
      case PROCESSOR_FLAGS::MIPS_ARCH_64R2:
      case PROCESSOR_FLAGS::MIPS_ARCH_32R6:
      case PROCESSOR_FLAGS::MIPS_ARCH_64R6:
          return (processor_flag() & MIPS_ARCH_MASK) == raw_flag;

      default:
          return (processor_flag() & raw_flag) != 0;
    }
  }

  if (ID != 0) {
    LIEF_WARN("Architecture {} is not yet supported for this flag",
              to_string(arch));
    return false;
  }

  return false;
}

std::vector<PROCESSOR_FLAGS> Header::flags_list() const {
  std::vector<PROCESSOR_FLAGS> flags;

  std::copy_if(std::begin(PFLAGS_LIST), std::end(PFLAGS_LIST),
               std::back_inserter(flags),
    [this] (PROCESSOR_FLAGS f) { return this->has(f); }
  );

  return flags;
}

std::ostream& operator<<(std::ostream& os, const Header& hdr) {
  static constexpr auto PADDING = 35;
  const std::vector<PROCESSOR_FLAGS> flags = hdr.flags_list();
  std::vector<std::string> flags_str;
  flags_str.reserve(flags.size());

  std::transform(flags.begin(), flags.end(), std::back_inserter(flags_str),
    [] (PROCESSOR_FLAGS f) { return to_string(f); }
  );

  os << "ELF Header:\n"
     << fmt::format("{:<{}} {:02x}\n", "Magic:", PADDING, fmt::join(hdr.identity(), " "))
     << fmt::format("{:<{}} {}\n", "Class:", PADDING, to_string(hdr.identity_class()))
     << fmt::format("{:<{}} {}\n", "Data:", PADDING, to_string(hdr.identity_data()))
     << fmt::format("{:<{}} {}\n", "Version:", PADDING, to_string(hdr.identity_version()))
     << fmt::format("{:<{}} {}\n", "OS/ABI:", PADDING, to_string(hdr.identity_os_abi()))
     << fmt::format("{:<{}} {}\n", "ABI Version:", PADDING, hdr.identity_abi_version())
     << fmt::format("{:<{}} {}\n", "Type:", PADDING, to_string(hdr.file_type()))
     << fmt::format("{:<{}} {}\n", "Machine:", PADDING, to_string(hdr.machine_type()))
     << fmt::format("{:<{}} {}\n", "Version:", PADDING, to_string(hdr.object_file_version()))
     << fmt::format("{:<{}} 0x{:08x}\n", "Entry point address:", PADDING, hdr.entrypoint())
     << fmt::format("{:<{}} {} (bytes into file)\n", "Start of program headers:", PADDING, hdr.program_headers_offset())
     << fmt::format("{:<{}} {} (bytes into file)\n", "Start of section headers:", PADDING, hdr.section_headers_offset())
     << fmt::format("{:<{}} 0x{:x} {}\n", "Flags:", PADDING, hdr.processor_flag(), fmt::to_string(fmt::join(flags_str, ", ")))
     << fmt::format("{:<{}} {} (bytes)\n", "Size of this header:", PADDING, hdr.header_size())
     << fmt::format("{:<{}} {} (bytes)\n", "Size of program headers:", PADDING, hdr.program_header_size())
     << fmt::format("{:<{}} {}\n", "Number of program headers:", PADDING, hdr.numberof_segments())
     << fmt::format("{:<{}} {} (bytes)\n", "Size of section headers:", PADDING, hdr.section_header_size())
     << fmt::format("{:<{}} {}\n", "Number of section headers:", PADDING, hdr.numberof_sections())
     << fmt::format("{:<{}} {}\n", "Section header string table index:", PADDING, hdr.section_name_table_idx())
  ;

  return os;
}

const char* to_string(Header::FILE_TYPE e) {
  switch (e) {
    case Header::FILE_TYPE::NONE: return "NONE";
    case Header::FILE_TYPE::REL: return "REL";
    case Header::FILE_TYPE::EXEC: return "EXEC";
    case Header::FILE_TYPE::DYN: return "DYN";
    case Header::FILE_TYPE::CORE: return "CORE";
  }
  return "UNKNOWN";
}

const char* to_string(Header::VERSION e) {
  switch (e) {
    case Header::VERSION::NONE: return "NONE";
    case Header::VERSION::CURRENT: return "CURRENT";
  }
  return "UNKNOWN";
}

const char* to_string(Header::CLASS e) {
  switch (e) {
    case Header::CLASS::NONE: return "NONE";
    case Header::CLASS::ELF32: return "ELF32";
    case Header::CLASS::ELF64: return "ELF64";
  }
  return "UNKNOWN";
}

const char* to_string(Header::ELF_DATA e) {
  switch (e) {
    case Header::ELF_DATA::NONE: return "NONE";
    case Header::ELF_DATA::LSB: return "LSB";
    case Header::ELF_DATA::MSB: return "MSB";
  }
  return "UNKNOWN";
}

const char* to_string(Header::OS_ABI e) {
  #define ENTRY(X) std::pair(Header::OS_ABI::X, #X)
  STRING_MAP enums2str {
    ENTRY(SYSTEMV),
    ENTRY(HPUX),
    ENTRY(NETBSD),
    ENTRY(GNU),
    ENTRY(LINUX),
    ENTRY(HURD),
    ENTRY(SOLARIS),
    ENTRY(AIX),
    ENTRY(IRIX),
    ENTRY(FREEBSD),
    ENTRY(TRU64),
    ENTRY(MODESTO),
    ENTRY(OPENBSD),
    ENTRY(OPENVMS),
    ENTRY(NSK),
    ENTRY(AROS),
    ENTRY(FENIXOS),
    ENTRY(CLOUDABI),
    ENTRY(C6000_ELFABI),
    ENTRY(AMDGPU_HSA),
    ENTRY(C6000_LINUX),
    ENTRY(ARM),
    ENTRY(STANDALONE),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}


}
}
