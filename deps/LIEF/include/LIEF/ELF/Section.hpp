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
#ifndef LIEF_ELF_SECTION_H
#define LIEF_ELF_SECTION_H

#include <string>
#include <ostream>
#include <vector>
#include <memory>

#include "LIEF/utils.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Abstract/Section.hpp"

#include "LIEF/ELF/enums.hpp"
#include "LIEF/iterators.hpp"

namespace LIEF {
class SpanStream;
namespace ELF {

namespace DataHandler {
class Handler;
}

class Segment;
class Parser;
class Binary;
class Builder;
class ExeLayout;
class ObjectFileLayout;


/// Class which represents an ELF Section
class LIEF_API Section : public LIEF::Section {
  friend class Parser;
  friend class Binary;
  friend class Builder;
  friend class ExeLayout;
  friend class ObjectFileLayout;

  public:
  using segments_t        = std::vector<Segment*>;
  using it_segments       = ref_iterator<segments_t&>;
  using it_const_segments = const_ref_iterator<const segments_t&>;

  static constexpr uint32_t MAX_SECTION_SIZE = 2_GB;

  enum class TYPE : uint64_t {
    SHT_NULL_           = 0,  /**< No associated section (inactive entry). */
    PROGBITS            = 1,  /**< Program-defined contents. */
    SYMTAB              = 2,  /**< Symbol table. */
    STRTAB              = 3,  /**< String table. */
    RELA                = 4,  /**< Relocation entries; explicit addends. */
    HASH                = 5,  /**< Symbol hash table. */
    DYNAMIC             = 6,  /**< Information for dynamic linking. */
    NOTE                = 7,  /**< Information about the file. */
    NOBITS              = 8,  /**< Data occupies no space in the file. */
    REL                 = 9,  /**< Relocation entries; no explicit addends. */
    SHLIB               = 10, /**< Reserved. */
    DYNSYM              = 11, /**< Symbol table. */
    INIT_ARRAY          = 14, /**< Pointers to initialization functions. */
    FINI_ARRAY          = 15, /**< Pointers to termination functions. */
    PREINIT_ARRAY       = 16, /**< Pointers to pre-init functions. */
    GROUP               = 17, /**< Section group. */
    SYMTAB_SHNDX        = 18, /**< Indices for SHN_XINDEX entries. */
    RELR                = 19, /**< Relocation entries; only offsets. */

    ANDROID_REL         = 0x60000001, /**< Packed relocations (Android specific). */
    ANDROID_RELA        = 0x60000002, /**< Packed relocations (Android specific). */
    LLVM_ADDRSIG        = 0x6fff4c03, /**< This section is used to mark symbols as address-significant. */
    ANDROID_RELR        = 0x6fffff00, /**< New relr relocations (Android specific). */
    GNU_ATTRIBUTES      = 0x6ffffff5, /**< Object attributes. */
    GNU_HASH            = 0x6ffffff6, /**< GNU-style hash table. */
    GNU_VERDEF          = 0x6ffffffd, /**< GNU version definitions. */
    GNU_VERNEED         = 0x6ffffffe, /**< GNU version references. */
    GNU_VERSYM          = 0x6fffffff, /**< GNU symbol versions table. */

    _ID_SHIFT_ = 32,
    _ARM_ID_ = 1LLU, _HEX_ID_ = 2LLU, _X86_64_ID_ = 2LLU,
    _MIPS_ID_ = 3LLU, _RISCV_ID_ = 4LLU,

    ARM_EXIDX           = 0x70000001U + (_ARM_ID_ << _ID_SHIFT_), /**< Exception Index table */
    ARM_PREEMPTMAP      = 0x70000002U + (_ARM_ID_ << _ID_SHIFT_), /**< BPABI DLL dynamic linking pre-emption map */
    ARM_ATTRIBUTES      = 0x70000003U + (_ARM_ID_ << _ID_SHIFT_), /**< Object file compatibility attributes */
    ARM_DEBUGOVERLAY    = 0x70000004U + (_ARM_ID_ << _ID_SHIFT_),
    ARM_OVERLAYSECTION  = 0x70000005U + (_ARM_ID_ << _ID_SHIFT_),

    HEX_ORDERED         = 0x70000000 + (_HEX_ID_ << _ID_SHIFT_), /**< Link editor is to sort the entries in this section based on their sizes */

    /* this section based on their sizes */
    X86_64_UNWIND       = 0x70000001 + (_X86_64_ID_ << _ID_SHIFT_), /**< Unwind information */

    MIPS_LIBLIST       = 0x70000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_MSYM          = 0x70000001 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_CONFLICT      = 0x70000002 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_GPTAB         = 0x70000003 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_UCODE         = 0x70000004 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DEBUG         = 0x70000005 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_REGINFO       = 0x70000006 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_PACKAGE       = 0x70000007 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_PACKSYM       = 0x70000008 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_RELD          = 0x70000009 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_IFACE         = 0x7000000b + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_CONTENT       = 0x7000000c + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_OPTIONS       = 0x7000000d + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_SHDR          = 0x70000010 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_FDESC         = 0x70000011 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_EXTSYM        = 0x70000012 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DENSE         = 0x70000013 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_PDESC         = 0x70000014 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_LOCSYM        = 0x70000015 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_AUXSYM        = 0x70000016 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_OPTSYM        = 0x70000017 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_LOCSTR        = 0x70000018 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_LINE          = 0x70000019 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_RFDESC        = 0x7000001a + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DELTASYM      = 0x7000001b + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DELTAINST     = 0x7000001c + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DELTACLASS    = 0x7000001d + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DWARF         = 0x7000001e + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_DELTADECL     = 0x7000001f + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_SYMBOL_LIB    = 0x70000020 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_EVENTS        = 0x70000021 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_TRANSLATE     = 0x70000022 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_PIXIE         = 0x70000023 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_XLATE         = 0x70000024 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_XLATE_DEBUG   = 0x70000025 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_WHIRL         = 0x70000026 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_EH_REGION     = 0x70000027 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_XLATE_OLD     = 0x70000028 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_PDR_EXCEPTION = 0x70000029 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_ABIFLAGS      = 0x7000002a + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_XHASH         = 0x7000002b + (_MIPS_ID_ << _ID_SHIFT_),

    RISCV_ATTRIBUTES    = 0x70000003 + (_RISCV_ID_ << _ID_SHIFT_),
  };

  enum class FLAGS : uint64_t {
    NONE                 = 0x000000000,
    WRITE                = 0x000000001,  /**< Section data should be writable during execution. */
    ALLOC                = 0x000000002,  /**< Section occupies memory during program execution. */
    EXECINSTR            = 0x000000004,  /**< Section contains executable machine instructions. */
    MERGE                = 0x000000010,  /**< The data in this section may be merged. */
    STRINGS              = 0x000000020,  /**< The data in this section is null-terminated strings. */
    INFO_LINK            = 0x000000040,  /**< A field in this section holds a section header table index. */
    LINK_ORDER           = 0x000000080,  /**< Adds special ordering requirements for link editors. */
    OS_NONCONFORMING     = 0x000000100,  /**< This section requires special OS-specific processing to avoid incorrect behavior */
    GROUP                = 0x000000200,  /**< This section is a member of a section group. */
    TLS                  = 0x000000400,  /**< This section holds Thread-Local Storage. */
    COMPRESSED           = 0x000000800,
    GNU_RETAIN           = 0x000200000,
    EXCLUDE              = 0x080000000,

    _ID_SHIFT_ = 32,
    _XCORE_ID_ = 1LLU, _HEX_ID_ = 3LLU, _X86_64_ID_ = 2LLU,
    _MIPS_ID_  = 4LLU, _ARM_ID_ = 5LLU,

    XCORE_SHF_DP_SECTION = 0x010000000 + (_XCORE_ID_ << _ID_SHIFT_),
    XCORE_SHF_CP_SECTION = 0x020000000 + (_XCORE_ID_ << _ID_SHIFT_),

    X86_64_LARGE         = 0x010000000 + (_X86_64_ID_ << _ID_SHIFT_),

    HEX_GPREL            = 0x010000000 + (_HEX_ID_ << _ID_SHIFT_),

    MIPS_NODUPES         = 0x001000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_NAMES           = 0x002000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_LOCAL           = 0x004000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_NOSTRIP         = 0x008000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_GPREL           = 0x010000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_MERGE           = 0x020000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_ADDR            = 0x040000000 + (_MIPS_ID_ << _ID_SHIFT_),
    MIPS_STRING          = 0x080000000 + (_MIPS_ID_ << _ID_SHIFT_),

    ARM_PURECODE         = 0x020000000 + (_ARM_ID_  << _ID_SHIFT_),
  };

  static constexpr uint64_t FLAG_MASK = (uint64_t(1) << uint8_t(FLAGS::_ID_SHIFT_)) - 1;
  static constexpr uint64_t TYPE_MASK = (uint64_t(1) << uint8_t(TYPE::_ID_SHIFT_)) - 1;

  static TYPE type_from(uint32_t value, ARCH arch);
  static uint32_t to_value(TYPE type) {
    return static_cast<uint64_t>(type) & TYPE_MASK;
  }

  Section(const std::string& name, TYPE type = TYPE::PROGBITS) :
    LIEF::Section(name),
    type_{type}
  {}

  Section() = default;
  ~Section() override = default;

  Section& operator=(Section other) {
    swap(other);
    return *this;
  }
  Section(const Section& other);
  void swap(Section& other) noexcept;

  TYPE type() const {
    return type_;
  }

  /// Section's content
  span<const uint8_t> content() const override;

  /// Set section content
  void content(const std::vector<uint8_t>& data) override;

  void content(std::vector<uint8_t>&& data);

  /// Section flags
  uint64_t flags() const {
    return flags_;
  }

  /// ``True`` if the section has the given flag
  bool has(FLAGS flag) const;

  /// ``True`` if the section is wrapped by the given Segment
  bool has(const Segment& segment) const;

  /// Return section flags as a ``std::set``
  std::vector<FLAGS> flags_list() const;

  uint64_t size() const override {
    return size_;
  }

  void size(uint64_t size) override;

  void offset(uint64_t offset) override;

  uint64_t offset() const override {
    return offset_;
  }

  /// @see offset
  uint64_t file_offset() const {
    return this->offset();
  }

  /// Original size of the section's data.
  ///
  /// This value is used by the ELF::Builder to determine if it needs
  /// to be relocated to avoid an override of the data
  uint64_t original_size() const {
    return original_size_;
  }

  /// Section file alignment
  uint64_t alignment() const {
    return address_align_;
  }

  /// Section information.
  /// The meaning of this value depends on the section's type
  uint64_t information() const {
    return info_;
  }

  /// This function returns the size of an element in the case of a section that contains
  /// an array.
  ///
  /// For instance, the `.dynamic` section contains an array of DynamicEntry. As the
  /// size of the raw C structure of this entry is 0x10 (`sizeoe(Elf64_Dyn)`)
  /// in a ELF64, the `entry_size` is set to this value.
  uint64_t entry_size() const {
    return entry_size_;
  }

  /// Index to another section
  uint32_t link() const {
    return link_;
  }

  /// Clear the content of the section with the given ``value``
  Section& clear(uint8_t value = 0);

  /// Add the given ELF_SECTION_FLAGS
  void add(FLAGS flag);

  /// Remove the given ELF_SECTION_FLAGS
  void remove(FLAGS flag);

  void type(TYPE type) {
    type_  = type;
  }

  void flags(uint64_t flags) {
    flags_ = flags;
  }

  void clear_flags() {
    flags_ = 0;
  }

  void file_offset(uint64_t offset) {
    this->offset(offset);
  }

  void link(uint32_t link) {
    link_ = link;
  }

  void information(uint32_t info) {
    info_ = info;
  }

  void alignment(uint64_t alignment) {
    address_align_ = alignment;
  }

  void entry_size(uint64_t entry_size) {
    entry_size_ = entry_size;
  }

  it_segments segments() {
    return segments_;
  }

  it_const_segments segments() const {
    return segments_;
  }

  Section& as_frame() {
    is_frame_ = true;
    return *this;
  }

  bool is_frame() const {
    return is_frame_;
  }

  void accept(Visitor& visitor) const override;

  Section& operator+=(FLAGS c) {
    add(c);
    return *this;
  }

  Section& operator-=(FLAGS c) {
    remove(c);
    return *this;
  }

  /// Return a stream over the content of this section
  std::unique_ptr<SpanStream> stream() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Section& section);

  private:
  template<class T>
  LIEF_LOCAL Section(const T& header, ARCH arch);

  LIEF_LOCAL span<uint8_t> writable_content();
  ARCH arch_ = ARCH::NONE;
  TYPE type_ = TYPE::SHT_NULL_;
  uint64_t flags_ = 0;
  uint64_t original_size_ = 0;
  uint32_t link_ = 0;
  uint32_t info_ = 0;
  uint64_t address_align_ = 0;
  uint64_t entry_size_ = 0;
  segments_t segments_;
  bool is_frame_ = false;
  DataHandler::Handler* datahandler_ = nullptr;
  std::vector<uint8_t> content_c_;
};

LIEF_API const char* to_string(Section::TYPE e);
LIEF_API const char* to_string(Section::FLAGS e);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::ELF::Section::FLAGS);

#endif
