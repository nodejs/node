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
#ifndef LIEF_MACHO_BINARY_PARSER_H
#define LIEF_MACHO_BINARY_PARSER_H
#include <memory>
#include <string>
#include <vector>
#include <limits>
#include <set>
#include <map>
#include <unordered_map>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

#include "LIEF/Abstract/Parser.hpp"

#include "LIEF/MachO/enums.hpp"
#include "LIEF/MachO/DyldChainedFormat.hpp"
#include "LIEF/MachO/ParserConfig.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"

namespace LIEF {
class BinaryStream;
class SpanStream;

namespace MachO {
class AtomInfo;
class ChainedBindingInfo;
class CodeSignature;
class CodeSignatureDir;
class DataInCode;
class DyldChainedFixups;
class DylibCommand;
class DynamicSymbolCommand;
class ExportInfo;
class FunctionStarts;
class FunctionVariants;
class FunctionVariantFixups;
class LinkerOptHint;
class Parser;
class Section;
class SegmentCommand;
class SegmentSplitInfo;
class Symbol;
class SymbolCommand;
class TwoLevelHints;
struct ParserConfig;


namespace details {
struct dyld_chained_starts_in_segment;
struct dyld_chained_fixups_header;
union dyld_chained_ptr_arm64e;
union dyld_chained_ptr_generic64;
union dyld_chained_ptr_generic32;
union dyld_chained_ptr_arm64e_segmented;
}

/// Class used to parse a **single** binary (i.e. non-FAT)
///
/// @warning This class should not be directly used.
///
/// @see MachO::Parser
class LIEF_API BinaryParser : public LIEF::Parser {

  friend class MachO::Parser;

  /// Maximum number of relocations
  constexpr static size_t MAX_RELOCATIONS = (std::numeric_limits<uint16_t>::max)();

  /// Maximum number of MachO LoadCommand
  constexpr static size_t MAX_COMMANDS = (std::numeric_limits<uint16_t>::max)();

  public:
  static std::unique_ptr<Binary> parse(const std::string& file);
  static std::unique_ptr<Binary> parse(const std::string& file, const ParserConfig& conf);
  static std::unique_ptr<Binary> parse(const std::vector<uint8_t>& data,
                                       const ParserConfig& conf = ParserConfig::deep());

  static std::unique_ptr<Binary> parse(const std::vector<uint8_t>& data, uint64_t fat_offset,
                                       const ParserConfig& conf = ParserConfig::deep());

  static std::unique_ptr<Binary> parse(std::unique_ptr<BinaryStream> stream, uint64_t fat_offset,
                                       const ParserConfig& conf);

  BinaryParser& operator=(const BinaryParser& copy) = delete;
  BinaryParser(const BinaryParser& copy) = delete;

  ~BinaryParser() override;

  private:
  using exports_list_t = std::vector<std::unique_ptr<ExportInfo>>;
  LIEF_LOCAL BinaryParser();

  LIEF_LOCAL ok_error_t init_and_parse();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_header();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_load_commands();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_relocations(Section& section);

  // Dyld info parser
  // ================

  // Rebase
  // ------
  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_dyldinfo_rebases();

  // Bindings
  // --------
  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_dyldinfo_binds();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_dyldinfo_generic_bind();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_dyldinfo_weak_bind();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_dyldinfo_lazy_bind();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t infer_indirect_bindings();

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_symtab(SymbolCommand& cmd, SpanStream& nlist_s,
                                     SpanStream& string_s);

  LIEF_LOCAL ok_error_t parse_indirect_symbols(
    DynamicSymbolCommand& cmd, std::vector<Symbol*>& symtab,
    BinaryStream& indirect_stream);


  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_data_in_code(DataInCode& cmd, BinaryStream& stream);

  using it_opaque_segments = void*; // To avoid including Binary.hpp. It must contains it_opaque_segments

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t do_bind(DyldBindingInfo::CLASS cls, uint8_t type, uint8_t segment_idx,
                     uint64_t segment_offset, const std::string& symbol_name,
                     int32_t ord, int64_t addend, bool is_weak,
                     bool is_non_weak_definition, it_opaque_segments segments_ptr, uint64_t offset = 0);


  template<class MACHO_T>
  LIEF_LOCAL ok_error_t do_rebase(
    uint8_t type, uint8_t segment_idx, uint64_t segment_offset,
    it_opaque_segments segments);

  /*
   * This set of functions are related to the parsing of LC_DYLD_CHAINED_FIXUPS
   */
  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_chained_payload(SpanStream& stream);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_chained_import(
    const details::dyld_chained_fixups_header& header, SpanStream& stream,
    SpanStream& symbol_pool);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_chained_fixup(
    const details::dyld_chained_fixups_header& header, SpanStream& stream);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t parse_fixup_seg(
    SpanStream& stream, uint32_t seg_info_offset, uint64_t offset,
    uint32_t seg_idx);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t do_fixup(
    DYLD_CHAINED_FORMAT fmt, int32_t ord, const std::string& symbol_name,
    int64_t addend, bool is_weak);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t process_fixup(
    SegmentCommand& segment, uint64_t chain_address, uint64_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info);

  template<class MACHO_T>
  LIEF_LOCAL result<uint64_t> next_chain(
    uint64_t& chain_address, uint64_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t walk_chain(
    SegmentCommand& segment, uint64_t chain_address, uint64_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info);

  LIEF_LOCAL ok_error_t do_chained_fixup(
    SegmentCommand& segment, uint64_t chain_address, uint32_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info,
    const details::dyld_chained_ptr_arm64e& fixup);

  LIEF_LOCAL ok_error_t do_chained_fixup(
    SegmentCommand& segment, uint64_t chain_address, uint32_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info,
    const details::dyld_chained_ptr_generic64& fixup);

  LIEF_LOCAL ok_error_t do_chained_fixup(
    SegmentCommand& segment, uint64_t chain_address, uint32_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info,
    const details::dyld_chained_ptr_generic32 & fixup);

  LIEF_LOCAL ok_error_t do_chained_fixup(
    SegmentCommand& segment, uint64_t chain_address, uint32_t chain_offset,
    const details::dyld_chained_starts_in_segment& seg_info,
    const details::dyld_chained_ptr_arm64e_segmented& fixup);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(SymbolCommand& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(FunctionStarts& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(DataInCode& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(SegmentSplitInfo& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(DynamicSymbolCommand& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(LinkerOptHint& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(AtomInfo& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(TwoLevelHints& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(CodeSignature& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(CodeSignatureDir& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(FunctionVariants& cmd);

  template<class MACHO_T>
  LIEF_LOCAL ok_error_t post_process(FunctionVariantFixups& cmd);

  LIEF_LOCAL ok_error_t parse_overlay();

  // Exports
  // -------
  LIEF_LOCAL ok_error_t parse_dyldinfo_export();
  LIEF_LOCAL ok_error_t parse_dyld_exports();

  LIEF_LOCAL ok_error_t parse_export_trie(
    exports_list_t& exports, BinaryStream& stream,
    uint64_t start, const std::string& prefix, bool* invalid_names);

  LIEF_LOCAL void copy_from(ChainedBindingInfo& to, ChainedBindingInfo& from);

  std::unique_ptr<BinaryStream>  stream_;
  std::unique_ptr<Binary>        binary_;
  MACHO_TYPES                    type_ = MACHO_TYPES::MAGIC_64;
  bool                           is64_ = true;
  ParserConfig                   config_;
  std::set<uint64_t>             visited_;
  std::unordered_map<std::string, Symbol*> memoized_symbols_;
  std::map<uint64_t, Symbol*>    memoized_symbols_by_address_;

  std::vector<DylibCommand*> binding_libs_;
  std::set<uint64_t> dyld_reloc_addrs_;

  // Cache of DyldChainedFixups
  DyldChainedFixups* chained_fixups_ = nullptr;
};


} // namespace MachO
} // namespace LIEF
#endif
