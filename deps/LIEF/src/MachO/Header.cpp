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

#include <map>
#include <set>
#include <algorithm>
#include <iterator>
#include <string>

#include "fmt_formatter.hpp"
#include "frozen.hpp"
#include "spdlog/fmt/fmt.h"

#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/Header.hpp"
#include "MachO/Structures.hpp"

FMT_FORMATTER(LIEF::MachO::Header::FLAGS, LIEF::MachO::to_string);

namespace LIEF {
namespace MachO {

static constexpr auto HEADER_FLAGS = {
  Header::FLAGS::NOUNDEFS, Header::FLAGS::INCRLINK,
  Header::FLAGS::DYLDLINK, Header::FLAGS::BINDATLOAD,
  Header::FLAGS::PREBOUND, Header::FLAGS::SPLIT_SEGS,
  Header::FLAGS::LAZY_INIT, Header::FLAGS::TWOLEVEL,
  Header::FLAGS::FORCE_FLAT, Header::FLAGS::NOMULTIDEFS,
  Header::FLAGS::NOFIXPREBINDING, Header::FLAGS::PREBINDABLE,
  Header::FLAGS::ALLMODSBOUND, Header::FLAGS::SUBSECTIONS_VIA_SYMBOLS,
  Header::FLAGS::CANONICAL, Header::FLAGS::WEAK_DEFINES,
  Header::FLAGS::BINDS_TO_WEAK, Header::FLAGS::ALLOW_STACK_EXECUTION,
  Header::FLAGS::ROOT_SAFE, Header::FLAGS::SETUID_SAFE,
  Header::FLAGS::NO_REEXPORTED_DYLIBS, Header::FLAGS::PIE,
  Header::FLAGS::DEAD_STRIPPABLE_DYLIB, Header::FLAGS::HAS_TLV_DESCRIPTORS,
  Header::FLAGS::NO_HEAP_EXECUTION, Header::FLAGS::APP_EXTENSION_SAFE,
  Header::FLAGS::NLIST_OUTOFSYNC_WITH_DYLDINFO, Header::FLAGS::SIM_SUPPORT,
  Header::FLAGS::IMPLICIT_PAGEZERO, Header::FLAGS::DYLIB_IN_CACHE,
};

template<class T>
Header::Header(const T& header) :
  magic_{static_cast<MACHO_TYPES>(header.magic)},
  cputype_(static_cast<CPU_TYPE>(header.cputype)),
  cpusubtype_{header.cpusubtype},
  filetype_{static_cast<FILE_TYPE>(header.filetype)},
  ncmds_{header.ncmds},
  sizeofcmds_{header.sizeofcmds},
  flags_{header.flags}
{
  if constexpr (std::is_same_v<T, details::mach_header_64>) {
    reserved_ = header.reserved;
  } else {
    reserved_ = 0;
  }
}

template Header::Header(const details::mach_header_64& header);
template Header::Header(const details::mach_header& header);


std::vector<Header::FLAGS> Header::flags_list() const {
  std::vector<Header::FLAGS> flags;

  std::copy_if(std::begin(HEADER_FLAGS), std::end(HEADER_FLAGS),
               std::inserter(flags, std::begin(flags)),
               [this] (FLAGS f) { return has(f); });

  return flags;
}


bool Header::has(FLAGS flag) const {
  return (flags() & static_cast<uint32_t>(flag)) > 0;
}

void Header::add(FLAGS flag) {
  flags(flags() | static_cast<uint32_t>(flag));
}

void Header::remove(FLAGS flag) {
  flags(flags() & ~static_cast<uint32_t>(flag));
}

void Header::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Header& hdr) {
  os << fmt::format("Magic: 0x{:08x}\n", uint32_t(hdr.magic()));
  os << fmt::format("CPU: {}\n", to_string(hdr.cpu_type()));
  os << fmt::format("CPU subtype: 0x{:08x}\n", hdr.cpu_subtype());
  os << fmt::format("File type: {} ({:#x})\n", to_string(hdr.file_type()), (uint32_t)hdr.file_type());
  os << fmt::format("Flags: {}\n", hdr.flags());
  os << fmt::format("Reserved: 0x{:x}\n", hdr.reserved());
  os << fmt::format("Nb cmds: {}\n", hdr.nb_cmds());
  os << fmt::format("Sizeof cmds: {}", hdr.sizeof_cmds());
  return os;
}

const char* to_string(Header::FLAGS e) {
  #define ENTRY(X) std::pair(Header::FLAGS::X, #X)
  STRING_MAP enums2str {
    ENTRY(NOUNDEFS),
    ENTRY(INCRLINK),
    ENTRY(DYLDLINK),
    ENTRY(BINDATLOAD),
    ENTRY(PREBOUND),
    ENTRY(SPLIT_SEGS),
    ENTRY(LAZY_INIT),
    ENTRY(TWOLEVEL),
    ENTRY(FORCE_FLAT),
    ENTRY(NOMULTIDEFS),
    ENTRY(NOFIXPREBINDING),
    ENTRY(PREBINDABLE),
    ENTRY(ALLMODSBOUND),
    ENTRY(SUBSECTIONS_VIA_SYMBOLS),
    ENTRY(CANONICAL),
    ENTRY(WEAK_DEFINES),
    ENTRY(BINDS_TO_WEAK),
    ENTRY(ALLOW_STACK_EXECUTION),
    ENTRY(ROOT_SAFE),
    ENTRY(SETUID_SAFE),
    ENTRY(NO_REEXPORTED_DYLIBS),
    ENTRY(PIE),
    ENTRY(DEAD_STRIPPABLE_DYLIB),
    ENTRY(HAS_TLV_DESCRIPTORS),
    ENTRY(NO_HEAP_EXECUTION),
    ENTRY(APP_EXTENSION_SAFE),
    ENTRY(NLIST_OUTOFSYNC_WITH_DYLDINFO),
    ENTRY(SIM_SUPPORT),
    ENTRY(IMPLICIT_PAGEZERO),
    ENTRY(DYLIB_IN_CACHE),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Header::FILE_TYPE e) {
  #define ENTRY(X) std::pair(Header::FILE_TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(OBJECT),
    ENTRY(EXECUTE),
    ENTRY(FVMLIB),
    ENTRY(CORE),
    ENTRY(PRELOAD),
    ENTRY(DYLIB),
    ENTRY(DYLINKER),
    ENTRY(BUNDLE),
    ENTRY(DYLIB_STUB),
    ENTRY(DSYM),
    ENTRY(KEXT_BUNDLE),
    ENTRY(FILESET),
    ENTRY(GPU_EXECUTE),
    ENTRY(GPU_DYLIB),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Header::CPU_TYPE e) {
  #define ENTRY(X) std::pair(Header::CPU_TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(ANY),
    ENTRY(X86),
    ENTRY(X86_64),
    ENTRY(MIPS),
    ENTRY(MC98000),
    ENTRY(HPPA),
    ENTRY(ARM),
    ENTRY(ARM64),
    ENTRY(MC88000),
    ENTRY(SPARC),
    ENTRY(I860),
    ENTRY(ALPHA),
    ENTRY(POWERPC),
    ENTRY(POWERPC64),
    ENTRY(APPLE_GPU),
    ENTRY(AMD_GPU),
    ENTRY(INTEL_GPU),
    ENTRY(AIR64),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
