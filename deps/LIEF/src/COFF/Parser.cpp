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
#include "LIEF/COFF/Parser.hpp"
#include "LIEF/COFF/Binary.hpp"
#include "LIEF/COFF/RegularHeader.hpp"
#include "LIEF/COFF/Section.hpp"
#include "LIEF/COFF/Relocation.hpp"
#include "LIEF/COFF/Symbol.hpp"
#include "LIEF/COFF/String.hpp"
#include "LIEF/COFF/utils.hpp"
#include "LIEF/COFF/AuxiliarySymbol.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryCLRToken.hpp"

#include "COFF/structures.hpp"

#include "logging.hpp"

namespace LIEF::COFF {
std::unique_ptr<Binary> Parser::parse(std::unique_ptr<BinaryStream> stream,
                                      const ParserConfig& config)
{
  if (stream == nullptr) {
    return nullptr;
  }

  Header::KIND kind = get_kind(*stream);
  if (kind == Header::KIND::UNKNOWN) {
    return nullptr;
  }

  Parser parser(std::move(stream), config, kind);
  parser.bin_ = std::unique_ptr<Binary>(new Binary());
  if (parser.process()) {
    return std::move(parser.bin_);
  }

  return nullptr;
}

ok_error_t Parser::process() {
  if (!parse_header()) {
    LIEF_WARN("Failed to parse COFF header");
    return make_error_code(lief_errors::parsing_error);
  }

  if (!parse_optional_header()) {
    LIEF_WARN("Failed to parse optional header");
    return make_error_code(lief_errors::parsing_error);
  }

  if (!parse_string_table()) {
    LIEF_WARN("Failed to parse the string table");
    return make_error_code(lief_errors::parsing_error);
  }

  if (!parse_symbols()) {
    LIEF_WARN("Failed to parse symbols");
    return make_error_code(lief_errors::parsing_error);
  }

  if (!parse_sections()) {
    LIEF_WARN("Failed to parse sections");
    return make_error_code(lief_errors::parsing_error);
  }

  return ok();
}

ok_error_t Parser::parse_header() {
  LIEF_DEBUG("Parsing COFF header ({})", to_string(kind_));
  std::unique_ptr<Header> hdr = Header::create(*stream_, kind_);
  if (hdr == nullptr) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::parsing_error);
  }
  bin_->header_ = std::move(hdr);
  return ok();
}

ok_error_t Parser::parse_optional_header() {
  const auto* hdr = bin_->header().as<RegularHeader>();
  if (hdr == nullptr) {
    return ok();
  }

  const size_t sz = hdr->sizeof_optionalheader();
  if (sz == 0) {
    return ok();
  }

  LIEF_DEBUG("Parsing optional header (size={} bytes)", sz);

  std::vector<uint8_t> raw_opt_hdr;
  if (!stream_->read_data(raw_opt_hdr, sz)) {
    LIEF_ERR("Can't read COFF optional header");
    return make_error_code(lief_errors::parsing_error);
  }

  return ok();
}

ok_error_t Parser::parse_sections() {
  const size_t nb_sections = bin_->header().nb_sections();
  LIEF_DEBUG("Parsing #{} section (offset=0x{:08x})", nb_sections,
              (uint32_t)stream_->pos());
  for (size_t i = 0; i < nb_sections; ++i) {
    std::unique_ptr<Section> sec = Section::parse(*stream_);
    if (sec == nullptr) {
      LIEF_WARN("Can't parse section #{}", i);
      break;
    }
    if (!parse_relocations(*sec)) {
      LIEF_INFO("Failed to parse relocation of section #{}", i);
    }

    // Resolve symbols associated with this section
    auto range = std::equal_range(symsec_.begin(), symsec_.end(),
        SymSec{i, nullptr}
    );
    for (auto it = range.first; it != range.second; ++it) {
      assert(it->symbol != nullptr);
      it->symbol->section_ = sec.get();
      sec->symbols_.push_back(it->symbol);
    }

    bin_->sections_.push_back(std::move(sec));
  }
  return ok();
}

ok_error_t Parser::parse_relocations(Section& section) {
  size_t nb_relocations = section.numberof_relocations();

  const uint32_t reloc_offset = section.pointerto_relocation();
  if (nb_relocations == 0 || reloc_offset == 0) {
    return ok();
  }

  LIEF_DEBUG("Parsing #{} relocations at offset: {:#x} (section: '{}')",
             nb_relocations, reloc_offset, section.name());

  ScopedStream strm(*stream_, reloc_offset);

  if (section.has_extended_relocations()) {
    std::unique_ptr<Relocation> reloc =
      Relocation::parse(*strm, bin_->header().machine());

    if (reloc == nullptr) {
      LIEF_ERR("Can't parse the first relocations");
      return ok();
    }

    if (reloc->address() < 1) {
      LIEF_ERR("Invalid number of relocations");
      return ok();
    }

    nb_relocations = reloc->address() - 1;
    LIEF_DEBUG("Number of (extended) relocations: {}", nb_relocations);

    reloc->section_ = &section;
    section.relocations_.push_back(reloc.get());
    bin_->relocations_.push_back(std::move(reloc));
  }


  for (size_t i = 0; i < nb_relocations; ++i) {
    std::unique_ptr<Relocation> reloc =
      Relocation::parse(*strm, bin_->header().machine());

    if (reloc == nullptr) {
      LIEF_WARN("Can't parse relocation #{} in section: '{}'", i, section.name());
      break;
    }

    if (auto it = symbol_idx_.find(reloc->symbol_idx()); it != symbol_idx_.end()) {
      reloc->symbol_ = it->second;
    }

    reloc->section_ = &section;
    section.relocations_.push_back(reloc.get());
    bin_->relocations_.push_back(std::move(reloc));
  }

  return ok();
}

String* Parser::find_coff_string(uint32_t offset) const {
  auto it = memoize_coff_str_.find(offset);
  if (it == memoize_coff_str_.end()) {
    return nullptr;
  }
  return &bin_->strings_table_[it->second];
}

ok_error_t Parser::parse_symbols() {
  const Header& hdr = bin_->header();
  const size_t nb_symbols = hdr.nb_symbols();
  const uint32_t symbols_offset = hdr.pointerto_symbol_table();
  if (nb_symbols == 0 || symbols_offset == 0) {
    return ok();
  }

  ScopedStream strm(*stream_, symbols_offset);
  Symbol::parsing_context_t ctx {
    /*.find_string =*/ [this] (uint32_t offset) {
      return this->find_coff_string(offset);
    },
    /*is_bigobj=*/kind_ == Header::KIND::BIGOBJ
  };
  LIEF_DEBUG("Parsing #{} symbols at {:#x}", nb_symbols, symbols_offset);

  std::vector<AuxiliaryCLRToken*> pending_resolution;
  for (size_t idx = 0; idx < nb_symbols;) {
    size_t current_idx = idx;
    std::unique_ptr<Symbol> sym = Symbol::parse(ctx, *strm, &idx);
    if (sym == nullptr) {
      LIEF_WARN("Can't parse symbol #{}", idx);
      break;
    }

    for (AuxiliarySymbol& aux : sym->auxiliary_symbols()) {
      auto* crl_token = aux.as<AuxiliaryCLRToken>();
      if (crl_token == nullptr) {
        continue;
      }
      pending_resolution.push_back(crl_token);
    }

    if (sym->section_idx() > 0 && (uint32_t)sym->section_idx() <= hdr.nb_sections()) {
      symsec_.push_back({(size_t)sym->section_idx() - 1, sym.get()});
    }

    symbol_idx_.insert({current_idx, sym.get()});
    bin_->symbols_.push_back(std::move(sym));
  }

  std::stable_sort(symsec_.begin(), symsec_.end());

  for (AuxiliaryCLRToken* clr : pending_resolution) {
    if (auto sym = symbol_idx_.find(clr->symbol_idx()); sym != symbol_idx_.end()) {
      clr->sym_ = sym->second;
    }
  }

  return ok();
}
void Parser::memoize(String str) {
  const uint32_t offset = str.offset();
  memoize_coff_str_[offset] = bin_->strings_table_.size();
  bin_->strings_table_.push_back(std::move(str));
}

ok_error_t Parser::parse_string_table() {
  const Header& hdr = bin_->header();
  const size_t nb_symbols = hdr.nb_symbols();
  const uint32_t symbols_offset = hdr.pointerto_symbol_table();
  if (symbols_offset == 0) {
    return ok();
  }
  const size_t sizeof_sym =
    kind_ ==  Header::KIND::BIGOBJ ? sizeof(details::symbol32) :
                                     sizeof(details::symbol16);

  const uint32_t string_tbl_offset = symbols_offset + nb_symbols * sizeof_sym;

  LIEF_DEBUG("Parsing string table (offset: {:#x})", string_tbl_offset);

  ScopedStream scoped(*stream_, string_tbl_offset);
  auto table_sz = scoped->read<uint32_t>();
  if (!table_sz) {
    return make_error_code(table_sz.error());
  }

  if (*table_sz <= 4) {
    return ok();
  }

  std::vector<uint8_t> buffer;
  if (auto is_ok = scoped->read_data(buffer, *table_sz - 4); !is_ok) {
    return make_error_code(is_ok.error());
  }

  SpanStream string_strm(buffer);

  while (string_strm) {
    size_t pos = string_strm.pos() + 4;
    auto str = string_strm.read_string();
    if (!str) {
      break;
    }

    LIEF_DEBUG("string[0x{:06x}]: {}", pos, *str);
    memoize(String(pos, std::move(*str)));
  }
  LIEF_DEBUG("#{} strings found", bin_->strings_table_.size());

  return ok();
}

Parser::~Parser() = default;

}
