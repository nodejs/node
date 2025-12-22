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
#ifndef LIEF_ELF_PARSER_H
#define LIEF_ELF_PARSER_H
#include <unordered_map>

#include "LIEF/visibility.h"
#include "LIEF/utils.hpp"

#include "LIEF/Abstract/Parser.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/ELF/enums.hpp"

#include "LIEF/ELF/ParserConfig.hpp"

namespace LIEF {
class BinaryStream;

namespace OAT {
class Parser;
}
namespace ELF {

class Section;
class Binary;
class Segment;
class Symbol;
class Note;
class Relocation;

/// Class which parses and transforms an ELF file into a ELF::Binary object
class LIEF_API Parser : public LIEF::Parser {
  friend class OAT::Parser;
  public:
  static constexpr uint32_t NB_MAX_SYMBOLS         = 1000000;
  static constexpr uint32_t DELTA_NB_SYMBOLS       = 3000;
  static constexpr uint32_t NB_MAX_BUCKETS         = NB_MAX_SYMBOLS;
  static constexpr uint32_t NB_MAX_CHAINS          = 1000000;
  static constexpr uint32_t NB_MAX_SEGMENTS        = 10000;
  static constexpr uint32_t NB_MAX_RELOCATIONS     = 3000000;
  static constexpr uint32_t NB_MAX_DYNAMIC_ENTRIES = 1000;
  static constexpr uint32_t MAX_SEGMENT_SIZE       = 3_GB;

  enum ELF_TYPE {
    ELF_UNKNOWN,
    ELF32, ELF64
  };

  /// Parse an ELF file and return a LIEF::ELF::Binary object
  ///
  /// For weird binaries (e.g. sectionless) you can choose which method to use for counting dynamic symbols
  ///
  /// @param[in] file Path to the ELF binary
  /// @param[in] conf Optional configuration for the parser
  ///
  /// @return LIEF::ELF::Binary as a `unique_ptr`
  static std::unique_ptr<Binary> parse(const std::string& file,
                                       const ParserConfig& conf = ParserConfig::all());

  /// Parse the given raw data as an ELF binary and return a LIEF::ELF::Binary object
  ///
  /// For weird binaries (e.g. sectionless) you can choose which method use to count dynamic symbols
  ///
  /// @param[in] data Raw ELF as a std::vector of uint8_t
  /// @param[in] conf Optional configuration for the parser
  ///
  /// @return LIEF::ELF::Binary
  static std::unique_ptr<Binary> parse(const std::vector<uint8_t>& data,
                                       const ParserConfig& conf = ParserConfig::all());

  /// Parse the ELF binary from the given stream and return a LIEF::ELF::Binary object
  ///
  /// For weird binaries (e.g. sectionless) you can choose which method use to count dynamic symbols
  ///
  /// @param[in] stream  The stream which wraps the ELF binary
  /// @param[in] conf    Optional configuration for the parser
  ///
  /// @return LIEF::ELF::Binary
  static std::unique_ptr<Binary> parse(std::unique_ptr<BinaryStream> stream,
                                       const ParserConfig& conf = ParserConfig::all());

  Parser& operator=(const Parser&) = delete;
  Parser(const Parser&)            = delete;

  ~Parser() override;

  protected:
  LIEF_LOCAL Parser();
  LIEF_LOCAL Parser(std::unique_ptr<BinaryStream> stream, ParserConfig config);
  LIEF_LOCAL Parser(const std::string& file, ParserConfig config);
  LIEF_LOCAL Parser(const std::vector<uint8_t>& data, ParserConfig config);

  LIEF_LOCAL ok_error_t init();

  LIEF_LOCAL bool should_swap() const;

  // map, dynamic_symbol.version <----> symbol_version
  // symbol_version comes from symbol_version table
  LIEF_LOCAL void link_symbol_version();

  LIEF_LOCAL ok_error_t link_symbol_section(Symbol& sym);

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_binary();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_header();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_sections();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_segments();

  LIEF_LOCAL uint64_t get_dynamic_string_table(BinaryStream* stream = nullptr) const;

  LIEF_LOCAL result<uint64_t> get_dynamic_string_table_from_segments(BinaryStream* stream = nullptr) const;

  LIEF_LOCAL uint64_t get_dynamic_string_table_from_sections() const;

  /// Return the number of dynamic symbols using the given method
  template<typename ELF_T>
  LIEF_LOCAL result<uint32_t>
    get_numberof_dynamic_symbols(ParserConfig::DYNSYM_COUNT mtd) const;

  /// Count based on hash table (reliable)
  template<typename ELF_T>
  LIEF_LOCAL result<uint32_t> nb_dynsym_hash() const;

  /// Count based on SYSV hash table
  template<typename ELF_T>
  LIEF_LOCAL result<uint32_t> nb_dynsym_sysv_hash() const;

  /// Count based on GNU hash table
  template<typename ELF_T>
  LIEF_LOCAL result<uint32_t> nb_dynsym_gnu_hash() const;

  /// Count based on sections (not very reliable)
  template<typename ELF_T>
  LIEF_LOCAL result<uint32_t> nb_dynsym_section() const;

  /// Count based on PLT/GOT relocations (very reliable but not accurate)
  template<typename ELF_T>
  LIEF_LOCAL result<uint32_t> nb_dynsym_relocations() const;

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_dynamic_entries(BinaryStream& stream);

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_dynamic_symbols(uint64_t offset);

  /// Parse symtab Symbol
  ///
  /// Parser find Symbols offset by using the file offset attribute of the
  /// ELF_SECTION_TYPES::SHT_SYMTAB Section.
  ///
  /// The number of symbols is taken from the `information` attribute in the section header.
  ///
  /// The section containing symbols name is found with the `link` attribute.
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t
    parse_symtab_symbols(uint64_t offset, uint32_t nb_symbols,
                         const Section& string_section);

  /// Parse Dynamic relocations
  ///
  /// It uses DT_REL/DT_RELA dynamic entries to parse it
  template<typename ELF_T, typename REL_T>
  LIEF_LOCAL ok_error_t
    parse_dynamic_relocations(uint64_t relocations_offset, uint64_t size);

  /// Parse `.plt.got`/`got` relocations
  ///
  /// For:
  /// * ELF32 it uses **DT_JMPREL** and **DT_PLTRELSZ**
  /// * ELF64 it uses **DT_PLTREL** and **DT_PLTRELSZ**
  template<typename ELF_T, typename REL_T>
  LIEF_LOCAL ok_error_t parse_pltgot_relocations(uint64_t offset, uint64_t size);


  /// Parse *relative* relocations
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_relative_relocations(uint64_t offset, uint64_t size);

  /// Parse Android packed relocations
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_packed_relocations(uint64_t offset, uint64_t size);

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t process_dynamic_table();

  /// Parse relocations using LIEF::ELF::Section.
  /// Section relocations are usually found in object files
  template<typename ELF_T, typename REL_T>
  LIEF_LOCAL ok_error_t
    parse_section_relocations(const Section& section);

  /// Parse SymbolVersionRequirement
  ///
  /// We use the virtual address stored in the
  /// DynamicEntry::TAG::VERNEED entry to get the offset.
  /// and DynamicEntry::TAG::VERNEEDNUM to get the number of entries
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t
    parse_symbol_version_requirement(uint64_t offset, uint32_t nb_entries);


  /// Parse SymbolVersionDefinition.
  ///
  /// We use the virtual address stored in
  /// the DynamicEntry::TAG::VERDEF DT_VERDEF entry to get the offset.
  /// DynamicEntry::TAG::VERDEFNUM gives the number of entries
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t
    parse_symbol_version_definition(uint64_t offset, uint32_t nb_entries);


  /// Parse @link SymbolVersion Symbol version @endlink.
  ///
  /// We use the virtual address stored in the
  /// DynamicEntry::TAG::VERSYM entry to parse it.
  ///
  /// @see http://dev.gentoo.org/~solar/elf/symbol-versioning
  LIEF_LOCAL ok_error_t parse_symbol_version(uint64_t symbol_version_offset);

  /// Parse Symbols's GNU hash
  ///
  /// @see https://blogs.oracle.com/ali/entry/gnu_hash_elf_sections
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t parse_symbol_gnu_hash(uint64_t offset);

  /// Parse Note (.gnu.note)
  LIEF_LOCAL ok_error_t parse_notes(uint64_t offset, uint64_t size);

  LIEF_LOCAL std::unique_ptr<Note>
    get_note(uint32_t type, std::string name, std::vector<uint8_t> desc_bytes);

  /// Parse Symbols's SYSV hash
  LIEF_LOCAL ok_error_t parse_symbol_sysv_hash(uint64_t offset);

  LIEF_LOCAL ok_error_t parse_overlay();

  template<typename ELF_T, typename REL_T>
  LIEF_LOCAL uint32_t max_relocation_index(uint64_t relocations_offset, uint64_t size) const;

  /// Check if the given Section is wrapped by the given segment
  LIEF_LOCAL static bool check_section_in_segment(const Section& section, const Segment& segment);

  LIEF_LOCAL bool bind_symbol(Relocation& R);
  LIEF_LOCAL Relocation& insert_relocation(std::unique_ptr<Relocation> R);

  template<class ELF_T>
  LIEF_LOCAL ok_error_t parse_dyn_table(Segment& pt_dyn);

  std::unique_ptr<BinaryStream> stream_;
  std::unique_ptr<Binary> binary_;
  ParserConfig config_;
  /*
   * parse_sections() may skip some sections so that
   * binary_->sections_ is not contiguous based on the index of the sections.
   *
   * On the other hand, we need these indexes to bind symbols that
   * reference sections. That's why we have this unordered_map.
   */
  std::unordered_map<size_t, Section*> sections_idx_;
};

} // namespace ELF
} // namespace LIEF
#endif
