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
#ifndef LIEF_COFF_SYMBOL_H
#define LIEF_COFF_SYMBOL_H

#include "LIEF/Abstract/Symbol.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

#include <memory>
#include <vector>

namespace LIEF {
class BinaryStream;
namespace COFF {
class Parser;
class AuxiliarySymbol;
class String;
class Section;

/// This class represents a COFF symbol
class LIEF_API Symbol : public LIEF::Symbol {
  public:
  friend class Parser;

  using auxiliary_symbols_t = std::vector<std::unique_ptr<AuxiliarySymbol>>;
  using it_auxiliary_symbols_t = ref_iterator<auxiliary_symbols_t&, AuxiliarySymbol*>;
  using it_const_auxiliary_symbols_t = const_ref_iterator<const auxiliary_symbols_t&, AuxiliarySymbol*>;

  struct parsing_context_t {
    std::function<String*(uint32_t)> find_string;
    bool is_bigobj;
  };
  static std::unique_ptr<Symbol> parse(
      parsing_context_t& ctx, BinaryStream& stream, size_t* idx);

  Symbol();
  Symbol(const Symbol&);
  Symbol& operator=(const Symbol&);

  Symbol(Symbol&&);
  Symbol& operator=(Symbol&&);

  /// The symbol provides general type or debugging information but does not
  /// correspond to a section. Microsoft tools use this setting along with
  /// `.file` records.
  static constexpr auto SYM_SEC_IDX_DEBUG = -2;
  /// The symbol has an absolute (non-relocatable) value and is not an address.
  static constexpr auto SYM_SEC_IDX_ABS = -1;
  /// The symbol record is not yet assigned a section. A value of zero indicates
  /// that a reference to an external symbol is defined elsewhere. A value of
  /// non-zero is a common symbol with a size that is specified by the value.
  static constexpr auto SYM_SEC_IDX_UNDEF = 0;

  static constexpr auto SYM_COMPLEX_TYPE_SHIFT = 4;

  /// Reference: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#storage-class
  enum class STORAGE_CLASS : int32_t {
    INVALID = 0xFF,

    END_OF_FUNCTION  = -1,  ///< Physical end of function
    NONE             = 0,   ///< No symbol
    AUTOMATIC        = 1,   ///< Stack variable
    EXTERNAL         = 2,   ///< External symbol
    STATIC           = 3,   ///< Static
    REGISTER         = 4,   ///< Register variable
    EXTERNAL_DEF     = 5,   ///< External definition
    LABEL            = 6,   ///< Label
    UNDEFINED_LABEL  = 7,   ///< Undefined label
    MEMBER_OF_STRUCT = 8,   ///< Member of structure
    ARGUMENT         = 9,   ///< Function argument
    STRUCT_TAG       = 10,  ///< Structure tag
    MEMBER_OF_UNION  = 11,  ///< Member of union
    UNION_TAG        = 12,  ///< Union tag
    TYPE_DEFINITION  = 13,  ///< Type definition
    UNDEFINED_STATIC = 14,  ///< Undefined static
    ENUM_TAG         = 15,  ///< Enumeration tag
    MEMBER_OF_ENUM   = 16,  ///< Member of enumeration
    REGISTER_PARAM   = 17,  ///< Register parameter
    BIT_FIELD        = 18,  ///< Bit field
    BLOCK            = 100,
    FUNCTION         = 101,
    END_OF_STRUCT    = 102, ///< End of structure
    FILE             = 103, ///< File name
    SECTION          = 104,
    WEAK_EXTERNAL    = 105, ///< Duplicate tag
    CLR_TOKEN        = 107
  };

  enum class BASE_TYPE : uint32_t {
    TY_NULL = 0,   ///< No type information or unknown base type.
    TY_VOID = 1,   ///< Used with void pointers and functions.
    TY_CHAR = 2,   ///< A character (signed byte).
    TY_SHORT = 3,  ///< A 2-byte signed integer.
    TY_INT = 4,    ///< A natural integer type on the target.
    TY_LONG = 5,   ///< A 4-byte signed integer.
    TY_FLOAT = 6,  ///< A 4-byte floating-point number.
    TY_DOUBLE = 7, ///< An 8-byte floating-point number.
    TY_STRUCT = 8, ///< A structure.
    TY_UNION = 9,  ///< An union.
    TY_ENUM = 10,  ///< An enumerated type.
    TY_MOE = 11,   ///< A member of enumeration (a specific value).
    TY_BYTE = 12,  ///< A byte; unsigned 1-byte integer.
    TY_WORD = 13,  ///< A word; unsigned 2-byte integer.
    TY_UINT = 14,  ///< An unsigned integer of natural size.
    TY_DWORD = 15  ///< An unsigned 4-byte integer.
  };

  enum class COMPLEX_TYPE : uint32_t {
    TY_NULL = 0,     ///< No complex type; simple scalar variable.
    TY_POINTER = 1,  ///< A pointer to base type.
    TY_FUNCTION = 2, ///< A function that returns a base type.
    TY_ARRAY = 3,    ///< An array of base type.
  };

  /// Check if the given section index is a reserved value
  static constexpr bool is_reversed_sec_idx(int16_t idx) {
    return idx <= 0;
  }

  /// The symbol type. The first byte represents the base type (see: base_type())
  /// while the upper byte represents the complex type, if any (see:
  /// complex_type()).
  uint16_t type() const {
    return type_;
  }

  /// Storage class of the symbol which indicates what kind of definition a
  /// symbol represents.
  STORAGE_CLASS storage_class() const {
    return (STORAGE_CLASS)storage_class_;
  }

  /// The simple (base) data type
  BASE_TYPE base_type() const {
    return (BASE_TYPE)(type_ & 0x0F);
  }

  /// The complex type (if any)
  COMPLEX_TYPE complex_type() const {
    return (COMPLEX_TYPE)((type_ & 0xF0) >> SYM_COMPLEX_TYPE_SHIFT);
  }

  /// The signed integer that identifies the section, using a one-based index
  /// into the section table. Some values have special meaning:
  ///
  /// *  0: The symbol record is not yet assigned a section. A value of zero
  ///       indicates that a reference to an external symbol is defined elsewhere.
  ///       A value of non-zero is a common symbol with a size that is specified
  ///       by the value.
  /// * -1: The symbol has an absolute (non-relocatable) value and is not an
  ///       address.
  /// * -2: The symbol provides general type or debugging information but does
  ///       not correspond to a section. Microsoft tools use this setting along
  ///       with `.file` records
  int16_t section_idx() const {
    return section_idx_;
  }

  /// Section associated with this symbol (if any)
  Section* section() {
    return section_;
  }

  const Section* section() const {
    return section_;
  }

  bool is_external() const {
    return storage_class() == STORAGE_CLASS::EXTERNAL;
  }

  bool is_weak_external() const {
    return storage_class() == STORAGE_CLASS::WEAK_EXTERNAL;
  }

  bool is_absolute() const {
    return section_idx() == SYM_SEC_IDX_ABS;
  }

  bool is_undefined() const {
    return is_external() && section_idx() == SYM_SEC_IDX_UNDEF &&
           value() == 0;
  }

  bool is_function_line_info() const {
    return storage_class() == STORAGE_CLASS::FUNCTION;
  }

  bool is_function() const {
    return complex_type() == COMPLEX_TYPE::TY_FUNCTION;
  }

  bool is_file_record() const {
    return storage_class() == STORAGE_CLASS::FILE;
  }

  /// Auxiliary symbols associated with this symbol.
  it_auxiliary_symbols_t auxiliary_symbols() {
    return auxiliary_symbols_;
  }

  it_const_auxiliary_symbols_t auxiliary_symbols() const {
    return auxiliary_symbols_;
  }

  /// Name of the symbol. If the symbol does not use a short name, it returns
  /// the string pointed by the COFF string offset
  const std::string& name() const override;

  std::string& name() override;

  /// COFF string used to represents the (long) symbol name
  const String* coff_name() const {
    return coff_name_;
  }

  String* coff_name() {
    return coff_name_;
  }

  /// Demangled representation of the symbol or an empty string if it can't
  /// be demangled
  std::string demangled_name() const;

  Symbol& type(uint16_t ty) {
    type_ = ty;
    return *this;
  }

  Symbol& storage_class(uint8_t value) {
    storage_class_ = value;
    return *this;
  }

  Symbol& section_idx(int16_t idx) {
    section_idx_ = idx;
    return *this;
  }

  /// Add a new auxiliary record
  AuxiliarySymbol& add_aux(std::unique_ptr<AuxiliarySymbol> sym);

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const Symbol& entry)
  {
    os << entry.to_string();
    return os;
  }

  ~Symbol() override;

  private:
  template<class T>
  static std::unique_ptr<Symbol> parse_impl(
      parsing_context_t& ctx, BinaryStream& stream, size_t* idx);
  String* coff_name_ = nullptr;
  uint16_t type_ = 0;
  uint8_t storage_class_ = 0;
  int16_t section_idx_ = 0;
  auxiliary_symbols_t auxiliary_symbols_;
  Section* section_ = nullptr;
};

LIEF_API const char* to_string(Symbol::STORAGE_CLASS e);
LIEF_API const char* to_string(Symbol::BASE_TYPE e);
LIEF_API const char* to_string(Symbol::COMPLEX_TYPE e);

}
}
#endif
