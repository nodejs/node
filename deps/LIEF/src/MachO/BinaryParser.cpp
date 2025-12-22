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
#include <memory>

#include "logging.hpp"
#include "BinaryParser.tcc"

#include "LIEF/BinaryStream/VectorStream.hpp"

#include "LIEF/MachO/BinaryParser.hpp"
#include "LIEF/MachO/utils.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/ExportInfo.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"

#include "internal_utils.hpp"

namespace LIEF {
namespace MachO {

BinaryParser::BinaryParser() = default;
BinaryParser::~BinaryParser() = default;


std::unique_ptr<Binary> BinaryParser::parse(const std::string& file) {
  return parse(file, ParserConfig::deep());
}

std::unique_ptr<Binary> BinaryParser::parse(const std::string& file, const ParserConfig& conf) {
  if (!is_macho(file)) {
    LIEF_DEBUG("{} is not a Mach-O file", file);
    return nullptr;
  }

  if (!is_fat(file)) {
    LIEF_ERR("{} is a Fat Mach-O file. Please use MachO::Parser::parse(...)", file);
    return nullptr;
  }

  auto stream = VectorStream::from_file(file);
  if (!stream) {
    LIEF_ERR("Error while creating the binary stream");
    return nullptr;
  }

  BinaryParser parser;
  parser.config_ = conf;
  parser.stream_ = std::make_unique<VectorStream>(std::move(*stream));
  parser.binary_ = std::unique_ptr<Binary>(new Binary{});
  parser.binary_->fat_offset_ = 0;

  if(!parser.init_and_parse()) {
    LIEF_WARN("Parsing with error. The binary might be in an inconsistent state");
  }

  return std::move(parser.binary_);
}

std::unique_ptr<Binary> BinaryParser::parse(const std::vector<uint8_t>& data, const ParserConfig& conf) {
  return parse(data, 0, conf);
}

std::unique_ptr<Binary> BinaryParser::parse(const std::vector<uint8_t>& data, uint64_t fat_offset,
                                            const ParserConfig& conf)
{
  if (!is_macho(data)) {
    return nullptr;
  }

  // TODO(romain): To implement
  // if (!is_fat(data)) {
  //   LIEF_ERR("{} is a Fat Mach-O file. Please use MachO::Parser::parse(...)");
  //   return nullptr;
  // }

  BinaryParser parser;
  parser.config_ = conf;
  parser.stream_ = std::make_unique<VectorStream>(data);
  parser.binary_ = std::unique_ptr<Binary>(new Binary{});
  parser.binary_->fat_offset_ = fat_offset;

  if(!parser.init_and_parse()) {
    LIEF_WARN("Parsing with error. The binary might be in an inconsistent state");
  }

  return std::move(parser.binary_);
}


std::unique_ptr<Binary> BinaryParser::parse(std::unique_ptr<BinaryStream> stream, uint64_t fat_offset,
                                            const ParserConfig& conf)
{
  BinaryParser parser;
  parser.config_ = conf;
  parser.stream_ = std::move(stream);
  parser.binary_ = std::unique_ptr<Binary>(new Binary{});
  parser.binary_->fat_offset_ = fat_offset;

  if(!parser.init_and_parse()) {
    LIEF_WARN("Parsing with error. The binary might be in an inconsistent state");
  }

  return std::move(parser.binary_);
}


ok_error_t BinaryParser::init_and_parse() {
  LIEF_DEBUG("Parsing MachO");
  if (!stream_->can_read<uint32_t>()) {
    return make_error_code(lief_errors::read_error);
  }
  const auto type = static_cast<MACHO_TYPES>(*stream_->peek<uint32_t>());

  is64_ = type == MACHO_TYPES::MAGIC_64 ||
          type == MACHO_TYPES::CIGAM_64 ||
          type == MACHO_TYPES::NEURAL_MODEL;

  binary_->is64_ = is64_;
  type_          = type;
  binary_->original_size_ = stream_->size();

  bool should_swap = type == MACHO_TYPES::CIGAM_64 ||
                     type == MACHO_TYPES::CIGAM;

  stream_->set_endian_swap(should_swap);

  return is64_ ? parse<details::MachO64>() :
                 parse<details::MachO32>();
}


ok_error_t BinaryParser::parse_export_trie(exports_list_t& exports,
                                           BinaryStream& stream,
                                           uint64_t start,
                                           const std::string& prefix,
                                           bool* invalid_names)
{
  if (!stream) {
    return make_error_code(lief_errors::read_error);
  }

  const auto terminal_size = stream.read<uint8_t>();
  if (!terminal_size) {
    LIEF_ERR("Can't read terminal size");
    return make_error_code(lief_errors::read_error);
  }
  uint64_t children_offset = stream.pos() + *terminal_size;

  if (*terminal_size != 0) {
    uint64_t offset = stream.pos();

    auto res_flags = stream.read_uleb128();
    if (!res_flags) {
      return make_error_code(lief_errors::read_error);
    }
    uint64_t flags = *res_flags;
    //uint64_t address = stream_->read_uleb128();

    const std::string& symbol_name = prefix;
    auto export_info = std::make_unique<ExportInfo>(0, flags, offset);
    Symbol* symbol = nullptr;
    auto search = memoized_symbols_.find(symbol_name);
    if (search != memoized_symbols_.end()) {
      symbol = search->second;
    } else {
      symbol = binary_->get_symbol(symbol_name);
    }
    if (symbol != nullptr) {
      export_info->symbol_ = symbol;
      symbol->export_info_ = export_info.get();
    } else { // Register it into the symbol table
      auto symbol = std::make_unique<Symbol>();

      symbol->origin_            = Symbol::ORIGIN::DYLD_EXPORT;
      symbol->value_             = 0;
      symbol->type_              = 0;
      symbol->numberof_sections_ = 0;
      symbol->description_       = 0;
      symbol->name(symbol_name);

      // Weak bind of the pointer
      symbol->export_info_       = export_info.get();
      export_info->symbol_       = symbol.get();
      binary_->symbols_.push_back(std::move(symbol));
    }

    // REEXPORT
    // ========
    if (export_info->has(ExportInfo::FLAGS::REEXPORT)) {
      auto res_ordinal = stream.read_uleb128();
      if (!res_ordinal) {
        LIEF_ERR("Can't read uleb128 to determine the ordinal value");
        return make_error_code(lief_errors::parsing_error);
      }
      const uint64_t ordinal = *res_ordinal;
      export_info->other_ = ordinal;

      auto res_imported_name = stream.peek_string();
      if (!res_imported_name) {
        LIEF_ERR("Can't read imported_name");
        return make_error_code(lief_errors::parsing_error);
      }

      std::string imported_name = std::move(*res_imported_name);

      if (imported_name.empty() && export_info->has_symbol()) {
        imported_name = export_info->symbol()->name();
      }

      Symbol* symbol = nullptr;
      auto search = memoized_symbols_.find(imported_name);
      if (search != memoized_symbols_.end()) {
        symbol = search->second;
      } else {
        symbol = binary_->get_symbol(imported_name);
      }
      if (symbol != nullptr) {
        export_info->alias_  = symbol;
        symbol->export_info_ = export_info.get();
        symbol->value_       = export_info->address();
      } else {
        auto symbol = std::make_unique<Symbol>();
        symbol->origin_            = Symbol::ORIGIN::DYLD_EXPORT;
        symbol->value_             = export_info->address();
        symbol->type_              = 0;
        symbol->numberof_sections_ = 0;
        symbol->description_       = 0;
        symbol->name(symbol_name);

        // Weak bind of the pointer
        symbol->export_info_      = export_info.get();
        export_info->alias_       = symbol.get();
        binary_->symbols_.push_back(std::move(symbol));
      }


      if (ordinal < binary_->libraries().size()) {
        DylibCommand& lib = binary_->libraries()[ordinal];
        export_info->alias_location_ = &lib;
      } else {
        LIEF_WARN("Library ordinal out of range");
      }
    } else {
      auto address = stream.read_uleb128();
      if (!address) {
        LIEF_ERR("Can't read export address");
        return make_error_code(lief_errors::parsing_error);
      }
      export_info->address(*address);
    }

    // STUB_AND_RESOLVER
    // =================
    if (export_info->has(ExportInfo::FLAGS::STUB_AND_RESOLVER)) {
      auto other = stream.read_uleb128();
      if (!other) {
        LIEF_ERR("Can't read 'other' value for the export info");
        return make_error_code(lief_errors::parsing_error);
      }
      export_info->other_ = *other;
    }

    exports.push_back(std::move(export_info));

  }
  stream.setpos(children_offset);
  const auto nb_children = stream.read<uint8_t>();
  if (!nb_children) {
    LIEF_ERR("Can't read nb_children");
    return make_error_code(lief_errors::parsing_error);
  }
  for (size_t i = 0; i < *nb_children; ++i) {
    auto suffix = stream.read_string();
    if (!suffix) {
      LIEF_ERR("Can't read suffix");
      break;
    }
    std::string name = prefix + std::move(*suffix);

    if (!is_printable(name)) {
      if (!*invalid_names) {
        LIEF_WARN("The export trie contains non-printable symbols");
        *invalid_names = true;
      }
    }

    auto res_child_node_offet = stream.read_uleb128();
    if (!res_child_node_offet) {
      LIEF_ERR("Can't read child_node_offet");
      break;
    }
    auto child_node_offet = static_cast<uint32_t>(*res_child_node_offet);

    if (child_node_offet == 0) {
      break;
    }

    if (!visited_.insert(child_node_offet).second) {
      break;
    }

    {
      ScopedStream scoped(stream, child_node_offet);
      parse_export_trie(exports, *scoped, start, name, invalid_names);
    }
  }
  return ok();
}

ok_error_t BinaryParser::parse_dyld_exports() {
  DyldExportsTrie* exports = binary_->dyld_exports_trie();
  if (exports == nullptr) {
    LIEF_ERR("Missing LC_DYLD_EXPORTS_TRIE in the main binary");
    return make_error_code(lief_errors::not_found);
  }

  uint32_t offset = exports->data_offset();
  uint32_t size   = exports->data_size();

  if (offset == 0 || size == 0) {
    return ok();
  }

  SegmentCommand* linkedit = binary_->segment_from_offset(offset);

  if (linkedit == nullptr) {
    linkedit = binary_->get_segment("__LINKEDIT");
  }

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the export trie");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();
  const uint64_t rel_offset = offset - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("The export trie is out of bounds of the segment {}", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  exports->content_ = content.subspan(rel_offset, size);
  SpanStream trie_stream(exports->content_);

  bool invalid_names = false;
  parse_export_trie(exports->export_info_, trie_stream, offset, "",
                    &invalid_names);
  return ok();
}

ok_error_t BinaryParser::parse_dyldinfo_export() {
  LIEF_DEBUG("[+] LC_DYLD_INFO.exports");
  DyldInfo* dyldinfo = binary_->dyld_info();
  if (dyldinfo == nullptr) {
    LIEF_ERR("Missing DyldInfo in the main binary");
    return make_error_code(lief_errors::not_found);
  }

  uint32_t offset = std::get<0>(dyldinfo->export_info());
  uint32_t size   = std::get<1>(dyldinfo->export_info());

  if (offset == 0 || size == 0) {
    return ok();
  }

  SegmentCommand* linkedit = binary_->segment_from_offset(offset);

  if (linkedit == nullptr) {
    linkedit = binary_->get_segment("__LINKEDIT");
  }

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the export trie");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();
  const uint64_t rel_offset = offset - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("The export trie is out of bounds of the segment {}", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  dyldinfo->export_trie_ = content.subspan(rel_offset, size);

  SpanStream trie_stream(dyldinfo->export_trie_);

  bool invalid_names = false;
  parse_export_trie(dyldinfo->export_info_, trie_stream, offset, "", &invalid_names);
  return ok();
}


ok_error_t BinaryParser::parse_overlay() {
  const uint64_t last_offset = binary_->off_ranges().end;
  if (last_offset >= stream_->size()) {
    return ok();
  }

  const uint64_t overlay_size = stream_->size() - last_offset;
  LIEF_INFO("Overlay detected at 0x{:x} ({} bytes)", last_offset, overlay_size);
  if (!stream_->peek_data(binary_->overlay_, last_offset, overlay_size)) {
    LIEF_WARN("Can't read overlay data");
    return make_error_code(lief_errors::read_error);
  }
  return ok();
}


ok_error_t BinaryParser::parse_indirect_symbols(DynamicSymbolCommand& cmd,
                                                std::vector<Symbol*>& symtab,
                                                BinaryStream& indirect_stream)
{
  for (size_t i = 0; i < cmd.nb_indirect_symbols(); ++i) {
    uint32_t index = 0;
    auto res = indirect_stream.read<uint32_t>();
    if (!res) {
      LIEF_ERR("Can't read indirect symbol #{}", index);
      return make_error_code(lief_errors::read_error);
    }
    index = *res;

    if (index == details::INDIRECT_SYMBOL_ABS) {
      cmd.indirect_symbols_.push_back(const_cast<Symbol*>(&Symbol::indirect_abs()));
      continue;
    }

    if (index == details::INDIRECT_SYMBOL_LOCAL) {
      cmd.indirect_symbols_.push_back(const_cast<Symbol*>(&Symbol::indirect_local()));
      continue;
    }

    if (index == (details::INDIRECT_SYMBOL_LOCAL | details::INDIRECT_SYMBOL_ABS)) {
      cmd.indirect_symbols_.push_back(const_cast<Symbol*>(&Symbol::indirect_abs_local()));
      continue;
    }

    if (index >= symtab.size()) {
      LIEF_ERR("Indirect symbol index is out of range ({}/0x{:x} vs max sym: {})",
               index, index, symtab.size());
      continue;
    }

    Symbol* indirect = symtab[index];
    LIEF_DEBUG("  indirectsyms[{}] = {}", index, indirect->name());
    cmd.indirect_symbols_.push_back(indirect);
  }
  LIEF_DEBUG("indirect_symbols_.size(): {} (nb_indirect_symbols: {})",
             cmd.indirect_symbols_.size(), cmd.nb_indirect_symbols());
  return ok();
}

} // namespace MachO
} // namespace LIEF
