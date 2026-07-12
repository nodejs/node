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
#ifndef LIEF_MACHO_SECTION_H
#define LIEF_MACHO_SECTION_H
#include <string>
#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/visibility.h"

#include "LIEF/Abstract/Section.hpp"
#include "LIEF/enums.hpp"

#include "LIEF/iterators.hpp"

namespace LIEF {
class SpanStream;

namespace MachO {

class BinaryParser;
class SegmentCommand;
class Binary;
class Relocation;

namespace details {
struct section_32;
struct section_64;
}

/// Class that represents a Mach-O section
class LIEF_API Section : public LIEF::Section {

  friend class BinaryParser;
  friend class Binary;
  friend class SegmentCommand;

  public:
  using content_t   = std::vector<uint8_t>;

  /// Internal container for storing Mach-O Relocation
  using relocations_t = std::vector<std::unique_ptr<Relocation>>;

  /// Iterator which outputs Relocation&
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;

  /// Iterator which outputs const Relocation&
  using it_const_relocations = const_ref_iterator<const relocations_t&, const Relocation*>;

  static constexpr auto FLAGS_MASK = uint32_t(0xffffff00u);
  static constexpr auto TYPE_MASK = uint32_t(0xff);

  enum class TYPE: uint64_t  {
    REGULAR                             = 0x00u, ///< Regular section.
    ZEROFILL                            = 0x01u, ///< Zero fill on demand section.
    CSTRING_LITERALS                    = 0x02u, ///< Section with literal C strings.
    IS_4BYTE_LITERALS                    = 0x03u, ///< Section with 4 byte literals.
    IS_8BYTE_LITERALS                    = 0x04u, ///< Section with 8 byte literals.
    LITERAL_POINTERS                    = 0x05u, ///< Section with pointers to literals.
    NON_LAZY_SYMBOL_POINTERS            = 0x06u, ///< Section with non-lazy symbol pointers.
    LAZY_SYMBOL_POINTERS                = 0x07u, ///< Section with lazy symbol pointers.
    SYMBOL_STUBS                        = 0x08u, ///< Section with symbol stubs, byte size of stub in the Reserved2 field.
    MOD_INIT_FUNC_POINTERS              = 0x09u, ///< Section with only function pointers for initialization.
    MOD_TERM_FUNC_POINTERS              = 0x0au, ///< Section with only function pointers for termination.
    COALESCED                           = 0x0bu, ///< Section contains symbols that are to be coalesced.
    GB_ZEROFILL                         = 0x0cu, ///< Zero fill on demand section (that can be larger than 4 gigabytes).
    INTERPOSING                         = 0x0du, ///< Section with only pairs of function pointers for interposing.
    IS_16BYTE_LITERALS                   = 0x0eu, ///< Section with only 16 byte literals.
    DTRACE_DOF                          = 0x0fu, ///< Section contains DTrace Object Format.
    LAZY_DYLIB_SYMBOL_POINTERS          = 0x10u, ///< Section with lazy symbol pointers to lazy loaded dylibs.
    THREAD_LOCAL_REGULAR                = 0x11u, ///< Thread local data section.
    THREAD_LOCAL_ZEROFILL               = 0x12u, ///< Thread local zerofill section.
    THREAD_LOCAL_VARIABLES              = 0x13u, ///< Section with thread local variable structure data.
    THREAD_LOCAL_VARIABLE_POINTERS      = 0x14u, ///< Section with pointers to thread local structures.
    THREAD_LOCAL_INIT_FUNCTION_POINTERS = 0x15u, ///< Section with thread local variable initialization pointers to functions.
    INIT_FUNC_OFFSETS                   = 0x16u, ///< Section with 32-bit offsets to initializer functions
  };

  enum class FLAGS: uint64_t  {
    PURE_INSTRUCTIONS   = 0x80000000u, ///< Section contains only true machine instructions
    NO_TOC              = 0x40000000u, ///< Section contains coalesced symbols that are not to be in a ranlib table of contents.
    STRIP_STATIC_SYMS   = 0x20000000u, ///< Ok to strip static symbols in this section in files with the MY_DYLDLINK flag.
    NO_DEAD_STRIP       = 0x10000000u, ///< No dead stripping.
    LIVE_SUPPORT        = 0x08000000u, ///< Blocks are live if they reference live blocks.
    SELF_MODIFYING_CODE = 0x04000000u, ///< Used with i386 code stubs written on by dyld
    DEBUG_INFO          = 0x02000000u, ///< A debug section.

    SOME_INSTRUCTIONS   = 0x00000400u, ///< Section contains some machine instructions.
    EXT_RELOC           = 0x00000200u, ///< Section has external relocation entries.
    LOC_RELOC           = 0x00000100u, ///< Section has local relocation entries.
  };

  public:
  Section();
  Section(const details::section_32& section_cmd);
  Section(const details::section_64& section_cmd);

  Section(std::string name);
  Section(std::string name, content_t content);

  Section& operator=(Section copy);
  Section(const Section& copy);

  void swap(Section& other) noexcept;

  ~Section() override;

  span<const uint8_t> content() const override;

  /// Update the content of the section
  void content(const content_t& data) override;

  /// Return the name of the segment linked to this section
  const std::string& segment_name() const;

  /// Virtual base address of the section
  uint64_t address() const {
    return virtual_address();
  }

  /// Section alignment as a power of 2
  uint32_t alignment() const {
    return align_;
  }

  /// Offset of the relocation table. This value should be 0
  /// for executable and libraries as the relocations are managed by the DyldInfo::rebase
  ///
  /// On the other hand, for object files (``.o``) this value should not be 0
  ///
  /// @see numberof_relocations
  /// @see relocations
  uint32_t relocation_offset() const {
    return relocations_offset_;
  }

  /// Number of relocations associated with this section
  uint32_t numberof_relocations() const {
    return nbof_relocations_;
  }

  /// Section's flags masked with SECTION_FLAGS_MASK (see: Section::FLAGS)
  ///
  /// @see flags
  FLAGS flags() const {
    return FLAGS(flags_ & FLAGS_MASK);
  }

  /// Type of the section. This value can help to determine
  /// the purpose of the section (e.g. TYPE::INTERPOSING)
  TYPE type() const {
    return TYPE(flags_ & TYPE_MASK);
  }

  /// According to the official ``loader.h`` file, this value is reserved
  /// for *offset* or *index*
  uint32_t reserved1() const {
    return reserved1_;
  }

  /// According to the official ``loader.h`` file, this value is reserved
  /// for *count* or *sizeof*
  uint32_t reserved2() const {
    return reserved2_;
  }

  /// This value is only present for 64 bits Mach-O files. In that case,
  /// the value is *reserved*.
  uint32_t reserved3() const {
    return reserved3_;
  }

  /// Return the Section::flags as a list of Section::FLAGS
  /// @see flags
  std::vector<FLAGS> flags_list() const;

  /// Section flags without applying the SECTION_FLAGS_MASK mask
  uint32_t raw_flags() const {
    return flags_;
  }

  /// Check if this section is correctly linked with a MachO::SegmentCommand
  bool has_segment() const {
    return segment() != nullptr;
  }

  /// The segment associated with this section or a nullptr
  /// if not present
  SegmentCommand* segment() {
    return segment_;
  }
  const SegmentCommand* segment() const {
    return segment_;
  }

  /// Return a stream over the content of this section
  std::unique_ptr<SpanStream> stream() const;

  /// Clear the content of this section by filling its values
  /// with the byte provided in parameter
  void clear(uint8_t v);

  /// Return an iterator over the MachO::Relocation associated with this section
  ///
  /// This iterator is likely to be empty of executable and libraries while it should not
  /// for object files (``.o``)
  it_relocations relocations() {
    return relocations_;
  }
  it_const_relocations relocations() const {
    return relocations_;
  }

  void segment_name(const std::string& name);
  void address(uint64_t address) {
    virtual_address(address);
  }
  void alignment(uint32_t align) {
    align_ = align;
  }
  void relocation_offset(uint32_t offset) {
    relocations_offset_ = offset;
  }
  void numberof_relocations(uint32_t nb_reloc) {
    nbof_relocations_ = nb_reloc;
  }
  void flags(uint32_t flags) {
    flags_ = flags_ | flags;
  }
  void flags(std::vector<FLAGS> flags);
  void type(TYPE type) {
    flags_ = (flags_ & FLAGS_MASK) | uint8_t(type);
  }
  void reserved1(uint32_t reserved1) {
    reserved1_ = reserved1;
  }
  void reserved2(uint32_t reserved2) {
    reserved2_ = reserved2;
  }
  void reserved3(uint32_t reserved3) {
    reserved3_ = reserved3;
  }

  /// Check if the section has the given Section::FLAGS flag
  bool has(FLAGS flag) const;

  /// Append a Section::FLAGS to the current section
  void add(FLAGS flag);

  /// Remove a Section::FLAGS to the current section
  void remove(FLAGS flag);

  Section& operator+=(FLAGS flag) {
    add(flag);
    return *this;
  }
  Section& operator-=(FLAGS flag) {
    remove(flag);
    return *this;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Section& section);

  private:
  std::string segment_name_;
  uint64_t original_size_ = 0;
  uint32_t align_ = 0;
  uint32_t relocations_offset_ = 0;
  uint32_t nbof_relocations_ = 0;
  uint32_t flags_ = 0;
  uint32_t reserved1_ = 0;
  uint32_t reserved2_ = 0;
  uint32_t reserved3_ = 0;
  content_t content_;
  SegmentCommand *segment_ = nullptr;
  relocations_t relocations_;
};

LIEF_API const char* to_string(Section::TYPE type);
LIEF_API const char* to_string(Section::FLAGS flag);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::MachO::Section::FLAGS);

#endif
