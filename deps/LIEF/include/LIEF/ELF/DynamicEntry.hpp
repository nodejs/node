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
#ifndef LIEF_ELF_DYNAMIC_ENTRY_H
#define LIEF_ELF_DYNAMIC_ENTRY_H

#include <ostream>
#include <memory>
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/ELF/enums.hpp"

namespace LIEF {
namespace ELF {
namespace details {
struct Elf64_Dyn;
struct Elf32_Dyn;
}

/// Class which represents an entry in the dynamic table
/// These entries are located in the ``.dynamic`` section or the ``PT_DYNAMIC`` segment
class LIEF_API DynamicEntry : public Object {
  public:
  static constexpr uint64_t MIPS_DISC    = 0x100000000;
  static constexpr uint64_t AARCH64_DISC = 0x200000000;
  static constexpr uint64_t HEXAGON_DISC = 0x300000000;
  static constexpr uint64_t PPC_DISC     = 0x400000000;
  static constexpr uint64_t PPC64_DISC   = 0x500000000;
  static constexpr uint64_t RISCV_DISC   = 0x600000000;
  static constexpr uint64_t X86_64_DISC  = 0x700000000;
  static constexpr uint64_t IA_64_DISC   = 0x800000000;

  enum class TAG : uint64_t {
    UNKNOWN                    = uint64_t(-1),
    DT_NULL_                   = 0, /**< Marks end of dynamic array. */
    NEEDED                     = 1, /**< String table offset of needed library. */
    PLTRELSZ                   = 2, /**< Size of relocation entries in PLT. */
    PLTGOT                     = 3, /**< Address associated with linkage table. */
    HASH                       = 4, /**< Address of symbolic hash table. */
    STRTAB                     = 5, /**< Address of dynamic string table. */
    SYMTAB                     = 6, /**< Address of dynamic symbol table. */
    RELA                       = 7, /**< Address of relocation table (Rela entries). */
    RELASZ                     = 8, /**< Size of Rela relocation table. */
    RELAENT                    = 9, /**< Size of a Rela relocation entry. */
    STRSZ                      = 10,/**< Total size of the string table. */
    SYMENT                     = 11,/**< Size of a symbol table entry. */
    INIT                       = 12,/**< Address of initialization function. */
    FINI                       = 13,/**< Address of termination function. */
    SONAME                     = 14,/**< String table offset of a shared objects name. */
    RPATH                      = 15,/**< String table offset of library search path. */
    SYMBOLIC                   = 16,/**< Changes symbol resolution algorithm. */
    REL                        = 17,/**< Address of relocation table (Rel entries). */
    RELSZ                      = 18,/**< Size of Rel relocation table. */
    RELENT                     = 19,/**< Size of a Rel relocation entry. */
    PLTREL                     = 20,/**< Type of relocation entry used for linking. */
    DEBUG_TAG                  = 21,/**< Reserved for debugger. */
    TEXTREL                    = 22,/**< Relocations exist for non-writable segments. */
    JMPREL                     = 23,/**< Address of relocations associated with PLT. */
    BIND_NOW                   = 24,/**< Process all relocations before execution. */
    INIT_ARRAY                 = 25,/**< Pointer to array of initialization functions. */
    FINI_ARRAY                 = 26,/**< Pointer to array of termination functions. */
    INIT_ARRAYSZ               = 27,/**< Size of DT_INIT_ARRAY. */
    FINI_ARRAYSZ               = 28,/**< Size of DT_FINI_ARRAY. */
    RUNPATH                    = 29,/**< String table offset of lib search path. */
    FLAGS                      = 30,/**< Flags. */
    PREINIT_ARRAY              = 32,/**< Pointer to array of preinit functions. */
    PREINIT_ARRAYSZ            = 33,/**< Size of the DT_PREINIT_ARRAY array. */
    SYMTAB_SHNDX               = 34,/**< Address of SYMTAB_SHNDX section */
    RELRSZ                     = 35,/**< Total size of RELR relative relocations */
    RELR                       = 36,/**< Address of RELR relative relocations */
    RELRENT                    = 37,/**< Size of one RELR relative relocaction */

    // GNU Extensions
    GNU_HASH                   = 0x6FFFFEF5, /**< Reference to the GNU hash table. */
    RELACOUNT                  = 0x6FFFFFF9, /**< ELF32_Rela count. */
    RELCOUNT                   = 0x6FFFFFFA, /**< ELF32_Rel count. */
    FLAGS_1                    = 0x6FFFFFFB, /**< Flags_1. */
    VERSYM                     = 0x6FFFFFF0, /**< The address of .gnu.version section. */
    VERDEF                     = 0x6FFFFFFC, /**< The address of the version definition table. */
    VERDEFNUM                  = 0x6FFFFFFD, /**< The number of entries in DT_VERDEF. */
    VERNEED                    = 0x6FFFFFFE, /**< The address of the version Dependency table. */
    VERNEEDNUM                 = 0x6FFFFFFF, /**< The number of entries in DT_VERNEED. */

    // Android Extensions
    ANDROID_REL_OFFSET         = 0x6000000D, /**< The offset of packed relocation data (older version < M) (Android specific). */
    ANDROID_REL_SIZE           = 0x6000000E, /**< The size of packed relocation data in bytes (older version < M) (Android specific). */
    ANDROID_REL                = 0x6000000F, /**< The offset of packed relocation data (Android specific). */
    ANDROID_RELSZ              = 0x60000010, /**< The size of packed relocation data in bytes (Android specific). */
    ANDROID_RELA               = 0x60000011, /**< The offset of packed relocation data (Android specific). */
    ANDROID_RELASZ             = 0x60000012, /**< The size of packed relocation data in bytes (Android specific). */
    ANDROID_RELR               = 0x6FFFE000, /**< The offset of new relr relocation data (Android specific). */
    ANDROID_RELRSZ             = 0x6FFFE001, /**< The size of nre relr relocation data in bytes (Android specific). */
    ANDROID_RELRENT            = 0x6FFFE003, /**< The size of a new relr relocation entry (Android specific). */
    ANDROID_RELRCOUNT          = 0x6FFFE005,  /**< Specifies the relative count of new relr relocation entries (Android specific). */

    /* Mips specific dynamic table entry tags. */
    MIPS_RLD_VERSION           = MIPS_DISC + 0x70000001, /**< 32 bit version number for runtime linker interface. */
    MIPS_TIME_STAMP            = MIPS_DISC + 0x70000002, /**< Time stamp. */
    MIPS_ICHECKSUM             = MIPS_DISC + 0x70000003, /**< Checksum of external strings and common sizes. */
    MIPS_IVERSION              = MIPS_DISC + 0x70000004, /**< Index of version string in string table. */
    MIPS_FLAGS                 = MIPS_DISC + 0x70000005, /**< 32 bits of flags. */
    MIPS_BASE_ADDRESS          = MIPS_DISC + 0x70000006, /**< Base address of the segment. */
    MIPS_MSYM                  = MIPS_DISC + 0x70000007, /**< Address of .msym section. */
    MIPS_CONFLICT              = MIPS_DISC + 0x70000008, /**< Address of .conflict section. */
    MIPS_LIBLIST               = MIPS_DISC + 0x70000009, /**< Address of .liblist section. */
    MIPS_LOCAL_GOTNO           = MIPS_DISC + 0x7000000a, /**< Number of local global offset table entries. */
    MIPS_CONFLICTNO            = MIPS_DISC + 0x7000000b, /**< Number of entries in the .conflict section. */
    MIPS_LIBLISTNO             = MIPS_DISC + 0x70000010, /**< Number of entries in the .liblist section. */
    MIPS_SYMTABNO              = MIPS_DISC + 0x70000011, /**< Number of entries in the .dynsym section. */
    MIPS_UNREFEXTNO            = MIPS_DISC + 0x70000012, /**< Index of first external dynamic symbol not referenced locally. */
    MIPS_GOTSYM                = MIPS_DISC + 0x70000013, /**< Index of first dynamic symbol in global offset table. */
    MIPS_HIPAGENO              = MIPS_DISC + 0x70000014, /**< Number of page table entries in global offset table. */
    MIPS_RLD_MAP               = MIPS_DISC + 0x70000016, /**< Address of run time loader map, used for debugging. */
    MIPS_DELTA_CLASS           = MIPS_DISC + 0x70000017, /**< Delta C++ class definition. */
    MIPS_DELTA_CLASS_NO        = MIPS_DISC + 0x70000018, /**< Number of entries in DT_MIPS_DELTA_CLASS. */
    MIPS_DELTA_INSTANCE        = MIPS_DISC + 0x70000019, /**< Delta C++ class instances. */
    MIPS_DELTA_INSTANCE_NO     = MIPS_DISC + 0x7000001A, /**< Number of entries in DT_MIPS_DELTA_INSTANCE. */
    MIPS_DELTA_RELOC           = MIPS_DISC + 0x7000001B, /**< Delta relocations. */
    MIPS_DELTA_RELOC_NO        = MIPS_DISC + 0x7000001C, /**< Number of entries in DT_MIPS_DELTA_RELOC. */
    MIPS_DELTA_SYM             = MIPS_DISC + 0x7000001D, /**< Delta symbols that Delta relocations refer to. */
    MIPS_DELTA_SYM_NO          = MIPS_DISC + 0x7000001E, /**< Number of entries in DT_MIPS_DELTA_SYM. */
    MIPS_DELTA_CLASSSYM        = MIPS_DISC + 0x70000020, /**< Delta symbols that hold class declarations. */
    MIPS_DELTA_CLASSSYM_NO     = MIPS_DISC + 0x70000021, /**< Number of entries in DT_MIPS_DELTA_CLASSSYM. */
    MIPS_CXX_FLAGS             = MIPS_DISC + 0x70000022, /**< Flags indicating information about C++ flavor. */
    MIPS_PIXIE_INIT            = MIPS_DISC + 0x70000023, /**< Pixie information. */
    MIPS_SYMBOL_LIB            = MIPS_DISC + 0x70000024, /**< Address of .MIPS.symlib */
    MIPS_LOCALPAGE_GOTIDX      = MIPS_DISC + 0x70000025, /**< The GOT index of the first PTE for a segment */
    MIPS_LOCAL_GOTIDX          = MIPS_DISC + 0x70000026, /**< The GOT index of the first PTE for a local symbol */
    MIPS_HIDDEN_GOTIDX         = MIPS_DISC + 0x70000027, /**< The GOT index of the first PTE for a hidden symbol */
    MIPS_PROTECTED_GOTIDX      = MIPS_DISC + 0x70000028, /**< The GOT index of the first PTE for a protected symbol */
    MIPS_OPTIONS               = MIPS_DISC + 0x70000029, /**< Address of `.MIPS.options'. */
    MIPS_INTERFACE             = MIPS_DISC + 0x7000002A, /**< Address of `.interface'. */
    MIPS_DYNSTR_ALIGN          = MIPS_DISC + 0x7000002B, /**< Unknown. */
    MIPS_INTERFACE_SIZE        = MIPS_DISC + 0x7000002C, /**< Size of the .interface section. */
    MIPS_RLD_TEXT_RESOLVE_ADDR = MIPS_DISC + 0x7000002D, /**< Size of rld_text_resolve function stored in the GOT. */
    MIPS_PERF_SUFFIX           = MIPS_DISC + 0x7000002E, /**< Default suffix of DSO to be added by rld on dlopen() calls. */
    MIPS_COMPACT_SIZE          = MIPS_DISC + 0x7000002F, /**< Size of compact relocation section (O32). */
    MIPS_GP_VALUE              = MIPS_DISC + 0x70000030, /**< GP value for auxiliary GOTs. */
    MIPS_AUX_DYNAMIC           = MIPS_DISC + 0x70000031, /**< Address of auxiliary .dynamic. */
    MIPS_PLTGOT                = MIPS_DISC + 0x70000032, /**< Address of the base of the PLTGOT. */
    MIPS_RWPLT                 = MIPS_DISC + 0x70000034,
    MIPS_RLD_MAP_REL           = MIPS_DISC + 0x70000035,
    MIPS_XHASH                 = MIPS_DISC + 0x70000036,

    AARCH64_BTI_PLT            = AARCH64_DISC + 0x70000001,
    AARCH64_PAC_PLT            = AARCH64_DISC + 0x70000003,
    AARCH64_VARIANT_PCS        = AARCH64_DISC + 0x70000005,
    AARCH64_MEMTAG_MODE        = AARCH64_DISC + 0x70000009,
    AARCH64_MEMTAG_HEAP        = AARCH64_DISC + 0x7000000b,
    AARCH64_MEMTAG_STACK       = AARCH64_DISC + 0x7000000c,
    AARCH64_MEMTAG_GLOBALS     = AARCH64_DISC + 0x7000000d,
    AARCH64_MEMTAG_GLOBALSSZ   = AARCH64_DISC + 0x7000000f,

    HEXAGON_SYMSZ              = HEXAGON_DISC + 0x70000000,
    HEXAGON_VER                = HEXAGON_DISC + 0x70000001,
    HEXAGON_PLT                = HEXAGON_DISC + 0x70000002,

    PPC_GOT                    = PPC_DISC     + 0x70000000,
    PPC_OPT                    = PPC_DISC     + 0x70000001,

    PPC64_GLINK                = PPC64_DISC   + 0x70000000,
    PPC64_OPT                  = PPC64_DISC   + 0x70000003,

    RISCV_VARIANT_CC           = RISCV_DISC   + 0x70000003,

    X86_64_PLT                 = X86_64_DISC  + 0x70000000,
    X86_64_PLTSZ               = X86_64_DISC  + 0x70000001,
    X86_64_PLTENT              = X86_64_DISC  + 0x70000003,

    IA_64_PLT_RESERVE          = IA_64_DISC + (0x70000000 + 0),
    IA_64_VMS_SUBTYPE          = IA_64_DISC + (0x60000000 + 0),
    IA_64_VMS_IMGIOCNT         = IA_64_DISC + (0x60000000 + 2),
    IA_64_VMS_LNKFLAGS         = IA_64_DISC + (0x60000000 + 8),
    IA_64_VMS_VIR_MEM_BLK_SIZ  = IA_64_DISC + (0x60000000 + 10),
    IA_64_VMS_IDENT            = IA_64_DISC + (0x60000000 + 12),
    IA_64_VMS_NEEDED_IDENT     = IA_64_DISC + (0x60000000 + 16),
    IA_64_VMS_IMG_RELA_CNT     = IA_64_DISC + (0x60000000 + 18),
    IA_64_VMS_SEG_RELA_CNT     = IA_64_DISC + (0x60000000 + 20),
    IA_64_VMS_FIXUP_RELA_CNT   = IA_64_DISC + (0x60000000 + 22),
    IA_64_VMS_FIXUP_NEEDED     = IA_64_DISC + (0x60000000 + 24),
    IA_64_VMS_SYMVEC_CNT       = IA_64_DISC + (0x60000000 + 26),
    IA_64_VMS_XLATED           = IA_64_DISC + (0x60000000 + 30),
    IA_64_VMS_STACKSIZE        = IA_64_DISC + (0x60000000 + 32),
    IA_64_VMS_UNWINDSZ         = IA_64_DISC + (0x60000000 + 34),
    IA_64_VMS_UNWIND_CODSEG    = IA_64_DISC + (0x60000000 + 36),
    IA_64_VMS_UNWIND_INFOSEG   = IA_64_DISC + (0x60000000 + 38),
    IA_64_VMS_LINKTIME         = IA_64_DISC + (0x60000000 + 40),
    IA_64_VMS_SEG_NO           = IA_64_DISC + (0x60000000 + 42),
    IA_64_VMS_SYMVEC_OFFSET    = IA_64_DISC + (0x60000000 + 44),
    IA_64_VMS_SYMVEC_SEG       = IA_64_DISC + (0x60000000 + 46),
    IA_64_VMS_UNWIND_OFFSET    = IA_64_DISC + (0x60000000 + 48),
    IA_64_VMS_UNWIND_SEG       = IA_64_DISC + (0x60000000 + 50),
    IA_64_VMS_STRTAB_OFFSET    = IA_64_DISC + (0x60000000 + 52),
    IA_64_VMS_SYSVER_OFFSET    = IA_64_DISC + (0x60000000 + 54),
    IA_64_VMS_IMG_RELA_OFF     = IA_64_DISC + (0x60000000 + 56),
    IA_64_VMS_SEG_RELA_OFF     = IA_64_DISC + (0x60000000 + 58),
    IA_64_VMS_FIXUP_RELA_OFF   = IA_64_DISC + (0x60000000 + 60),
    IA_64_VMS_PLTGOT_OFFSET    = IA_64_DISC + (0x60000000 + 62),
    IA_64_VMS_PLTGOT_SEG       = IA_64_DISC + (0x60000000 + 64),
    IA_64_VMS_FPMODE           = IA_64_DISC + (0x60000000 + 66),
  };

  static TAG from_value(uint64_t value, ARCH arch);
  static uint64_t to_value(TAG tag);

  DynamicEntry() = default;
  DynamicEntry(const details::Elf64_Dyn& header, ARCH arch);
  DynamicEntry(const details::Elf32_Dyn& header, ARCH arch);

  DynamicEntry(TAG tag, uint64_t value) :
    tag_(tag), value_(value)
  {}

  DynamicEntry& operator=(const DynamicEntry&) = default;
  DynamicEntry(const DynamicEntry&) = default;
  ~DynamicEntry() override = default;

  static std::unique_ptr<DynamicEntry> create(TAG tag, uint64_t value);

  static std::unique_ptr<DynamicEntry> create(TAG tag) {
    return create(tag, /*value=*/0);
  }

  virtual std::unique_ptr<DynamicEntry> clone() const {
    return std::unique_ptr<DynamicEntry>(new DynamicEntry(*this));
  }

  /// Tag of the current entry. The most common tags are:
  /// DT_NEEDED, DT_INIT, ...
  TAG tag() const {
    return tag_;
  }

  /// Return the entry's value
  ///
  /// The meaning of the value strongly depends on the tag.
  /// It can be an offset, an index, a flag, ...
  uint64_t value() const {
    return value_;
  }

  void tag(TAG tag) {
    tag_ = tag;
  }

  void value(uint64_t value) {
    value_ = value;
  }

  void accept(Visitor& visitor) const override;

  virtual std::ostream& print(std::ostream& os) const;

  std::string to_string() const;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const DynamicEntry& entry) {
    return entry.print(os);
  }

  template<class T>
  const T* cast() const {
    static_assert(std::is_base_of<DynamicEntry, T>::value,
                  "Require DynamicEntry inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* cast() {
    return const_cast<T*>(static_cast<const DynamicEntry*>(this)->cast<T>());
  }

  protected:
  TAG      tag_ = TAG::DT_NULL_;
  uint64_t value_ = 0;
};

LIEF_API const char* to_string(DynamicEntry::TAG e);

}
}
#endif
