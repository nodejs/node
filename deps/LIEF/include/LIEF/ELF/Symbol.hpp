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
#ifndef LIEF_ELF_SYMBOL_H
#define LIEF_ELF_SYMBOL_H

#include <string>
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Abstract/Symbol.hpp"
#include "LIEF/ELF/enums.hpp"

namespace LIEF {
namespace ELF {
class Parser;
class Binary;
class SymbolVersion;
class Section;

/// Class which represents an ELF symbol
class LIEF_API Symbol : public LIEF::Symbol {
  friend class Parser;
  friend class Binary;
  public:

  enum class BINDING {
    LOCAL      = 0,  ///< Local symbol
    GLOBAL     = 1,  ///< Global symbol
    WEAK       = 2,  ///< Weak symbol
    GNU_UNIQUE = 10, ///< Unique symbol
  };

  /// Type of the symbol. This enum matches the `STT_xxx` values of the ELF
  /// specs
  enum class TYPE {
    NOTYPE    = 0,   ///< Symbol's type is not specified
    OBJECT    = 1,   ///< Symbol is a data object (variable, array, etc.)
    FUNC      = 2,   ///< Symbol is executable code (function, etc.)
    SECTION   = 3,   ///< Symbol refers to a section
    FILE      = 4,   ///< Local, absolute symbol that refers to a file
    COMMON    = 5,   ///< An uninitialized common block
    TLS       = 6,   ///< Thread local data object
    GNU_IFUNC = 10,  ///< GNU indirect function
  };

  /// Visibility of the symbol. This enum matches the `STV_xxx` values of the
  /// official ELF specs
  enum class VISIBILITY {
    DEFAULT   = 0,  ///< Visibility is specified by binding type
    INTERNAL  = 1,  ///< Defined by processor supplements
    HIDDEN    = 2,  ///< Not visible to other components
    PROTECTED = 3   ///< Visible in other components but not preemptable
  };

  /// Special section indices
  enum SECTION_INDEX {
    UNDEF  = 0,      ///< Undefined section
    ABS    = 0xfff1, ///< Associated symbol is absolute
    COMMON = 0xfff2, ///< Associated symbol is common
  };

  public:
  Symbol(std::string name):
    LIEF::Symbol(std::move(name), 0, 0)
  {}

  static BINDING binding_from(uint32_t value, ARCH) {
    return BINDING(value);
  }

  static TYPE type_from(uint32_t value, ARCH) {
    return TYPE(value);
  }

  static uint8_t to_value(BINDING binding) {
    return static_cast<uint8_t>(binding);
  }

  static uint8_t to_value(TYPE type) {
    return static_cast<uint8_t>(type);
  }

  Symbol() = default;
  ~Symbol() override = default;

  Symbol& operator=(Symbol other);
  Symbol(const Symbol& other);
  void swap(Symbol& other);

  /// The symbol's type provides a general classification for the associated entity
  TYPE type() const {
    return type_;
  }

  /// The symbol's binding determines the linkage visibility and behavior
  BINDING binding() const {
    return binding_;
  }

  /// This member specifies the symbol's type and binding attributes.
  uint8_t information() const;

  /// Alias for visibility()
  uint8_t other() const {
    return other_;
  }

  /// ELF::Section index associated with the symbol
  uint16_t section_idx() const {
    return shndx();
  }

  /// Symbol visibility
  VISIBILITY visibility() const {
    return VISIBILITY(other_);
  }

  /// Section associated with the symbol or a nullptr if it does not exist.
  Section* section() {
    return section_;
  }

  const Section* section() const {
    return section_;
  }

  /// This member has slightly different interpretations:
  ///   * In relocatable files, `value` holds alignment constraints for a symbol for which section index
  ///     is SHN_COMMON
  ///   * In relocatable files, `value` holds a section offset for a defined symbol. That is, `value` is an
  ///     offset from the beginning of the section associated with this symbol.
  ///   * In executable and shared object files, `value` holds a virtual address. To make these files's
  ///     symbols more useful for the dynamic linker, the section offset (file interpretation) gives way to
  ///     a virtual address (memory interpretation) for which the section number is irrelevant.
  uint64_t value() const override {
    return value_;
  }

  /// Symbol size
  ///
  /// Many symbols have associated sizes. For example, a data object's size is the number of
  /// bytes contained in the object. This member holds `0` if the symbol has no size or
  /// an unknown size.
  uint64_t size() const override {
    return size_;
  }

  /// @see Symbol::section_idx
  uint16_t shndx() const {
    return shndx_;
  }

  /// Check if this symbols has a @link ELF::SymbolVersion symbol version @endlink
  bool has_version() const {
    return symbol_version_ != nullptr;
  }

  /// Return the SymbolVersion associated with this symbol.
  /// If there is no symbol version, return a nullptr
  SymbolVersion* symbol_version() {
    return symbol_version_;
  }

  const SymbolVersion* symbol_version() const {
    return symbol_version_;
  }

  bool is_local() const {
    return binding() == BINDING::LOCAL;
  }

  bool is_global() const {
    return binding() == BINDING::GLOBAL;
  }

  bool is_weak() const {
    return binding() == BINDING::WEAK;
  }

  /// Symbol's unmangled name. If not available, it returns an empty string
  std::string demangled_name() const;

  void type(TYPE type) {
    type_ = type;
  }

  void binding(BINDING binding) {
    binding_ = binding;
  }

  void other(uint8_t other) {
    other_ = other;
  }

  void visibility(VISIBILITY visibility) {
    other_ = static_cast<uint8_t>(visibility);
  }

  void information(uint8_t info);

  void shndx(uint16_t idx) {
    shndx_ = idx;
  }

  void value(uint64_t value) override {
    value_ = value;
  }

  void size(uint64_t size) override {
    size_ = size;
  }

  /// Check if the current symbol is exported
  bool is_exported() const;

  /// Set whether or not the symbol is exported
  void set_exported(bool flag = true);

  /// Check if the current symbol is imported
  bool is_imported() const;

  /// Set whether or not the symbol is imported
  void set_imported(bool flag = true);

  /// True if the symbol is a static one
  bool is_static() const {
    return this->binding() == BINDING::GLOBAL;
  }

  /// True if the symbol represent a function
  bool is_function() const {
    return this->type() == TYPE::FUNC;
  }

  /// True if the symbol represent a variable
  bool is_variable() const {
    return this->type() == TYPE::OBJECT;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Symbol& entry);

  private:
  template<class T>
  LIEF_API Symbol(const T& header, ARCH arch);

  TYPE    type_ = TYPE::NOTYPE;
  BINDING binding_ = BINDING::LOCAL;
  uint8_t other_   = 0;
  uint16_t shndx_   = 0;
  Section* section_ = nullptr;
  SymbolVersion* symbol_version_ = nullptr;
  ARCH arch_ = ARCH::NONE;
};

LIEF_API const char* to_string(Symbol::BINDING binding);
LIEF_API const char* to_string(Symbol::TYPE type);
LIEF_API const char* to_string(Symbol::VISIBILITY viz);
}
}
#endif /* _ELF_SYMBOL_H */
