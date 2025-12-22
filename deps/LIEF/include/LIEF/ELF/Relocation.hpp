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
#ifndef LIEF_ELF_RELOCATION_H
#define LIEF_ELF_RELOCATION_H

#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

#include "LIEF/Abstract/Relocation.hpp"

#include "LIEF/ELF/enums.hpp"
#include "LIEF/ELF/Header.hpp"

namespace LIEF {
namespace ELF {

class Parser;
class Binary;
class Builder;
class Symbol;
class Section;

/// Class that represents an ELF relocation.
class LIEF_API Relocation : public LIEF::Relocation {

  friend class Parser;
  friend class Binary;
  friend class Builder;

  public:

  /// The *purpose* of a relocation defines how this relocation is used by the
  /// loader.
  enum class PURPOSE {
    NONE = 0,
    PLTGOT = 1,  ///< The relocation is associated with the PLT/GOT resolution
    DYNAMIC = 2, ///< The relocation is used for regulard data/code relocation
    OBJECT = 3,  ///< The relocation is used in an object file
  };

  enum class ENCODING {
    UNKNOWN = 0,
    REL,   ///< The relocation is using the regular Elf_Rel structure
    RELA,  ///< The relocation is using the regular Elf_Rela structure
    RELR,  ///< The relocation is using the relative relocation format
    ANDROID_SLEB, ///< The relocation is using the packed Android-SLEB128 format
  };

  static constexpr uint64_t R_BIT = 27;
  static constexpr uint64_t R_MASK = (uint64_t(1) << R_BIT) - 1;

  static constexpr uint64_t R_X64     = uint64_t(1)  << R_BIT;
  static constexpr uint64_t R_AARCH64 = uint64_t(2)  << R_BIT;
  static constexpr uint64_t R_ARM     = uint64_t(3)  << R_BIT;
  static constexpr uint64_t R_HEXAGON = uint64_t(4)  << R_BIT;
  static constexpr uint64_t R_X86     = uint64_t(5)  << R_BIT;
  static constexpr uint64_t R_LARCH   = uint64_t(6)  << R_BIT;
  static constexpr uint64_t R_MIPS    = uint64_t(7)  << R_BIT;
  static constexpr uint64_t R_PPC     = uint64_t(8)  << R_BIT;
  static constexpr uint64_t R_PPC64   = uint64_t(9)  << R_BIT;
  static constexpr uint64_t R_SPARC   = uint64_t(10) << R_BIT;
  static constexpr uint64_t R_SYSZ    = uint64_t(11) << R_BIT;
  static constexpr uint64_t R_RISCV   = uint64_t(12) << R_BIT;
  static constexpr uint64_t R_BPF     = uint64_t(13) << R_BIT;
  static constexpr uint64_t R_SH4     = uint64_t(14) << R_BIT;

  /// The different types of the relocation
  enum class TYPE : uint32_t {
    UNKNOWN = uint32_t(-1),

    #define ELF_RELOC(name, value) name = (value | R_X64),
      #include "LIEF/ELF/Relocations/x86_64.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_AARCH64),
      #include "LIEF/ELF/Relocations/AArch64.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_ARM),
      #include "LIEF/ELF/Relocations/ARM.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_HEXAGON),
      #include "LIEF/ELF/Relocations/Hexagon.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_X86),
      #include "LIEF/ELF/Relocations/i386.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_LARCH),
      #include "LIEF/ELF/Relocations/LoongArch.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_MIPS),
      #include "LIEF/ELF/Relocations/Mips.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_PPC),
      #include "LIEF/ELF/Relocations/PowerPC.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_PPC64),
      #include "LIEF/ELF/Relocations/PowerPC64.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_SPARC),
      #include "LIEF/ELF/Relocations/Sparc.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_SYSZ),
      #include "LIEF/ELF/Relocations/SystemZ.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_RISCV),
      #include "LIEF/ELF/Relocations/RISCV.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_BPF),
      #include "LIEF/ELF/Relocations/BPF.def"
    #undef ELF_RELOC

    #define ELF_RELOC(name, value) name = (value | R_SH4),
      #include "LIEF/ELF/Relocations/SH4.def"
    #undef ELF_RELOC
  };

  static TYPE type_from(uint32_t value, ARCH arch);

  static uint32_t to_value(TYPE type) {
    return static_cast<uint32_t>(type) & R_MASK;
  }

  Relocation(uint64_t address, TYPE type, ENCODING enc);

  Relocation() = default;
  Relocation(ARCH arch) {
    architecture_ = arch;
  }

  ~Relocation() override = default;

  /// Copy constructor.
  ///
  /// \warning When this constructor is invoked, referenced sections or symbols
  /// are discarded. This means that on the copied Relocation, Relocation::section,
  /// Relocation::symbol and Relocation::symbol_table are set to a nullptr.
  Relocation(const Relocation& other) :
    LIEF::Relocation{other},
    type_{other.type_},
    addend_{other.addend_},
    encoding_{other.encoding_},
    architecture_{other.architecture_}
  {}

  /// Copy assignment operator.
  ///
  /// Please read the notice of the copy constructor
  Relocation& operator=(Relocation other) {
    swap(other);
    return *this;
  }

  void swap(Relocation& other) {
    std::swap(address_,      other.address_);
    std::swap(type_,         other.type_);
    std::swap(addend_,       other.addend_);
    std::swap(encoding_,     other.encoding_);
    std::swap(symbol_,       other.symbol_);
    std::swap(architecture_, other.architecture_);
    std::swap(purpose_,      other.purpose_);
    std::swap(section_,      other.section_);
    std::swap(symbol_table_, other.symbol_table_);
    std::swap(info_,         other.info_);
    std::swap(binary_,       other.binary_);
  }

  /// Additional value that can be involved in the relocation processing
  int64_t addend() const {
    return addend_;
  }

  /// Type of the relocation
  TYPE type() const {
    return type_;
  }

  /// Check if the relocation uses the explicit addend() field
  /// (this is usually the case for 64 bits binaries)
  bool is_rela() const {
    return encoding_ == ENCODING::RELA;
  }

  /// Check if the relocation uses the implicit addend
  /// (i.e. not present in the ELF structure)
  bool is_rel() const {
    return encoding_ == ENCODING::REL;
  }

  /// True if the relocation is using the relative encoding
  bool is_relatively_encoded() const {
    return encoding_ == ENCODING::RELR;
  }

  /// True if the relocation is using the Android packed relocation format
  bool is_android_packed() const {
    return encoding_ == ENCODING::ANDROID_SLEB;
  }

  /// Relocation info which contains, for instance, the symbol index
  uint32_t info() const {
    return info_;
  }

  /// (re)Compute the *raw* `r_info` attribute based on the given ELF class
  uint64_t r_info(Header::CLASS clazz) const {
    if (clazz == Header::CLASS::NONE) {
      return 0;
    }
    return clazz == Header::CLASS::ELF32 ?
           uint32_t(info()) << 8  | to_value(type()) :
           uint64_t(info()) << 32 | (to_value(type()) & 0xffffffffL);
  }

  /// Target architecture for this relocation
  ARCH architecture() const {
    return architecture_;
  }

  PURPOSE purpose() const {
    return purpose_;
  }

  /// The encoding of the relocation
  ENCODING encoding() const {
    return encoding_;
  }

  /// True if the semantic of the relocation is `<ARCH>_RELATIVE`
  bool is_relative() const {
    return type_ == TYPE::AARCH64_RELATIVE || type_ == TYPE::X86_64_RELATIVE ||
           type_ == TYPE::X86_RELATIVE || type_ == TYPE::ARM_RELATIVE ||
           type_ == TYPE::HEX_RELATIVE || type_ == TYPE::PPC64_RELATIVE ||
           type_ == TYPE::PPC_RELATIVE;
  }

  /// Return the size (in **bits**) of the value associated with this relocation
  /// Return -1 if the size can't be determined
  size_t size() const override;

  /// True if the current relocation is associated with a symbol
  bool has_symbol() const {
    return symbol_ != nullptr;
  }

  /// Symbol associated with the relocation (or a nullptr)
  Symbol* symbol() {
    return symbol_;
  }

  const Symbol* symbol() const {
    return symbol_;
  }

  /// True if the relocation has an associated section
  bool has_section() const {
    return section() != nullptr;
  }

  /// The section in which the relocation is applied (or a nullptr)
  Section* section() {
    return section_;
  }

  const Section* section() const {
    return section_;
  }

  /// The associated symbol table (or a nullptr)
  Section* symbol_table() {
    return symbol_table_;
  }

  const Section* symbol_table() const {
    return symbol_table_;
  }

  void addend(int64_t addend) {
    addend_ = addend;
  }

  void type(TYPE type) {
    type_ = type;
  }

  void purpose(PURPOSE purpose) {
    purpose_ = purpose;
  }

  void info(uint32_t v) {
    info_ = v;
  }

  void symbol(Symbol* symbol) {
    symbol_ = symbol;
  }

  void section(Section* section) {
    section_ = section;
  }

  void symbol_table(Section* section) {
    symbol_table_ = section;
  }

  /// Try to resolve the value of the relocation such as
  /// `*address() = resolve()`
  result<uint64_t> resolve(uint64_t base_address = 0) const;

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Relocation& entry);

  private:
  template<class T>
  LIEF_LOCAL Relocation(const T& header, PURPOSE purpose, ENCODING enc, ARCH arch);

  TYPE type_ = TYPE::UNKNOWN;
  int64_t addend_ = 0;
  ENCODING encoding_ = ENCODING::UNKNOWN;
  Symbol* symbol_ = nullptr;
  ARCH architecture_ = ARCH::NONE;
  PURPOSE purpose_ = PURPOSE::NONE;
  Section* section_ = nullptr;
  Section* symbol_table_ = nullptr;
  uint32_t info_ = 0;

  Binary* binary_ = nullptr;
};

LIEF_API const char* to_string(Relocation::TYPE type);

}
}
#endif
