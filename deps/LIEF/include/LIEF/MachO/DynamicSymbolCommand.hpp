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
#ifndef LIEF_MACHO_DYNAMIC_SYMBOL_COMMAND_H
#define LIEF_MACHO_DYNAMIC_SYMBOL_COMMAND_H
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {
class Symbol;
class BinaryParser;
class Builder;
class Binary;

namespace details {
struct dysymtab_command;
}

/// Class that represents the LC_DYSYMTAB command.
///
/// This command completes the LC_SYMTAB (SymbolCommand) to provide
/// a better granularity over the symbols layout.
class LIEF_API DynamicSymbolCommand : public LoadCommand {
  friend class BinaryParser;
  friend class Builder;
  friend class Binary;

  public:
  /// Container for the indirect symbols references (owned by MachO::Binary)
  using indirect_symbols_t = std::vector<Symbol*>;

  /// Iterator for the indirect symbols referenced by this command
  using it_indirect_symbols = ref_iterator<indirect_symbols_t&>;
  using it_const_indirect_symbols = const_ref_iterator<const indirect_symbols_t&>;

  DynamicSymbolCommand();

  DynamicSymbolCommand(const details::dysymtab_command& cmd);

  DynamicSymbolCommand& operator=(const DynamicSymbolCommand& copy) = default;
  DynamicSymbolCommand(const DynamicSymbolCommand& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DynamicSymbolCommand>(new DynamicSymbolCommand(*this));
  }

  ~DynamicSymbolCommand() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  /// Index of the first symbol in the group of local symbols.
  uint32_t idx_local_symbol() const {
    return idx_local_symbol_;
  }

  /// Number of symbols in the group of local symbols.
  uint32_t nb_local_symbols() const {
    return nb_local_symbols_;
  }

  /// Index of the first symbol in the group of defined external symbols.
  uint32_t idx_external_define_symbol() const {
    return idx_external_define_symbol_;
  }

  /// Number of symbols in the group of defined external symbols.
  uint32_t nb_external_define_symbols() const {
    return nb_external_define_symbols_;
  }

  /// Index of the first symbol in the group of undefined external symbols.
  uint32_t idx_undefined_symbol() const {
    return idx_undefined_symbol_;
  }

  /// Number of symbols in the group of undefined external symbols.
  uint32_t nb_undefined_symbols() const {
    return nb_undefined_symbols_;
  }

  /// Byte offset from the start of the file to the table of contents data
  ///
  /// Table of content is used by legacy Mach-O loader and this field should be
  /// set to 0
  uint32_t toc_offset() const {
    return toc_offset_;
  }

  /// Number of entries in the table of contents.
  ///
  /// Should be set to 0 on recent Mach-O
  uint32_t nb_toc() const {
    return nb_toc_;
  }

  /// Byte offset from the start of the file to the module table data.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t module_table_offset() const {
    return module_table_offset_;
  }

  /// Number of entries in the module table.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t nb_module_table() const {
    return nb_module_table_;
  }

  /// Byte offset from the start of the file to the external reference table data.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t external_reference_symbol_offset() const {
    return external_reference_symbol_offset_;
  }

  /// Number of entries in the external reference table
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t nb_external_reference_symbols() const {
    return nb_external_reference_symbols_;
  }

  /// Byte offset from the start of the file to the indirect symbol table data.
  ///
  /// Indirect symbol table is used by the loader to speed-up symbol resolution during
  /// the *lazy binding* process
  ///
  /// References:
  ///   * dyld-519.2.1/src/ImageLoaderMachOCompressed.cpp
  ///   * dyld-519.2.1/src/ImageLoaderMachOClassic.cpp
  uint32_t indirect_symbol_offset() const {
    return indirect_sym_offset_;
  }

  /// Number of entries in the indirect symbol table.
  ///
  /// @see indirect_symbol_offset
  uint32_t nb_indirect_symbols() const {
    return nb_indirect_symbols_;
  }


  /// Byte offset from the start of the file to the external relocation table data.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t external_relocation_offset() const {
    return external_relocation_offset_;
  }

  /// Number of entries in the external relocation table.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t nb_external_relocations() const {
    return nb_external_relocations_;
  }

  /// Byte offset from the start of the file to the local relocation table data.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t local_relocation_offset() const {
    return local_relocation_offset_;
  }

  /// Number of entries in the local relocation table.
  ///
  /// This field seems unused by recent Mach-O loader and should be set to 0
  uint32_t nb_local_relocations() const {
    return nb_local_relocations_;
  }

  void idx_local_symbol(uint32_t value) {
    idx_local_symbol_ = value;
  }
  void nb_local_symbols(uint32_t value) {
    nb_local_symbols_ = value;
  }

  void idx_external_define_symbol(uint32_t value) {
    idx_external_define_symbol_ = value;
  }
  void nb_external_define_symbols(uint32_t value) {
    nb_external_define_symbols_ = value;
  }

  void idx_undefined_symbol(uint32_t value) {
    idx_undefined_symbol_ = value;
  }
  void nb_undefined_symbols(uint32_t value) {
    nb_undefined_symbols_ = value;
  }

  void toc_offset(uint32_t value) {
    toc_offset_ = value;
  }
  void nb_toc(uint32_t value) {
    nb_toc_ = value;
  }

  void module_table_offset(uint32_t value) {
    module_table_offset_ = value;
  }
  void nb_module_table(uint32_t value) {
    nb_module_table_ = value;
  }

  void external_reference_symbol_offset(uint32_t value) {
    external_reference_symbol_offset_ = value;
  }
  void nb_external_reference_symbols(uint32_t value) {
    nb_external_reference_symbols_ = value;
  }

  void indirect_symbol_offset(uint32_t value) {
    indirect_sym_offset_ = value;
  }
  void nb_indirect_symbols(uint32_t value) {
    nb_indirect_symbols_ = value;
  }

  void external_relocation_offset(uint32_t value) {
    external_relocation_offset_ = value;
  }
  void nb_external_relocations(uint32_t value) {
    nb_external_relocations_ = value;
  }

  void local_relocation_offset(uint32_t value) {
    local_relocation_offset_ = value;
  }
  void nb_local_relocations(uint32_t value) {
    nb_local_relocations_ = value;
  }

  /// Iterator over the indirect symbols indexed by this command
  it_indirect_symbols indirect_symbols() {
    return indirect_symbols_;
  }

  it_const_indirect_symbols indirect_symbols() const {
    return indirect_symbols_;
  }

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::DYSYMTAB;
  }

  private:
  uint32_t idx_local_symbol_ = 0;
  uint32_t nb_local_symbols_ = 0;

  uint32_t idx_external_define_symbol_ = 0;
  uint32_t nb_external_define_symbols_ = 0;

  uint32_t idx_undefined_symbol_ = 0;
  uint32_t nb_undefined_symbols_ = 0;

  uint32_t toc_offset_ = 0;
  uint32_t nb_toc_ = 0;

  uint32_t module_table_offset_ = 0;
  uint32_t nb_module_table_ = 0;

  uint32_t external_reference_symbol_offset_ = 0;
  uint32_t nb_external_reference_symbols_ = 0;

  uint32_t indirect_sym_offset_ = 0;
  uint32_t nb_indirect_symbols_ = 0;

  uint32_t external_relocation_offset_ = 0;
  uint32_t nb_external_relocations_ = 0;

  uint32_t local_relocation_offset_ = 0;
  uint32_t nb_local_relocations_ = 0;

  indirect_symbols_t indirect_symbols_;
};

}
}
#endif
