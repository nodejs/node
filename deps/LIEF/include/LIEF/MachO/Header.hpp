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
#ifndef LIEF_MACHO_HEADER_H
#define LIEF_MACHO_HEADER_H

#include <ostream>
#include <vector>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/enums.hpp"

#include "LIEF/MachO/enums.hpp"

namespace LIEF {
namespace MachO {
class BinaryParser;

namespace details {
struct mach_header_64;
struct mach_header;
}

/// Class that represents the Mach-O header
class LIEF_API Header : public Object {
  friend class BinaryParser;
  public:
  Header() = default;

  Header& operator=(const Header& copy) = default;
  Header(const Header& copy) = default;

  ~Header() override = default;

  enum class FILE_TYPE : uint32_t {
    UNKNOWN     = 0,
    OBJECT      = 0x1u,
    EXECUTE     = 0x2u,
    FVMLIB      = 0x3u,
    CORE        = 0x4u,
    PRELOAD     = 0x5u,
    DYLIB       = 0x6u,
    DYLINKER    = 0x7u,
    BUNDLE      = 0x8u,
    DYLIB_STUB  = 0x9u,
    DSYM        = 0xAu,
    KEXT_BUNDLE = 0xBu,
    FILESET     = 0xCu,
    GPU_EXECUTE = 0xDu,
    GPU_DYLIB   = 0xEu,
  };

  enum class FLAGS : uint32_t {
    NOUNDEFS                      = 0x00000001u,
    INCRLINK                      = 0x00000002u,
    DYLDLINK                      = 0x00000004u,
    BINDATLOAD                    = 0x00000008u,
    PREBOUND                      = 0x00000010u,
    SPLIT_SEGS                    = 0x00000020u,
    LAZY_INIT                     = 0x00000040u,
    TWOLEVEL                      = 0x00000080u,
    FORCE_FLAT                    = 0x00000100u,
    NOMULTIDEFS                   = 0x00000200u,
    NOFIXPREBINDING               = 0x00000400u,
    PREBINDABLE                   = 0x00000800u,
    ALLMODSBOUND                  = 0x00001000u,
    SUBSECTIONS_VIA_SYMBOLS       = 0x00002000u,
    CANONICAL                     = 0x00004000u,
    WEAK_DEFINES                  = 0x00008000u,
    BINDS_TO_WEAK                 = 0x00010000u,
    ALLOW_STACK_EXECUTION         = 0x00020000u,
    ROOT_SAFE                     = 0x00040000u,
    SETUID_SAFE                   = 0x00080000u,
    NO_REEXPORTED_DYLIBS          = 0x00100000u,
    PIE                           = 0x00200000u,
    DEAD_STRIPPABLE_DYLIB         = 0x00400000u,
    HAS_TLV_DESCRIPTORS           = 0x00800000u,
    NO_HEAP_EXECUTION             = 0x01000000u,
    APP_EXTENSION_SAFE            = 0x02000000u,
    NLIST_OUTOFSYNC_WITH_DYLDINFO = 0x04000000u,
    SIM_SUPPORT                   = 0x08000000u,
    IMPLICIT_PAGEZERO             = 0x10000000u,
    DYLIB_IN_CACHE                = 0x80000000u,
  };

  static constexpr int ABI64 = 0x01000000;

  enum class CPU_TYPE: int32_t {
    ANY       = -1,
    X86       = 7,
    X86_64    = 7 | ABI64,
    MIPS      = 8,
    MC98000   = 10,
    HPPA      = 11,
    ARM       = 12,
    ARM64     = 12 | ABI64,
    MC88000   = 13,
    SPARC     = 14,
    I860      = 15,
    ALPHA	    = 16,
    POWERPC   = 18,
    POWERPC64 = 18 | ABI64,
    APPLE_GPU = 19 | ABI64,
    AMD_GPU   = 20 | ABI64,
    INTEL_GPU = 21 | ABI64,
    AIR64     = 23 | ABI64,
  };

  static constexpr uint32_t SUBTYPE_MASK = 0xff000000;
  static constexpr uint32_t SUBTYPE_LIB64 = 0x80000000;

  static constexpr auto CPU_SUBTYPE_ARM64_ARM64E = 2;

  /// The Mach-O magic bytes. These bytes determine whether it is
  /// a 32 bits Mach-O, a 64 bits Mach-O files etc.
  MACHO_TYPES magic() const {
    return magic_;
  }

  /// The CPU architecture targeted by this binary
  CPU_TYPE cpu_type() const {
    return cputype_;
  }

  /// Return the CPU subtype supported by the Mach-O binary.
  /// For ARM architectures, this value could represent the minimum version
  /// for which the Mach-O binary has been compiled for.
  uint32_t cpu_subtype() const {
    return cpusubtype_;
  }

  /// Return the type of the Mach-O file (executable, object, shared library, ...)
  FILE_TYPE file_type() const {
    return filetype_;
  }

  /// Return the FLAGS as a list
  std::vector<FLAGS> flags_list() const;

  /// Check if the given HEADER_FLAGS is present in the header's flags
  bool has(FLAGS flag) const;

  /// Number of LoadCommand present in the Mach-O binary
  uint32_t nb_cmds() const {
    return ncmds_;
  }

  /// The size of **all** the LoadCommand
  uint32_t sizeof_cmds() const {
    return sizeofcmds_;
  }

  /// Header flags (cf. HEADER_FLAGS)
  ///
  /// @see flags_list
  uint32_t flags() const {
    return flags_;
  }

  /// According to the official documentation, a reserved value
  uint32_t reserved() const {
    return reserved_;
  }

  void add(FLAGS flag);

  void magic(MACHO_TYPES magic) {
    magic_ = magic;
  }
  void cpu_type(CPU_TYPE type) {
    cputype_ = type;
  }

  void cpu_subtype(uint32_t cpusubtype) {
    cpusubtype_ = cpusubtype;
  }

  void file_type(FILE_TYPE filetype) {
    filetype_ = filetype;
  }

  void nb_cmds(uint32_t ncmds) {
    ncmds_ = ncmds;
  }

  void sizeof_cmds(uint32_t sizeofcmds) {
    sizeofcmds_ = sizeofcmds;
  }

  void flags(uint32_t flags) {
    flags_ = flags;
  }

  /// True if the binary is 32-bit
  bool is_32bit() const {
    return magic_ == MACHO_TYPES::MAGIC ||
           magic_ == MACHO_TYPES::CIGAM;
  }

  /// True if the binary is 64-bit
  bool is_64bit() const {
    return magic_ == MACHO_TYPES::MAGIC_64 ||
           magic_ == MACHO_TYPES::CIGAM_64;
  }

  void remove(FLAGS flag);

  void reserved(uint32_t reserved) {
    reserved_ = reserved;
  }

  Header& operator+=(FLAGS c) {
    add(c);
    return *this;
  }
  Header& operator-=(FLAGS c) {
    remove(c);
    return *this;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr);

  private:
  template<class T>
  LIEF_LOCAL Header(const T& header);

  MACHO_TYPES magic_ = MACHO_TYPES::UNKNOWN;
  CPU_TYPE   cputype_ = CPU_TYPE::ANY;
  uint32_t   cpusubtype_;
  FILE_TYPE  filetype_ = FILE_TYPE::UNKNOWN;
  uint32_t   ncmds_ = 0;
  uint32_t   sizeofcmds_ = 0;
  uint32_t   flags_ = 0;
  uint32_t   reserved_ = 0;
};

LIEF_API const char* to_string(Header::FILE_TYPE e);
LIEF_API const char* to_string(Header::CPU_TYPE e);
LIEF_API const char* to_string(Header::FLAGS e);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::MachO::Header::FLAGS);

#endif
