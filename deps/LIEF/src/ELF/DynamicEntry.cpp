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

#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/EnumToString.hpp"
#include "LIEF/ELF/DynamicEntryLibrary.hpp"
#include "LIEF/ELF/DynamicEntryArray.hpp"
#include "LIEF/ELF/DynamicEntryFlags.hpp"
#include "LIEF/ELF/DynamicEntryRpath.hpp"
#include "LIEF/ELF/DynamicEntryRunPath.hpp"
#include "LIEF/ELF/DynamicSharedObject.hpp"

#include <spdlog/fmt/fmt.h>

#include "frozen.hpp"
#include "logging.hpp"
#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

DynamicEntry::TAG DynamicEntry::from_value(uint64_t value, ARCH arch) {
  static constexpr auto LOPROC = 0x70000000;
  static constexpr auto HIPROC = 0x7FFFFFFF;

  static constexpr auto LOOS = 0x60000000;
  static constexpr auto HIOS = 0x6FFFFFFF;

  const bool is_os_spec = (LOOS <= value && value <= HIOS);
  const bool is_proc_spec = (LOPROC <= value && value <= HIPROC);

  if (is_os_spec && arch == ARCH::IA_64) {
    return TAG(IA_64_DISC + value);
  }

  if (is_proc_spec) {
    switch (arch) {
      case ARCH::AARCH64:
        return TAG(AARCH64_DISC + value);

      case ARCH::MIPS_RS3_LE:
      case ARCH::MIPS:
        return TAG(MIPS_DISC + value);

      case ARCH::HEXAGON:
        return TAG(HEXAGON_DISC + value);

      case ARCH::PPC:
        return TAG(PPC_DISC + value);

      case ARCH::PPC64:
        return TAG(PPC64_DISC + value);

      case ARCH::RISCV:
        return TAG(RISCV_DISC + value);

      case ARCH::X86_64:
        return TAG(X86_64_DISC + value);

      case ARCH::IA_64:
        return TAG(IA_64_DISC + value);

      default:
        LIEF_WARN("Dynamic tag: 0x{:04x} is not supported for the "
                  "current architecture ({})", value, ELF::to_string(arch));
        return TAG::UNKNOWN;
    }
  }

  return TAG(value);
}

uint64_t DynamicEntry::to_value(DynamicEntry::TAG tag) {
  auto raw_value = static_cast<uint64_t>(tag);
  if (MIPS_DISC <= raw_value && raw_value < AARCH64_DISC) {
    return raw_value - MIPS_DISC;
  }

  if (AARCH64_DISC <= raw_value && raw_value < HEXAGON_DISC) {
    return raw_value - AARCH64_DISC;
  }

  if (HEXAGON_DISC <= raw_value && raw_value < PPC_DISC) {
    return raw_value - HEXAGON_DISC;
  }

  if (PPC_DISC <= raw_value && raw_value < PPC64_DISC) {
    return raw_value - PPC_DISC;
  }

  if (PPC64_DISC <= raw_value && raw_value < RISCV_DISC) {
    return raw_value - PPC64_DISC;
  }

  if (RISCV_DISC <= raw_value && raw_value < X86_64_DISC) {
    return raw_value - RISCV_DISC;
  }

  if (X86_64_DISC <= raw_value) {
    return raw_value - X86_64_DISC;
  }

  if (IA_64_DISC <= raw_value) {
    return raw_value - IA_64_DISC;
  }

  return raw_value;
}


std::unique_ptr<DynamicEntry> DynamicEntry::create(TAG tag, uint64_t value) {
  switch (tag) {
    default:
      return std::make_unique<DynamicEntry>(tag, value);
    case TAG::NEEDED:
      return std::make_unique<DynamicEntryLibrary>();

    case TAG::SONAME:
      return std::make_unique<DynamicSharedObject>();

    case TAG::RUNPATH:
      return std::make_unique<DynamicEntryRunPath>();

    case TAG::RPATH:
      return std::make_unique<DynamicEntryRpath>();

    case TAG::FLAGS_1:
      return DynamicEntryFlags::create_dt_flag_1(value).clone();

    case TAG::FLAGS:
      return DynamicEntryFlags::create_dt_flag(value).clone();

    case TAG::INIT_ARRAY:
    case TAG::FINI_ARRAY:
    case TAG::PREINIT_ARRAY:
      return std::make_unique<DynamicEntryArray>(tag, DynamicEntryArray::array_t());
  }
  return std::make_unique<DynamicEntry>(tag, value);
}

DynamicEntry::DynamicEntry(const details::Elf64_Dyn& header, ARCH arch) :
  tag_{DynamicEntry::from_value(header.d_tag, arch)},
  value_{header.d_un.d_val}
{}

DynamicEntry::DynamicEntry(const details::Elf32_Dyn& header, ARCH arch) :
  tag_{DynamicEntry::from_value(header.d_tag, arch)},
  value_{header.d_un.d_val}
{}

void DynamicEntry::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DynamicEntry::print(std::ostream& os) const {
  os << fmt::format("{:<20}: 0x{:06x} ", ELF::to_string(tag()), value());
  return os;
}

std::string DynamicEntry::to_string() const {
  std::ostringstream oss;
  print(oss);
  return oss.str();
}

const char* to_string(DynamicEntry::TAG tag) {
  #define ENTRY(X) std::pair(DynamicEntry::TAG::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(DT_NULL_),
    ENTRY(NEEDED),
    ENTRY(PLTRELSZ),
    ENTRY(PLTGOT),
    ENTRY(HASH),
    ENTRY(STRTAB),
    ENTRY(SYMTAB),
    ENTRY(RELA),
    ENTRY(RELASZ),
    ENTRY(RELAENT),
    ENTRY(STRSZ),
    ENTRY(SYMENT),
    ENTRY(INIT),
    ENTRY(FINI),
    ENTRY(SONAME),
    ENTRY(RPATH),
    ENTRY(SYMBOLIC),
    ENTRY(REL),
    ENTRY(RELSZ),
    ENTRY(RELENT),
    ENTRY(PLTREL),
    ENTRY(DEBUG_TAG),
    ENTRY(TEXTREL),
    ENTRY(JMPREL),
    ENTRY(BIND_NOW),
    ENTRY(INIT_ARRAY),
    ENTRY(FINI_ARRAY),
    ENTRY(INIT_ARRAYSZ),
    ENTRY(FINI_ARRAYSZ),
    ENTRY(RUNPATH),
    ENTRY(FLAGS),
    ENTRY(PREINIT_ARRAY),
    ENTRY(PREINIT_ARRAYSZ),
    ENTRY(SYMTAB_SHNDX),
    ENTRY(RELRSZ),
    ENTRY(RELR),
    ENTRY(RELRENT),
    ENTRY(GNU_HASH),
    ENTRY(RELACOUNT),
    ENTRY(RELCOUNT),
    ENTRY(FLAGS_1),
    ENTRY(VERSYM),
    ENTRY(VERDEF),
    ENTRY(VERDEFNUM),
    ENTRY(VERNEED),
    ENTRY(VERNEEDNUM),
    ENTRY(ANDROID_REL_OFFSET),
    ENTRY(ANDROID_REL_SIZE),
    ENTRY(ANDROID_REL),
    ENTRY(ANDROID_RELSZ),
    ENTRY(ANDROID_RELA),
    ENTRY(ANDROID_RELASZ),
    ENTRY(ANDROID_RELR),
    ENTRY(ANDROID_RELRSZ),
    ENTRY(ANDROID_RELRENT),
    ENTRY(ANDROID_RELRCOUNT),
    ENTRY(MIPS_RLD_VERSION),
    ENTRY(MIPS_TIME_STAMP),
    ENTRY(MIPS_ICHECKSUM),
    ENTRY(MIPS_IVERSION),
    ENTRY(MIPS_FLAGS),
    ENTRY(MIPS_BASE_ADDRESS),
    ENTRY(MIPS_MSYM),
    ENTRY(MIPS_CONFLICT),
    ENTRY(MIPS_LIBLIST),
    ENTRY(MIPS_LOCAL_GOTNO),
    ENTRY(MIPS_CONFLICTNO),
    ENTRY(MIPS_LIBLISTNO),
    ENTRY(MIPS_SYMTABNO),
    ENTRY(MIPS_UNREFEXTNO),
    ENTRY(MIPS_GOTSYM),
    ENTRY(MIPS_HIPAGENO),
    ENTRY(MIPS_RLD_MAP),
    ENTRY(MIPS_DELTA_CLASS),
    ENTRY(MIPS_DELTA_CLASS_NO),
    ENTRY(MIPS_DELTA_INSTANCE),
    ENTRY(MIPS_DELTA_INSTANCE_NO),
    ENTRY(MIPS_DELTA_RELOC),
    ENTRY(MIPS_DELTA_RELOC_NO),
    ENTRY(MIPS_DELTA_SYM),
    ENTRY(MIPS_DELTA_SYM_NO),
    ENTRY(MIPS_DELTA_CLASSSYM),
    ENTRY(MIPS_DELTA_CLASSSYM_NO),
    ENTRY(MIPS_CXX_FLAGS),
    ENTRY(MIPS_PIXIE_INIT),
    ENTRY(MIPS_SYMBOL_LIB),
    ENTRY(MIPS_LOCALPAGE_GOTIDX),
    ENTRY(MIPS_LOCAL_GOTIDX),
    ENTRY(MIPS_HIDDEN_GOTIDX),
    ENTRY(MIPS_PROTECTED_GOTIDX),
    ENTRY(MIPS_OPTIONS),
    ENTRY(MIPS_INTERFACE),
    ENTRY(MIPS_DYNSTR_ALIGN),
    ENTRY(MIPS_INTERFACE_SIZE),
    ENTRY(MIPS_RLD_TEXT_RESOLVE_ADDR),
    ENTRY(MIPS_PERF_SUFFIX),
    ENTRY(MIPS_COMPACT_SIZE),
    ENTRY(MIPS_GP_VALUE),
    ENTRY(MIPS_AUX_DYNAMIC),
    ENTRY(MIPS_PLTGOT),
    ENTRY(MIPS_RWPLT),
    ENTRY(MIPS_RLD_MAP_REL),
    ENTRY(MIPS_XHASH),

    ENTRY(AARCH64_BTI_PLT),
    ENTRY(AARCH64_PAC_PLT),
    ENTRY(AARCH64_VARIANT_PCS),
    ENTRY(AARCH64_MEMTAG_MODE),
    ENTRY(AARCH64_MEMTAG_HEAP),
    ENTRY(AARCH64_MEMTAG_STACK),
    ENTRY(AARCH64_MEMTAG_GLOBALS),
    ENTRY(AARCH64_MEMTAG_GLOBALSSZ),

    ENTRY(HEXAGON_SYMSZ),
    ENTRY(HEXAGON_VER),
    ENTRY(HEXAGON_PLT),

    ENTRY(PPC_GOT),
    ENTRY(PPC_OPT),

    ENTRY(PPC64_GLINK),
    ENTRY(PPC64_OPT),

    ENTRY(RISCV_VARIANT_CC),

    ENTRY(X86_64_PLT),
    ENTRY(X86_64_PLTSZ),
    ENTRY(X86_64_PLTENT),

    ENTRY(IA_64_PLT_RESERVE),
    ENTRY(IA_64_VMS_SUBTYPE),
    ENTRY(IA_64_VMS_IMGIOCNT),
    ENTRY(IA_64_VMS_LNKFLAGS),
    ENTRY(IA_64_VMS_VIR_MEM_BLK_SIZ),
    ENTRY(IA_64_VMS_IDENT),
    ENTRY(IA_64_VMS_NEEDED_IDENT),
    ENTRY(IA_64_VMS_IMG_RELA_CNT),
    ENTRY(IA_64_VMS_SEG_RELA_CNT),
    ENTRY(IA_64_VMS_FIXUP_RELA_CNT),
    ENTRY(IA_64_VMS_FIXUP_NEEDED),
    ENTRY(IA_64_VMS_SYMVEC_CNT),
    ENTRY(IA_64_VMS_XLATED),
    ENTRY(IA_64_VMS_STACKSIZE),
    ENTRY(IA_64_VMS_UNWINDSZ),
    ENTRY(IA_64_VMS_UNWIND_CODSEG),
    ENTRY(IA_64_VMS_UNWIND_INFOSEG),
    ENTRY(IA_64_VMS_LINKTIME),
    ENTRY(IA_64_VMS_SEG_NO),
    ENTRY(IA_64_VMS_SYMVEC_OFFSET),
    ENTRY(IA_64_VMS_SYMVEC_SEG),
    ENTRY(IA_64_VMS_UNWIND_OFFSET),
    ENTRY(IA_64_VMS_UNWIND_SEG),
    ENTRY(IA_64_VMS_STRTAB_OFFSET),
    ENTRY(IA_64_VMS_SYSVER_OFFSET),
    ENTRY(IA_64_VMS_IMG_RELA_OFF),
    ENTRY(IA_64_VMS_SEG_RELA_OFF),
    ENTRY(IA_64_VMS_FIXUP_RELA_OFF),
    ENTRY(IA_64_VMS_PLTGOT_OFFSET),
    ENTRY(IA_64_VMS_PLTGOT_SEG),
    ENTRY(IA_64_VMS_FPMODE),
  };
  #undef ENTRY

  if (auto it = enums2str.find(tag); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

}
}
