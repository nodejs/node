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
#ifndef LIEF_ELF_BINARY_H
#define LIEF_ELF_BINARY_H

#include <vector>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/Abstract/Binary.hpp"

#include "LIEF/ELF/Note.hpp"
#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/Header.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Builder.hpp"

namespace LIEF {
/// Namespace related to the LIEF's ELF module
namespace ELF {
namespace DataHandler {
class Handler;
}

class ExeLayout;
class GnuHash;
class Layout;
class ObjectFileLayout;
class Parser;
class Relocation;
class Section;
class Segment;
class Symbol;
class SymbolVersion;
class SymbolVersionDefinition;
class SymbolVersionRequirement;
class DynamicEntryLibrary;
class SysvHash;
struct sizing_info_t;

/// Class which represents an ELF binary
class LIEF_API Binary : public LIEF::Binary {
  friend class Parser;
  friend class Builder;
  friend class ExeLayout;
  friend class Layout;
  friend class ObjectFileLayout;

  public:
  using string_list_t  = std::vector<std::string>;

  /// Internal container for storing notes
  using notes_t = std::vector<std::unique_ptr<Note>>;

  /// Iterator which outputs Note& object
  using it_notes = ref_iterator<notes_t&, Note*>;

  /// Iterator which outputs const Note& object
  using it_const_notes = const_ref_iterator<const notes_t&, const Note*>;

  /// Internal container for storing SymbolVersionRequirement
  using symbols_version_requirement_t = std::vector<std::unique_ptr<SymbolVersionRequirement>>;

  /// Iterator which outputs SymbolVersionRequirement& object
  using it_symbols_version_requirement = ref_iterator<symbols_version_requirement_t&, SymbolVersionRequirement*>;

  /// Iterator which outputs const SymbolVersionRequirement& object
  using it_const_symbols_version_requirement = const_ref_iterator<const symbols_version_requirement_t&, const SymbolVersionRequirement*>;

  /// Internal container for storing SymbolVersionDefinition
  using symbols_version_definition_t = std::vector<std::unique_ptr<SymbolVersionDefinition>>;

  /// Iterator which outputs SymbolVersionDefinition& object
  using it_symbols_version_definition       = ref_iterator<symbols_version_definition_t&, SymbolVersionDefinition*>;

  /// Iterator which outputs const SymbolVersionDefinition& object
  using it_const_symbols_version_definition = const_ref_iterator<const symbols_version_definition_t&, const SymbolVersionDefinition*>;

  /// Internal container for storing ELF's Segment
  using segments_t = std::vector<std::unique_ptr<Segment>>;

  /// Iterator which outputs Segment& object
  using it_segments = ref_iterator<segments_t&, Segment*>;

  /// Iterator which outputs const Segment& object
  using it_const_segments = const_ref_iterator<const segments_t&, const Segment*>;

  /// Internal container for storing ELF's DynamicEntry
  using dynamic_entries_t = std::vector<std::unique_ptr<DynamicEntry>>;

  /// Iterator which outputs DynamicEntry& object
  using it_dynamic_entries = ref_iterator<dynamic_entries_t&, DynamicEntry*>;

  /// Iterator which outputs const DynamicEntry& object
  using it_const_dynamic_entries = const_ref_iterator<const dynamic_entries_t&, const DynamicEntry*>;

  /// Internal container for storing ELF's SymbolVersion
  using symbols_version_t = std::vector<std::unique_ptr<SymbolVersion>>;

  /// Iterator which outputs SymbolVersion& object
  using it_symbols_version = ref_iterator<symbols_version_t&, SymbolVersion*>;

  /// Iterator which outputs const SymbolVersion& object
  using it_const_symbols_version = const_ref_iterator<const symbols_version_t&, const SymbolVersion*>;

  /// Internal container for storing ELF's Relocation
  using relocations_t = std::vector<std::unique_ptr<Relocation>>;

  /// Iterator which outputs plt/got Relocation& object
  using it_pltgot_relocations = filter_iterator<relocations_t&, Relocation*>;

  /// Iterator which outputs plt/got const Relocation& object
  using it_const_pltgot_relocations = const_filter_iterator<const relocations_t&, const Relocation*>;

  /// Iterator which outputs dynamic Relocation& object (not related to the PLT/GOT mechanism)
  using it_dynamic_relocations = filter_iterator<relocations_t&, Relocation*>;

  /// Iterator which outputs dynamic const Relocation& object (not related to the PLT/GOT mechanism)
  using it_const_dynamic_relocations = const_filter_iterator<const relocations_t&, const Relocation*>;

  /// Iterator which outputs Relocation& object found in object files (.o)
  using it_object_relocations = filter_iterator<relocations_t&, Relocation*>;

  /// Iterator which outputs const Relocation& object found in object files (.o)
  using it_const_object_relocations = const_filter_iterator<const relocations_t&, const Relocation*>;

  /// Iterator which outputs Relocation& object
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;

  /// Iterator which outputs const Relocation& object
  using it_const_relocations = const_ref_iterator<const relocations_t&, const Relocation*>;

  /// Internal container for storing ELF's Symbol
  using symbols_t = std::vector<std::unique_ptr<Symbol>>;

  /// Iterator which outputs the Dynamic Symbol& object
  using it_dynamic_symbols = ref_iterator<symbols_t&, Symbol*>;

  /// Iterator which outputs the Dynamic const Symbol& object
  using it_const_dynamic_symbols = const_ref_iterator<const symbols_t&, const Symbol*>;

  /// Iterator which outputs the static/debug Symbol& object
  using it_symtab_symbols = ref_iterator<symbols_t&, Symbol*>;

  /// Iterator which outputs the static/debug const Symbol& object
  using it_const_symtab_symbols = const_ref_iterator<const symbols_t&, const Symbol*>;

  /// Iterator which outputs static and dynamic Symbol& object
  using it_symbols = ref_iterator<std::vector<Symbol*>>;

  /// Iterator which outputs static and dynamic const Symbol& object
  using it_const_symbols = const_ref_iterator<std::vector<Symbol*>>;

  /// Iterator which outputs exported Symbol& object
  using it_exported_symbols = filter_iterator<std::vector<Symbol*>>;

  /// Iterator which outputs exported const Symbol& object
  using it_const_exported_symbols = const_filter_iterator<std::vector<Symbol*>>;

  /// Iterator which outputs imported Symbol& object
  using it_imported_symbols = filter_iterator<std::vector<Symbol*>>;

  /// Iterator which outputs imported const Symbol& object
  using it_const_imported_symbols = const_filter_iterator<std::vector<Symbol*>>;

  /// Internal container for storing ELF's Section
  using sections_t = std::vector<std::unique_ptr<Section>>;

  /// Iterator which outputs Section& object
  using it_sections = ref_iterator<sections_t&, Section*>;

  /// Iterator which outputs const Section& object
  using it_const_sections = const_ref_iterator<const sections_t&, const Section*>;

  public:
  /**
   * This enum describes the different ways to relocate the segments table.
   */
  enum PHDR_RELOC {
    /**
     * Defer the choice of the layout to LIEF.
     */
    AUTO = 0,

    /**
     * The content of the binary right after the segments table is shifted
     * and the relocations are updated accordingly.
     * This kind of shift only works with PIE binaries.
     */
    PIE_SHIFT,

    /**
     * The new segments table is relocated right after the first bss-like
     * segment.
     */
    BSS_END,
    /**
     * The new segments table is relocated at the end of the binary.
     */
    BINARY_END,
    /**
     * The new segments table is relocated between two LOAD segments.
     * This kind of relocation is only doable when there is an alignment
     * enforcement.
     */
    SEGMENT_GAP,
  };

  /// This enum defines where the content of a newly added section should be
  /// inserted.
  enum class SEC_INSERT_POS {
    /// Defer the choice to LIEF
    AUTO = 0,

    /// Insert the section after the last valid offset in the **segments**
    /// table.
    ///
    /// With this choice, the section is inserted after the loaded content but
    /// before any debug information.
    POST_SEGMENT,

    /// Insert the section after the last valid offset in the **section** table.
    ///
    /// With this choice, the section is inserted at the very end of the binary.
    POST_SECTION,
  };

  public:
  Binary& operator=(const Binary& ) = delete;
  Binary(const Binary& copy) = delete;

  /// Return binary's class (ELF32 or ELF64)
  Header::CLASS type() const {
    return type_;
  }

  /// Return @link ELF::Header Elf header @endlink
  Header& header() {
    return header_;
  }

  const Header& header() const {
    return header_;
  }

  /// Return the last offset used in binary
  /// according to sections table
  uint64_t last_offset_section() const;

  /// Return the last offset used in binary
  /// according to segments table
  uint64_t last_offset_segment() const;

  /// Return the next virtual address available
  uint64_t next_virtual_address() const;

  /// Return an iterator over the binary's sections
  it_sections sections() {
    return sections_;
  }

  it_const_sections sections() const {
    return sections_;
  }

  /// Return the binary's entrypoint
  uint64_t entrypoint() const override {
    return header_.entrypoint();
  }

  /// Return binary's segments
  it_segments segments() {
    return segments_;
  }

  it_const_segments segments() const {
    return segments_;
  }

  /// Return binary's dynamic entries
  it_dynamic_entries dynamic_entries() {
    return dynamic_entries_;
  }

  it_const_dynamic_entries dynamic_entries() const {
    return dynamic_entries_;
  }

  /// Add the given dynamic entry and return the new entry
  DynamicEntry& add(const DynamicEntry& entry);

  /// Add the given note and return the created entry
  Note& add(const Note& note);

  /// Remove the given dynamic entry
  void remove(const DynamicEntry& entry);

  /// Remove **all** dynamic entries with the given tag
  void remove(DynamicEntry::TAG tag);

  /// Remove the given section. The ``clear`` parameter
  /// can be used to zeroize the original content beforehand
  ///
  /// @param[in] section The section to remove
  /// @param[in] clear   Whether zeroize the original content
  void remove(const Section& section, bool clear = false);

  /// Remove the given note
  void remove(const Note& note);

  /// Remove **all** notes with the given type
  void remove(Note::TYPE type);

  /// Remove the given segment. If \p clear is set, the original content of the
  /// segment will be filled with zeros before removal.
  void remove(const Segment& seg, bool clear = false);

  /// Remove all segments associated with the given type.
  ///
  /// If \p clear is set, the original content of the segment will be filled
  /// with zeros before removal.
  void remove(Segment::TYPE type, bool clear = false);

  /// Return an iterator over the binary's dynamic symbols
  /// The dynamic symbols are those located in the ``.dynsym`` section
  it_dynamic_symbols dynamic_symbols() {
    return dynamic_symbols_;
  }

  it_const_dynamic_symbols dynamic_symbols() const {
    return dynamic_symbols_;
  }

  /// Return symbols which are exported by the binary
  it_exported_symbols       exported_symbols();
  it_const_exported_symbols exported_symbols() const;

  /// Return symbols which are imported by the binary
  it_imported_symbols       imported_symbols();
  it_const_imported_symbols imported_symbols() const;

  /// Return the debug symbols from the `.symtab` section.
  it_symtab_symbols symtab_symbols() {
    return symtab_symbols_;
  }

  it_const_symtab_symbols symtab_symbols() const {
    return symtab_symbols_;
  }

  /// Return the symbol versions
  it_symbols_version symbols_version() {
    return symbol_version_table_;
  }
  it_const_symbols_version symbols_version() const {
    return symbol_version_table_;
  }

  /// Return symbols version definition
  it_symbols_version_definition symbols_version_definition() {
    return symbol_version_definition_;
  }

  it_const_symbols_version_definition symbols_version_definition() const {
    return symbol_version_definition_;
  }

  /// Return Symbol version requirement
  it_symbols_version_requirement symbols_version_requirement() {
    return symbol_version_requirements_;
  }

  it_const_symbols_version_requirement symbols_version_requirement() const {
    return symbol_version_requirements_;
  }

  /// Return dynamic relocations
  it_dynamic_relocations       dynamic_relocations();
  it_const_dynamic_relocations dynamic_relocations() const;

  /// Add a new *dynamic* relocation.
  ///
  /// We consider a dynamic relocation as a relocation which is not plt-related
  ///
  /// See: add_pltgot_relocation
  Relocation& add_dynamic_relocation(const Relocation& relocation);

  /// Add a .plt.got relocation. This kind of relocation is usually
  /// associated with a PLT stub that aims at resolving the underlying symbol
  ///
  /// See also: add_dynamic_relocation
  Relocation& add_pltgot_relocation(const Relocation& relocation);

  /// Add relocation for object file (.o)
  ///
  /// The first parameter is the section to add while the second parameter
  /// is the LIEF::ELF::Section associated with the relocation.
  ///
  /// If there is an error, this function returns a ``nullptr``. Otherwise, it returns
  /// the relocation added.
  Relocation* add_object_relocation(const Relocation& relocation, const Section& section);

  /// Return `plt.got` relocations
  it_pltgot_relocations       pltgot_relocations();
  it_const_pltgot_relocations pltgot_relocations() const;

  /// Return relocations used in an object file (``*.o``)
  it_object_relocations       object_relocations();
  it_const_object_relocations object_relocations() const;

  /// Return **all** relocations present in the binary
  it_relocations relocations() {
    return relocations_;
  }

  it_const_relocations relocations() const {
    return relocations_;
  }

  /// Return relocation associated with the given address.
  /// It returns a ``nullptr`` if it is not found
  const Relocation* get_relocation(uint64_t address) const;
  Relocation* get_relocation(uint64_t address) {
    return const_cast<Relocation*>(static_cast<const Binary*>(this)->get_relocation(address));
  }

  /// Return relocation associated with the given Symbol
  /// It returns a ``nullptr`` if it is not found
  const Relocation* get_relocation(const Symbol& symbol) const;
  Relocation* get_relocation(const Symbol& symbol) {
    return const_cast<Relocation*>(static_cast<const Binary*>(this)->get_relocation(symbol));
  }

  /// Return relocation associated with the given Symbol name
  /// It returns a ``nullptr`` if it is not found
  const Relocation* get_relocation(const std::string& symbol_name) const;
  Relocation* get_relocation(const std::string& symbol_name) {
    return const_cast<Relocation*>(static_cast<const Binary*>(this)->get_relocation(symbol_name));
  }

  /// ``true`` if GNU hash is used
  ///
  /// @see gnu_hash and use_sysv_hash
  bool use_gnu_hash() const {
    return gnu_hash_ != nullptr && has(DynamicEntry::TAG::GNU_HASH);
  }

  /// Return the GnuHash object in **readonly**
  /// If the ELF binary does not use the GNU hash table, return a nullptr
  const GnuHash* gnu_hash() const {
    return use_gnu_hash() ? gnu_hash_.get() : nullptr;
  }

  /// ``true`` if SYSV hash is used
  ///
  /// @see sysv_hash and use_gnu_hash
  bool use_sysv_hash() const {
    return sysv_hash_ != nullptr && has(DynamicEntry::TAG::HASH);
  }

  /// Return the SysvHash object as a **read-only** object
  /// If the ELF binary does not use the legacy sysv hash table, return a nullptr
  const SysvHash* sysv_hash() const {
    return use_sysv_hash() ? sysv_hash_.get() : nullptr;
  }

  /// Check if a section with the given name exists in the binary
  bool has_section(const std::string& name) const {
    return get_section(name) != nullptr;
  }

  /// Check if a section that handles the given offset exists
  bool has_section_with_offset(uint64_t offset) const;

  /// Check if a section that handles the given virtual address exists
  bool has_section_with_va(uint64_t va) const;

  /// Return Section with the given `name`. If the section can't be
  /// found, it returns a nullptr
  Section* get_section(const std::string& name) {
    return const_cast<Section*>(static_cast<const Binary*>(this)->get_section(name));
  }
  const Section* get_section(const std::string& name) const;

  /// Return the `.text` section. If the section
  /// can't be found, it returns a nullptr
  Section* text_section() {
    return get_section(".text");
  }

  /// Return the `.dynamic` section. If the section
  /// can't be found, it returns a nullptr
  Section* dynamic_section();

  /// Return the hash section. If the section
  /// can't be found, it returns a nullptr
  Section* hash_section();

  /// Return section which holds the symtab symbols. If the section
  /// can't be found, it returns a nullptr
  Section* symtab_symbols_section();

  /// Return program image base. For instance ``0x40000``
  ///
  /// To compute the image base, we look for the PT_PHDR segment header (phdr),
  /// and we return ``phdr->p_vaddr - phdr->p_offset``
  uint64_t imagebase() const override;

  /// Return the size of the mapped binary
  uint64_t virtual_size() const;

  /// Check if the binary uses a loader (also named linker or interpreter)
  /// @see interpreter
  bool has_interpreter() const;

  /// Return the ELF interpreter if any. (e.g. `/lib64/ld-linux-x86-64.so.2`)
  /// If the binary does not have an interpreter, it returns an empty string
  ///
  /// @see has_interpreter
  const std::string& interpreter() const {
    return interpreter_;
  }

  /// Change the interpreter
  void interpreter(const std::string& interpreter) {
    interpreter_ = interpreter;
  }

  /// Return an iterator on both static and dynamic symbols
  it_symbols symbols() {
    return symtab_dyn_symbols();
  }

  it_const_symbols symbols() const {
    return symtab_dyn_symbols();
  }

  /// Export the given symbol and create it if it doesn't exist
  Symbol& export_symbol(const Symbol& symbol);

  /// Export the symbol with the given name and create it if it doesn't exist
  Symbol& export_symbol(const std::string& symbol_name, uint64_t value = 0);

  /// Check if the symbol with the given ``name`` exists in the dynamic symbols table
  bool has_dynamic_symbol(const std::string& name) const {
    return get_dynamic_symbol(name) != nullptr;
  }

  /// Get the dynamic symbol from the given name.
  /// Return a nullptr if it can't be found
  const Symbol* get_dynamic_symbol(const std::string& name) const;

  Symbol* get_dynamic_symbol(const std::string& name) {
    return const_cast<Symbol*>(static_cast<const Binary*>(this)->get_dynamic_symbol(name));
  }

  /// Check if the symbol with the given ``name`` exists in the symtab symbol table
  bool has_symtab_symbol(const std::string& name) const {
    return get_symtab_symbol(name) != nullptr;
  }

  /// Get the symtab symbol from the given name
  /// Return a nullptr if it can't be found
  const Symbol* get_symtab_symbol(const std::string& name) const;

  Symbol* get_symtab_symbol(const std::string& name) {
    return const_cast<Symbol*>(static_cast<const Binary*>(this)->get_symtab_symbol(name));
  }

  /// Return list of the strings used by the ELF binary.
  ///
  /// Basically, this function looks for string in the ``.roadata`` section
  string_list_t strings(size_t min_size = 5) const;

  /// Remove symbols with the given name in both:
  ///   * dynamic symbols
  ///   * symtab symbols
  ///
  /// @see remove_symtab_symbol, remove_dynamic_symbol
  void remove_symbol(const std::string& name);

  /// Remove symtabl symbols with the given name
  void remove_symtab_symbol(const std::string& name);
  void remove_symtab_symbol(Symbol* symbol);

  /// Remove dynamic symbols with the given name
  void remove_dynamic_symbol(const std::string& name);

  /// Remove the given symbol from the dynamic symbols table.
  ///
  /// As a side effect, it will remove any ELF::Relocation
  /// that refers to this symbol and the SymbolVersion (if any)
  /// associated with this symbol
  void remove_dynamic_symbol(Symbol* symbol);

  /// Return the address of the given function name
  result<uint64_t> get_function_address(const std::string& func_name) const override;

  /// Return the address of the given function name
  //
  /// @param[in] func_name    The function's name target
  /// @param[in] demangled    Use the demangled name
  result<uint64_t> get_function_address(const std::string& func_name, bool demangled) const;

  /// Add a new section in the binary
  ///
  /// @param[in] section  The section object to insert
  /// @param[in] loaded   Boolean value to indicate that section's data must be loaded
  ///                     by a PT_LOAD segment
  /// @param[in] pos      Position where to insert the data in the sections table
  ///
  /// @return The section added. The `size` and the `virtual address` might change.
  ///
  /// This function requires a well-formed ELF binary
  Section* add(const Section& section, bool loaded = true,
               SEC_INSERT_POS pos = SEC_INSERT_POS::AUTO);

  Section* extend(const Section& section, uint64_t size);

  /// Add a symtab symbol
  Symbol& add_symtab_symbol(const Symbol& symbol);

  /// Add a dynamic symbol with the associated SymbolVersion
  Symbol& add_dynamic_symbol(const Symbol& symbol, const SymbolVersion* version = nullptr);

  /// Create a symbol for the function at the given address and export it
  Symbol& add_exported_function(uint64_t address, const std::string& name = "");

  /// Add a library as dependency
  DynamicEntryLibrary& add_library(const std::string& library_name);

  /// Remove the given library from the dependencies
  void remove_library(const std::string& library_name);

  /// Get the library object (DynamicEntryLibrary) from the given name
  /// If the library can't be found, it returns a nullptr.
  DynamicEntryLibrary* get_library(const std::string& library_name) {
    return const_cast<DynamicEntryLibrary*>(static_cast<const Binary*>(this)->get_library(library_name));
  }

  /// Get the library object (DynamicEntryLibrary) from the given name
  /// If the library can't be found, it returns a nullptr.
  const DynamicEntryLibrary* get_library(const std::string& library_name) const;

  /// Check if the given library name exists in the current binary
  bool has_library(const std::string& name) const {
    return get_library(name) != nullptr;
  }

  /// Add a new segment in the binary
  ///
  /// The segment is inserted at the end
  ///
  /// @return The segment added. `Virtual address` and `File Offset` might change.
  ///
  /// This function requires a well-formed ELF binary
  Segment* add(const Segment& segment, uint64_t base = 0);

  /// Replace the segment given in 2nd parameter with the segment given in the first one and return the updated segment.
  ///
  /// @warning The ``original_segment`` is no longer valid after this function
  Segment* replace(const Segment& new_segment, const Segment& original_segment, uint64_t base = 0);

  Segment* extend(const Segment& segment, uint64_t size);


  /// Patch the content at virtual address @p address with @p patch_value
  ///
  /// @param[in] address      Address to patch
  /// @param[in] patch_value  Patch to apply
  /// @param[in] addr_type    Specify if the address should be used as an absolute virtual address or an RVA
  void patch_address(uint64_t address, const std::vector<uint8_t>& patch_value,
                     LIEF::Binary::VA_TYPES addr_type = LIEF::Binary::VA_TYPES::AUTO) override;


  /// Patch the address with the given value
  ///
  /// @param[in] address        Address to patch
  /// @param[in] patch_value    Patch to apply
  /// @param[in] size           Size of the value in **bytes** (1, 2, ... 8)
  /// @param[in] addr_type      Specify if the address should be used as an absolute virtual address or an RVA
  void patch_address(uint64_t address, uint64_t patch_value,
                     size_t size = sizeof(uint64_t),
                     LIEF::Binary::VA_TYPES addr_type = LIEF::Binary::VA_TYPES::AUTO) override;

  /// Patch the imported symbol with the ``address``
  ///
  /// @param[in] symbol Imported symbol to patch
  /// @param[in] address New address
  void patch_pltgot(const Symbol& symbol, uint64_t address);


  /// Patch the imported symbol's name with the ``address``
  ///
  /// @param[in] symbol_name Imported symbol's name to patch
  /// @param[in] address New address
  void patch_pltgot(const std::string& symbol_name, uint64_t address);

  /// Strip the binary by removing symtab symbols
  void strip();

  /// Remove a binary's section.
  ///
  /// @param[in] name   The name of the section to remove
  /// @param[in] clear  Whether zeroize the original content
  void remove_section(const std::string& name, bool clear = false) override;

  /// Reconstruct the binary object and write it in `filename`
  ///
  /// This function assumes that the layout of the current ELF binary is correct
  /// (i.e. the binary can run).
  ///
  /// @param filename Path for the written ELF binary
  void write(const std::string& filename) {
    return write(filename, Builder::config_t{});
  }

  /// Reconstruct the binary object with the given config and write it in `filename`
  ///
  /// This function assumes that the layout of the current ELF binary is correct
  /// (i.e. the binary can run).
  ///
  /// @param filename Path for the written ELF binary
  /// @param config   Builder configuration
  void write(const std::string& filename, const Builder::config_t& config);

  /// Reconstruct the binary object and write it in `os` stream
  ///
  /// This function assumes that the layout of the current ELF binary is correct
  /// (i.e. the binary can run).
  ///
  /// @param os Output stream for the written ELF binary
  void write(std::ostream& os) {
    return write(os, Builder::config_t{});
  }

  /// Reconstruct the binary object with the given config and write it in `os` stream
  ///
  /// @param os     Output stream for the written ELF binary
  /// @param config Builder configuration
  void write(std::ostream& os, const Builder::config_t& config);

  /// Reconstruct the binary object and return its content as a byte vector
  std::vector<uint8_t> raw();

  /// Convert a virtual address to a file offset
  result<uint64_t> virtual_address_to_offset(uint64_t virtual_address) const;

  /// Convert the given offset into a virtual address.
  ///
  /// @param[in] offset   The offset to convert.
  /// @param[in] slide    If not 0, it will replace the default base address (if any)
  result<uint64_t> offset_to_virtual_address(uint64_t offset, uint64_t slide = 0) const override;

  /// Check if the binary has been compiled with `-fpie -pie` flags
  ///
  /// To do so we check if there is a `PT_INTERP` segment and if
  /// the binary type is `ET_DYN` (Shared object)
  bool is_pie() const override;

  /// Check if the binary uses the ``NX`` protection (Non executable stack)
  bool has_nx() const override;

  /// Symbol index in the dynamic symbol table or -1 if the symbol
  /// does not exist.
  int64_t dynsym_idx(const std::string& name) const;

  int64_t dynsym_idx(const Symbol& sym) const;

  /// Symbol index from the `.symtab` section or -1 if the symbol is not present
  int64_t symtab_idx(const std::string& name) const;

  int64_t symtab_idx(const Symbol& sym) const;

  /// Return the ELF::Section from the given @p offset. Return a nullptr
  /// if a section can't be found
  ///
  /// If @p skip_nobits is set (which is the case by default), this function won't
  /// consider section for which the type is ``SHT_NOBITS`` (like ``.bss, .tbss, ...``)
  const Section* section_from_offset(uint64_t offset, bool skip_nobits = true) const;
  Section*       section_from_offset(uint64_t offset, bool skip_nobits = true) {
    return const_cast<Section*>(static_cast<const Binary*>(this)->section_from_offset(offset, skip_nobits));
  }

  /// Return the ELF::Section from the given @p address. Return a nullptr
  /// if a section can't be found.
  ///
  /// If @p skip_nobits is set (which is the case by default), this function won't
  /// consider section for which type is ``SHT_NOBITS`` (like ``.bss, .tbss, ...``)
  const Section* section_from_virtual_address(uint64_t address, bool skip_nobits = true) const;
  Section* section_from_virtual_address(uint64_t address, bool skip_nobits = true) {
    return const_cast<Section*>(static_cast<const Binary*>(this)->section_from_virtual_address(address, skip_nobits));
  }

  /// Return the ELF::Segment from the given @p address. Return a nullptr
  /// if a segment can't be found.
  const Segment* segment_from_virtual_address(uint64_t address) const;
  Segment* segment_from_virtual_address(uint64_t address) {
    return const_cast<Segment*>(static_cast<const Binary*>(this)->segment_from_virtual_address(address));
  }


  const Segment* segment_from_virtual_address(Segment::TYPE type, uint64_t address) const;
  Segment* segment_from_virtual_address(Segment::TYPE type, uint64_t address) {
    return const_cast<Segment*>(static_cast<const Binary*>(this)->segment_from_virtual_address(type, address));
  }

  /// Return the ELF::Segment from the @p offset. Return a nullptr
  /// if a segment can't be found.
  const Segment* segment_from_offset(uint64_t offset) const;
  Segment* segment_from_offset(uint64_t offset) {
    return const_cast<Segment*>(static_cast<const Binary*>(this)->segment_from_offset(offset));
  }

  /// Return the **first** ELF::DynamicEntry associated with the given tag
  /// If the tag can't be found, it returns a nullptr
  const DynamicEntry* get(DynamicEntry::TAG tag) const;
  DynamicEntry*       get(DynamicEntry::TAG tag) {
    return const_cast<DynamicEntry*>(static_cast<const Binary*>(this)->get(tag));
  }

  /// Return the **first** ELF::Segment associated with the given type.
  /// If a segment can't be found, it returns a nullptr.
  const Segment* get(Segment::TYPE type) const;
  Segment* get(Segment::TYPE type) {
    return const_cast<Segment*>(static_cast<const Binary*>(this)->get(type));
  }

  /// Return the **first** ELF::Note associated with the given type
  /// If a note can't be found, it returns a nullptr.
  const Note* get(Note::TYPE type) const;
  Note* get(Note::TYPE type) {
    return const_cast<Note*>(static_cast<const Binary*>(this)->get(type));
  }

  /// Return the **first** ELF::Section associated with the given type
  /// If a section can't be found, it returns a nullptr.
  const Section* get(Section::TYPE type) const;
  Section* get(Section::TYPE type) {
    return const_cast<Section*>(static_cast<const Binary*>(this)->get(type));
  }

  /// Check if an ELF::DynamicEntry associated with the given tag exists.
  bool has(DynamicEntry::TAG tag) const {
    return get(tag) != nullptr;
  }

  /// Check if ELF::Segment associated with the given type exists.
  bool has(Segment::TYPE type) const {
    return get(type) != nullptr;
  }

  /// Check if a ELF::Note associated with the given type exists.
  bool has(Note::TYPE type) const {
    return get(type) != nullptr;
  }

  /// Check if a ELF::Section associated with the given type exists.
  bool has(Section::TYPE type) const {
    return get(type) != nullptr;
  }

  /// Return the content located at virtual address
  span<const uint8_t> get_content_from_virtual_address(uint64_t virtual_address, uint64_t size,
                                     Binary::VA_TYPES addr_type = Binary::VA_TYPES::AUTO) const override;

  /// Method associated with the visitor pattern.
  void accept(LIEF::Visitor& visitor) const override;

  /// Apply the given permutation on the dynamic symbols table
  void permute_dynamic_symbols(const std::vector<size_t>& permutation);

  /// List of binary constructors (typically, the functions located in the ``.init_array``)
  LIEF::Binary::functions_t ctor_functions() const override;

  /// List of the binary destructors (typically, the functions located in the ``.fini_array``)
  LIEF::Binary::functions_t dtor_functions() const;

  /// List of the functions found the in the binary.
  LIEF::Binary::functions_t functions() const;

  /// ``true`` if the binary embeds notes
  bool has_notes() const;

  /// Return an iterator over the ELF's LIEF::ELF::Note
  ///
  /// @see has_note
  it_const_notes notes() const {
    return notes_;
  }

  it_notes notes() {
    return notes_;
  }

  /// Return the last offset used by the ELF binary according to both: the sections table
  /// and the segments table
  uint64_t eof_offset() const;

  /// True if data are present at the end of the binary
  bool has_overlay() const {
    return !overlay_.empty();
  }

  /// Overlay data (if any)
  span<const uint8_t> overlay() const {
    return overlay_;
  }

  /// Function to set the overlay
  void overlay(std::vector<uint8_t> overlay) {
    overlay_ = std::move(overlay);
  }

  /// Force relocating the segments table in a specific way.
  ///
  /// This function can be used to enforce a specific relocation of the
  /// segments table.
  ///
  /// @param[in] type The relocation type to apply
  /// @return The offset of the new segments table or 0 if it fails with
  ///         the given method.
  uint64_t relocate_phdr_table(PHDR_RELOC type);

  /// Return the array defined by the given tag (e.g.
  /// DynamicEntry::TAG::INIT_ARRAY) with relocations applied (if any)
  std::vector<uint64_t> get_relocated_dynamic_array(DynamicEntry::TAG tag) const;

  /// True if the current ELF is targeting Android
  bool is_targeting_android() const;

  /// Find the index of the section given in the first parameter
  result<size_t> get_section_idx(const Section& section) const {
    auto it = std::find_if(sections_.begin(), sections_.end(),
      [&section] (const std::unique_ptr<Section>& S) {
        return S.get() == &section;
      }
    );
    if (it == sections_.end()) {
      return make_error_code(lief_errors::not_found);
    }
    return std::distance(sections_.begin(), it);
  }

  /// Find the index of the section with the name given in the first parameter
  result<size_t> get_section_idx(const std::string& name) const {
    auto it = std::find_if(sections_.begin(), sections_.end(),
      [name] (const std::unique_ptr<Section>& S) {
        return S->name() == name;
      }
    );
    if (it == sections_.end()) {
      return make_error_code(lief_errors::not_found);
    }
    return std::distance(sections_.begin(), it);
  }

  /// Try to find the SymbolVersionRequirement associated with the given library
  /// name (e.g. `libc.so.6`)
  const SymbolVersionRequirement*
    find_version_requirement(const std::string& libname) const;

  SymbolVersionRequirement* find_version_requirement(const std::string& name) {
      return const_cast<SymbolVersionRequirement*>(static_cast<const Binary*>(this)->find_version_requirement(name));
  }

  /// Deletes all required symbol versions linked to the specified library name.
  /// The function returns true if the operation succeed, false otherwise.
  ///
  /// \warning To maintain consistency, this function also removes versions
  ///          associated with dynamic symbols that are linked to the specified
  ///          library name.
  bool remove_version_requirement(const std::string& libname);

  uint8_t ptr_size() const {
    switch (type()) {
      case Header::CLASS::ELF32:
        return sizeof(uint32_t);
      case Header::CLASS::ELF64:
        return sizeof(uint64_t);
      default:
        return 0;
    }
    return 0;
  }

  uint64_t page_size() const override;

  static bool classof(const LIEF::Binary* bin) {
    return bin->format() == Binary::FORMATS::ELF ||
           bin->format() == Binary::FORMATS::OAT;
  }

  size_t hash(const std::string& name);

  ~Binary() override;

  std::ostream& print(std::ostream& os) const override;

  Binary& operator+=(const DynamicEntry& entry) {
    add(entry);
    return *this;
  }
  Binary& operator+=(const Section& section) {
    add(section);
    return *this;
  }

  Binary& operator+=(const Segment& segment) {
    add(segment);
    return *this;
  }

  Binary& operator+=(const Note& note) {
    add(note);
    return *this;
  }

  Binary& operator-=(const DynamicEntry& entry) {
    remove(entry);
    return *this;
  }

  Binary& operator-=(DynamicEntry::TAG tag) {
    remove(tag);
    return *this;
  }

  Binary& operator-=(const Note& note) {
    remove(note);
    return *this;
  }

  Binary& operator-=(Note::TYPE type) {
    remove(type);
    return *this;
  }

  Segment* operator[](Segment::TYPE type) {
    return get(type);
  }

  const Segment* operator[](Segment::TYPE type) const {
    return get(type);
  }

  DynamicEntry* operator[](DynamicEntry::TAG tag) {
    return get(tag);
  }

  const DynamicEntry* operator[](DynamicEntry::TAG tag) const {
    return get(tag);
  }

  Note* operator[](Note::TYPE type) {
    return get(type);
  }

  const Note* operator[](Note::TYPE type) const {
    return get(type);
  }

  Section* operator[](Section::TYPE type) {
    return get(type);
  }

  const Section* operator[](Section::TYPE type) const {
    return get(type);
  }

  bool should_swap() const {
    return should_swap_;
  }

  protected:
  struct phdr_relocation_info_t {
    uint64_t new_offset = 0;
    size_t nb_segments = 0;
    void clear() {
      new_offset = 0;
      nb_segments = 0;
    }
  };
  LIEF_LOCAL Binary();

  /// Return an abstraction of binary's section: LIEF::Section
  LIEF_LOCAL LIEF::Binary::sections_t get_abstract_sections() override;

  LIEF_LOCAL LIEF::Header get_abstract_header() const override {
    return LIEF::Header::from(*this);
  }

  LIEF_LOCAL LIEF::Binary::functions_t get_abstract_exported_functions() const override;
  LIEF_LOCAL LIEF::Binary::functions_t get_abstract_imported_functions() const override;
  LIEF_LOCAL std::vector<std::string> get_abstract_imported_libraries() const override;
  LIEF_LOCAL LIEF::Binary::symbols_t     get_abstract_symbols() override;
  LIEF_LOCAL LIEF::Binary::relocations_t get_abstract_relocations() override;

  template<ELF::ARCH ARCH>
  LIEF_LOCAL void patch_relocations(uint64_t from, uint64_t shift);

  template<class T>
  LIEF_LOCAL void patch_addend(Relocation& relocatio, uint64_t from, uint64_t shift);

  LIEF_LOCAL void shift_sections(uint64_t from, uint64_t shift);
  LIEF_LOCAL void shift_segments(uint64_t from, uint64_t shift);
  LIEF_LOCAL void shift_dynamic_entries(uint64_t from, uint64_t shift);
  LIEF_LOCAL void shift_symbols(uint64_t from, uint64_t shift);
  LIEF_LOCAL void shift_relocations(uint64_t from, uint64_t shift);

  template<class ELF_T>
  LIEF_LOCAL void fix_got_entries(uint64_t from, uint64_t shift);

  LIEF_LOCAL LIEF::Binary::functions_t eh_frame_functions() const;
  LIEF_LOCAL LIEF::Binary::functions_t armexid_functions() const;

  template<Header::FILE_TYPE OBJECT_TYPE, bool note = false>
  LIEF_LOCAL Segment* add_segment(const Segment& segment, uint64_t base);

  LIEF_LOCAL uint64_t relocate_phdr_table_auto();
  LIEF_LOCAL uint64_t relocate_phdr_table_pie();
  LIEF_LOCAL uint64_t relocate_phdr_table_v1();
  LIEF_LOCAL uint64_t relocate_phdr_table_v2();
  LIEF_LOCAL uint64_t relocate_phdr_table_v3();

  template<Segment::TYPE PT>
  LIEF_LOCAL Segment* extend_segment(const Segment& segment, uint64_t size);

  template<bool LOADED>
  LIEF_LOCAL Section* add_section(const Section& section, SEC_INSERT_POS pos);

  std::vector<Symbol*> symtab_dyn_symbols() const;

  LIEF_LOCAL std::string shstrtab_name() const;
  LIEF_LOCAL Section* add_frame_section(const Section& sec);
  LIEF_LOCAL Section* add_section(std::unique_ptr<Section> sec);

  LIEF_LOCAL LIEF::Binary::functions_t tor_functions(DynamicEntry::TAG tag) const;

  Header::CLASS type_ = Header::CLASS::NONE;
  Header header_;
  sections_t sections_;
  segments_t segments_;
  dynamic_entries_t dynamic_entries_;
  symbols_t dynamic_symbols_;
  symbols_t symtab_symbols_;
  relocations_t relocations_;
  symbols_version_t symbol_version_table_;
  symbols_version_requirement_t symbol_version_requirements_;
  symbols_version_definition_t  symbol_version_definition_;
  notes_t notes_;
  std::unique_ptr<GnuHash> gnu_hash_;
  std::unique_ptr<SysvHash> sysv_hash_;
  std::unique_ptr<DataHandler::Handler> datahandler_;
  phdr_relocation_info_t phdr_reloc_info_;

  std::string interpreter_;
  std::vector<uint8_t> overlay_;
  std::unique_ptr<sizing_info_t> sizing_info_;
  uint64_t pagesize_ = 0;
  bool should_swap_ = false;
};

}
}
#endif
