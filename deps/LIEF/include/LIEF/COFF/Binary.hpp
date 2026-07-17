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
#ifndef LIEF_COFF_BINARY_H
#define LIEF_COFF_BINARY_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/span.hpp"

#include "LIEF/COFF/String.hpp"

#include "LIEF/asm/Instruction.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace LIEF {

namespace assembly {
class Engine;
}

namespace COFF {
class Header;
class Parser;
class Section;
class Relocation;
class Symbol;

/// Class that represents a COFF Binary
class LIEF_API Binary {
  public:
  friend class Parser;

  /// Internal container used to store COFF's section
  using sections_t = std::vector<std::unique_ptr<Section>>;

  /// Iterator that outputs Section& object
  using it_sections = ref_iterator<sections_t&, Section*>;

  /// Iterator that outputs const Section& object
  using it_const_sections = const_ref_iterator<const sections_t&, const Section*>;

  /// Internal container used to store COFF's relocations
  using relocations_t = std::vector<std::unique_ptr<Relocation>>;

  /// Iterator that outputs Relocation& object
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;

  /// Iterator that outputs const Relocation& object
  using it_const_relocations = const_ref_iterator<const relocations_t&, const Relocation*>;

  /// Internal container used to store COFF's strings
  using strings_table_t = std::vector<String>;

  /// Iterator that outputs String& object
  using it_strings_table = ref_iterator<strings_table_t&>;

  /// Iterator that outputs const String& object
  using it_const_strings_table = const_ref_iterator<const strings_table_t&>;

  /// Internal container used to store COFF's symbols
  using symbols_t = std::vector<std::unique_ptr<Symbol>>;

  /// Iterator that outputs Symbol& object
  using it_symbols = ref_iterator<symbols_t&, Symbol*>;

  /// Iterator that outputs Symbol& object
  using it_const_symbols = const_ref_iterator<const symbols_t&, const Symbol*>;

  /// Instruction iterator
  using instructions_it = iterator_range<assembly::Instruction::Iterator>;

  /// Iterator which outputs COFF symbols representing functions
  using it_functions = filter_iterator<symbols_t&, Symbol*>;

  /// Iterator which outputs COFF symbols representing functions
  using it_const_function = const_filter_iterator<const symbols_t&, const Symbol*>;

  /// The COFF header
  const Header& header() const {
    return *header_;
  }

  Header& header() {
    return *header_;
  }

  /// Iterator over the different sections located in this COFF binary
  it_sections sections() {
    return sections_;
  }

  it_const_sections sections() const {
    return sections_;
  }

  /// Iterator over **all** the relocations used by this COFF binary
  it_relocations relocations() {
    return relocations_;
  }

  it_const_relocations relocations() const {
    return relocations_;
  }

  /// Iterator over the COFF's symbols
  it_symbols symbols() {
    return symbols_;
  }

  it_const_symbols symbols() const {
    return symbols_;
  }

  /// Iterator over the COFF's strings
  it_const_strings_table string_table() const {
    return strings_table_;
  }

  it_strings_table string_table() {
    return strings_table_;
  }

  /// Try to find the COFF string at the given offset in the COFF string table.
  ///
  /// \warning This offset must include the first 4 bytes holding the size of
  ///          the table. Hence, the first string starts a the offset 4.
  String* find_string(uint32_t offset) {
    auto it = std::find_if(strings_table_.begin(), strings_table_.end(),
      [offset] (const String& item) {
        return offset == item.offset();
      }
    );
    return it == strings_table_.end() ? nullptr : &*it;
  }

  const String* find_string(uint32_t offset) const {
    return const_cast<Binary*>(this)->find_string(offset);
  }

  /// Iterator over the functions implemented in this COFF
  it_const_function functions() const;

  it_functions functions();

  /// Try to find the function (symbol) with the given name
  const Symbol* find_function(const std::string& name) const;

  Symbol* find_function(const std::string& name) {
    return const_cast<Symbol*>(static_cast<const Binary*>(this)->find_function(name));
  }

  /// Try to find the function (symbol) with the given **demangled** name
  const Symbol* find_demangled_function(const std::string& name) const;

  Symbol* find_demangled_function(const std::string& name) {
    return const_cast<Symbol*>(static_cast<const Binary*>(this)->find_demangled_function(name));
  }

  /// Disassemble code for the given symbol
  ///
  /// ```cpp
  /// const Symbol* func = binary->find_demangled_function("int __cdecl my_function(int, int)");
  /// auto insts = binary->disassemble(*func);
  /// for (std::unique_ptr<assembly::Instruction> inst : insts) {
  ///   std::cout << inst->to_string() << '\n';
  /// }
  /// ```
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const Symbol& symbol) const;

  /// Disassemble code for the given symbol name
  ///
  /// ```cpp
  /// auto insts = binary->disassemble("main");
  /// for (std::unique_ptr<assembly::Instruction> inst : insts) {
  ///   std::cout << inst->to_string() << '\n';
  /// }
  /// ```
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const std::string& symbol) const;

  /// Disassemble code provided by the given buffer at the specified
  /// `address` parameter.
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const uint8_t* buffer, size_t size,
                              uint64_t address = 0) const;


  /// Disassemble code provided by the given vector of bytes at the specified
  /// `address` parameter.
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const std::vector<uint8_t>& buffer,
                              uint64_t address = 0) const {
    return disassemble(buffer.data(), buffer.size(), address);
  }

  instructions_it disassemble(LIEF::span<const uint8_t> buffer,
                              uint64_t address = 0) const {
    return disassemble(buffer.data(), buffer.size(), address);
  }

  instructions_it disassemble(LIEF::span<uint8_t> buffer, uint64_t address = 0) const {
    return disassemble(buffer.data(), buffer.size(), address);
  }

  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Binary& bin) {
    os << bin.to_string();
    return os;
  }

  ~Binary();

  private:
  Binary();
  std::unique_ptr<Header> header_;
  sections_t sections_;
  relocations_t relocations_;
  strings_table_t strings_table_;
  symbols_t symbols_;

  mutable std::unordered_map<uint32_t, std::unique_ptr<assembly::Engine>> engines_;

  assembly::Engine* get_engine(uint64_t address) const;

  template<uint32_t Key, class F>
  LIEF_LOCAL assembly::Engine* get_cache_engine(uint64_t address, F&& f) const;
};

}
}
#endif
