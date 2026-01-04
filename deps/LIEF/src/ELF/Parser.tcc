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
#include <cctype>
#include <memory>
#include <unordered_set>
#include "logging.hpp"

#include "LIEF/utils.hpp"
#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/ELF/hash.hpp"
#include "LIEF/ELF/Parser.hpp"
#include "LIEF/ELF/DynamicEntryFlags.hpp"
#include "LIEF/ELF/Relocation.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/GnuHash.hpp"
#include "LIEF/ELF/DynamicEntryLibrary.hpp"
#include "LIEF/ELF/DynamicEntryArray.hpp"
#include "LIEF/ELF/DynamicSharedObject.hpp"
#include "LIEF/ELF/DynamicEntryRunPath.hpp"
#include "LIEF/ELF/DynamicEntryRpath.hpp"
#include "LIEF/ELF/SymbolVersionRequirement.hpp"
#include "LIEF/ELF/SymbolVersionDefinition.hpp"
#include "LIEF/ELF/SymbolVersionAuxRequirement.hpp"
#include "LIEF/ELF/SymbolVersionAux.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/SymbolVersion.hpp"
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/EnumToString.hpp"

#include "ELF/Structures.hpp"
#include "ELF/DataHandler/Handler.hpp"
#include "ELF/SizingInfo.hpp"

#include "Object.tcc"
#include "internal_utils.hpp"

namespace LIEF {
namespace ELF {
template<typename ELF_T>
ok_error_t Parser::parse_binary() {

  LIEF_DEBUG("Start parsing");
  // Parse header
  // ============
  auto res = parse_header<ELF_T>();
  if (!res) {
    LIEF_WARN("ELF Header parsed with errors");
  }

  // Parse Sections
  // ==============
  if (binary_->header_.section_headers_offset() > 0) {
    parse_sections<ELF_T>();
  } else {
    LIEF_WARN("The current binary doesn't have a section header");
  }

  // Parse segments
  // ==============
  if (binary_->header_.program_headers_offset() > 0) {
    parse_segments<ELF_T>();
  } else {
    if (binary_->header().file_type() != Header::FILE_TYPE::REL) {
      LIEF_WARN("Binary doesn't have a program header");
    }
  }

  if (Segment* seg_dyn = binary_->get(Segment::TYPE::DYNAMIC)) {
    if (!parse_dyn_table<ELF_T>(*seg_dyn)) {
      LIEF_WARN("PT_DYNAMIC parsing failed with error");
    }
  }

  process_dynamic_table<ELF_T>();

  if (const Section* sec_symbtab = binary_->get(Section::TYPE::SYMTAB)) {
    auto nb_entries = static_cast<uint32_t>((sec_symbtab->size() / sizeof(typename ELF_T::Elf_Sym)));
    nb_entries = std::min(nb_entries, Parser::NB_MAX_SYMBOLS);

    if (sec_symbtab->link() == 0 || sec_symbtab->link() >= binary_->sections_.size()) {
      LIEF_WARN("section->link() is not valid !");
    } else {
      if (config_.parse_symtab_symbols) {
        // We should have:
        // nb_entries == section->information())
        // but lots of compiler not respect this rule
        parse_symtab_symbols<ELF_T>(sec_symbtab->file_offset(), nb_entries,
                                    *binary_->sections_[sec_symbtab->link()]);
      }
    }
  }


  // Parse Symbols's hash
  // ====================
  if (DynamicEntry* dt_hash = binary_->get(DynamicEntry::TAG::HASH)) {
    if (auto res = binary_->virtual_address_to_offset(dt_hash->value())) {
      parse_symbol_sysv_hash(*res);
    } else {
      LIEF_WARN("Can't convert DT_HASH.virtual_address into an offset (0x{:x})", dt_hash->value());
    }
  }


  if (DynamicEntry* dt = binary_->get(DynamicEntry::TAG::GNU_HASH)) {
    if (Segment* seg = binary_->segment_from_virtual_address(dt->value())) {
      const auto dynsymcount = (uint64_t)(binary_->dynamic_symbols_.size());
      std::unique_ptr<SpanStream> strm = seg->stream();
      const uint64_t addr = dt->value();
      if (strm != nullptr) {
        strm->set_endian_swap(stream_->should_swap());
        assert(addr >= seg->virtual_address());
        uint64_t offset = addr - seg->virtual_address();
        strm->setpos(offset);
        binary_->gnu_hash_ = GnuHash::parse<ELF_T>(*strm, dynsymcount);
        binary_->sizing_info_->gnu_hash = binary_->gnu_hash_->original_size();
      }
    }
  }

  if (config_.parse_notes) {
    // Parse Note segment
    // ==================
    for (const Segment& segment : binary_->segments()) {
      if (segment.type() != Segment::TYPE::NOTE) {
        continue;
      }
      parse_notes(segment.file_offset(), segment.physical_size());
    }

    // Parse Note Sections
    // ===================
    for (const Section& section : binary_->sections()) {
      if (section.type() != Section::TYPE::NOTE) {
        continue;
      }
      LIEF_DEBUG("Notes from section: {}", section.name());
      parse_notes(section.offset(), section.size());
    }
  }

  // Try to parse using sections
  // If we don't have any relocations, we parse all relocation sections
  // otherwise, only the non-allocated sections to avoid parsing dynamic
  // relocations (or plt relocations) twice.
  if (config_.parse_relocations) {
    bool skip_allocated_sections = !binary_->relocations_.empty();
    for (const Section& section : binary_->sections()) {
      if (skip_allocated_sections && section.has(Section::FLAGS::ALLOC)){
        continue;
      }
      if (section.type() == Section::TYPE::REL) {
        parse_section_relocations<ELF_T, typename ELF_T::Elf_Rel>(section);
      }
      else if (section.type() == Section::TYPE::RELA) {
        parse_section_relocations<ELF_T, typename ELF_T::Elf_Rela>(section);
      }
    }
  }
  if (config_.parse_symbol_versions) {
    link_symbol_version();
  }

  if (config_.parse_overlay) {
    parse_overlay();
  }
  return ok();
}


template<class ELF_T>
ok_error_t Parser::parse_dyn_table(Segment& pt_dyn) {
  // Parse the dynamic table. To process this table, we can either process
  // the content of the PT_DYNAMIC segment or process the content of the PT_LOAD
  // segment that wraps the dynamic table. The second approach should be
  // preferred since it uses a more accurate representation.
  // (c.f. samples `issue_dynamic_table.elf` provided by @lebr0nli)
  using Elf_Off = typename ELF_T::Elf_Off;
  std::vector<Segment*> segments;

  // Find the PT_LOAD segment that wraps the PT_DYNAMIC table.
  // As demonstrated in the library: ELF32_x86_library_libshellx.so
  // we need to consider overlapping segments and take the "latest" one since
  // this is what the loader would do.
  for (const std::unique_ptr<Segment>& segment : binary_->segments_) {
    if (!segment->is_load()) {
      continue;
    }
    const uint64_t dyn_start = pt_dyn.virtual_address();
    const uint64_t dyn_end = dyn_start + pt_dyn.virtual_size();
    const uint64_t load_start = segment->virtual_address();
    const uint64_t load_end = load_start + segment->virtual_size();
    if (!(load_start <= dyn_start && dyn_start < load_end)) {
      continue;
    }

    if (!(load_start < dyn_end && dyn_end <= load_end)) {
      continue;
    }
    segments.push_back(segment.get());
  }

  binary_->sizing_info_->dynamic = pt_dyn.physical_size();

  // Usually #segments is 1 but we might have > 1 for overlapping segments
  LIEF_DEBUG("Nb segments: {}", segments.size());

  if (segments.empty()) {
    // No PT_LOAD segment wrapping up the PT_DYNAMIC table
    const Elf_Off offset = pt_dyn.file_offset();
    ScopedStream scoped(*stream_, offset);
    return parse_dynamic_entries<ELF_T>(*scoped);
  }
  const Segment& load_seg = *segments.back();
  LIEF_DEBUG("Dynamic content wrapped by segment LOAD: [0x{:016x}, 0x{:016x}] "
             "[0x{:016x}, 0x{:016x}]", load_seg.virtual_address(),
             load_seg.virtual_address() + load_seg.virtual_size(),
             load_seg.file_offset(), load_seg.file_offset() + load_seg.physical_size());

  span<const uint8_t> seg_content = load_seg.content();
  if (seg_content.empty()) {
    return make_error_code(lief_errors::corrupted);
  }

  int64_t rel_offset = (int64_t)pt_dyn.virtual_address() - (int64_t)load_seg.virtual_address();
  if (rel_offset < 0 || (uint64_t)rel_offset >= seg_content.size()) {
    return make_error_code(lief_errors::corrupted);
  }

  span<const uint8_t> dynamic_content = seg_content.subspan(rel_offset);
  SpanStream stream(dynamic_content);
  stream.set_endian_swap(stream_->should_swap());
  return parse_dynamic_entries<ELF_T>(stream);
}

template<typename ELF_T>
ok_error_t Parser::process_dynamic_table() {
  {
    DynamicEntry* dt_symtab = binary_->get(DynamicEntry::TAG::SYMTAB);
    DynamicEntry* dt_syment = binary_->get(DynamicEntry::TAG::SYMENT);

    if (dt_syment != nullptr && dt_syment->value() != sizeof(typename ELF_T::Elf_Sym)) {
      LIEF_WARN("DT_SYMTENT is corrupted");
    }

    if (dt_symtab != nullptr && config_.parse_dyn_symbols) {
      const uint64_t virtual_address = dt_symtab->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_dynamic_symbols<ELF_T>(*res);
      } else {
        LIEF_WARN("Can't convert DT_SYMTAB.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  {
    DynamicEntry* dt_rela   = binary_->get(DynamicEntry::TAG::RELA);
    DynamicEntry* dt_relasz = binary_->get(DynamicEntry::TAG::RELASZ);

    if (dt_rela != nullptr && dt_relasz != nullptr && config_.parse_relocations) {
      const uint64_t virtual_address = dt_rela->value();
      const uint64_t size            = dt_relasz->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_dynamic_relocations<ELF_T, typename ELF_T::Elf_Rela>(*res, size);
        binary_->sizing_info_->rela = size;
      } else {
        LIEF_WARN("Can't convert DT_RELA.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  {
    DynamicEntry* dt_rel   = binary_->get(DynamicEntry::TAG::REL);
    DynamicEntry* dt_relsz = binary_->get(DynamicEntry::TAG::RELSZ);

    if (dt_rel != nullptr && dt_relsz != nullptr && config_.parse_relocations) {
      const uint64_t virtual_address = dt_rel->value();
      const uint64_t size            = dt_relsz->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_dynamic_relocations<ELF_T, typename ELF_T::Elf_Rel>(*res, size);
        binary_->sizing_info_->rela = size;
      } else {
        LIEF_WARN("Can't convert DT_REL.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  {
    DynamicEntry* dt_relr   = binary_->get(DynamicEntry::TAG::RELR);
    DynamicEntry* dt_relrsz = binary_->get(DynamicEntry::TAG::RELRSZ);

    if (dt_relr != nullptr && dt_relrsz != nullptr && config_.parse_relocations) {
      const uint64_t virtual_address = dt_relr->value();
      const uint64_t size            = dt_relrsz->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_relative_relocations<ELF_T>(*res, size);
        binary_->sizing_info_->relr = size;
      } else {
        LIEF_WARN("Can't convert DT_RELR.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  {
    DynamicEntry* dt_relr   = binary_->get(DynamicEntry::TAG::ANDROID_RELR);
    DynamicEntry* dt_relrsz = binary_->get(DynamicEntry::TAG::ANDROID_RELRSZ);

    if (dt_relr != nullptr && dt_relrsz != nullptr && config_.parse_relocations) {
      const uint64_t virtual_address = dt_relr->value();
      const uint64_t size            = dt_relrsz->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_relative_relocations<ELF_T>(*res, size);
        binary_->sizing_info_->relr = size;
      } else {
        LIEF_WARN("Can't convert (Android)DT_RELR.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  {
    DynamicEntry* dt_rela = binary_->get(DynamicEntry::TAG::ANDROID_RELA);
    DynamicEntry* dt_relasz = binary_->get(DynamicEntry::TAG::ANDROID_RELASZ);
    if (dt_rela == nullptr) {
      dt_rela = binary_->get(DynamicEntry::TAG::ANDROID_REL);
      dt_relasz = binary_->get(DynamicEntry::TAG::ANDROID_RELSZ);
    }

    if (dt_rela != nullptr && dt_relasz != nullptr && config_.parse_relocations) {
      const uint64_t virtual_address = dt_rela->value();
      const uint64_t size            = dt_relasz->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_packed_relocations<ELF_T>(*res, size);
        binary_->sizing_info_->android_rela = size;
      } else {
        LIEF_WARN("Can't convert DT_ANDROID_REL[A].virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  {
    DynamicEntry* dt_jmprel   = binary_->get(DynamicEntry::TAG::JMPREL);
    DynamicEntry* dt_pltrelsz = binary_->get(DynamicEntry::TAG::PLTRELSZ);

    if (dt_jmprel != nullptr && dt_pltrelsz != nullptr && config_.parse_relocations) {
      const uint64_t virtual_address = dt_jmprel->value();
      const uint64_t size            = dt_pltrelsz->value();
      DynamicEntry* dt_pltrel        = binary_->get(DynamicEntry::TAG::PLTREL);
      DynamicEntry::TAG type;
      if (dt_pltrel != nullptr) {
        type = DynamicEntry::from_value(dt_pltrel->value(), binary_->header().machine_type());
      } else {
        // Try to guess: We assume that on ELF64 -> DT_RELA and on ELF32 -> DT_REL
        if constexpr (std::is_same_v<ELF_T, details::ELF64>) {
          type = DynamicEntry::TAG::RELA;
        } else {
          type = DynamicEntry::TAG::REL;
        }
      }

      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        type == DynamicEntry::TAG::RELA ?
                parse_pltgot_relocations<ELF_T, typename ELF_T::Elf_Rela>(*res, size) :
                parse_pltgot_relocations<ELF_T, typename ELF_T::Elf_Rel>(*res, size);
        binary_->sizing_info_->jmprel = size;
      } else {
        LIEF_WARN("Can't convert DT_JMPREL.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }
  if (config_.parse_symbol_versions && config_.parse_dyn_symbols) {
    if (DynamicEntry* dt_versym = binary_->get(DynamicEntry::TAG::VERSYM)) {
      const uint64_t virtual_address = dt_versym->value();
      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_symbol_version(*res);
        binary_->sizing_info_->versym = binary_->dynamic_symbols_.size() * sizeof(uint16_t);
      } else {
        LIEF_WARN("Can't convert DT_VERSYM.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }

  if (config_.parse_symbol_versions) {
    DynamicEntry* dt_verneed     = binary_->get(DynamicEntry::TAG::VERNEED);
    DynamicEntry* dt_verneed_num = binary_->get(DynamicEntry::TAG::VERNEEDNUM);

    if (dt_verneed != nullptr && dt_verneed_num != nullptr) {
      const uint64_t virtual_address = dt_verneed->value();
      const uint32_t nb_entries = std::min(Parser::NB_MAX_SYMBOLS,
                                           static_cast<uint32_t>(dt_verneed_num->value()));

      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_symbol_version_requirement<ELF_T>(*res, nb_entries);
      } else {
        LIEF_WARN("Can't convert DT_VERNEED.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }

  if (config_.parse_symbol_versions) {
    DynamicEntry* dt_verdef     = binary_->get(DynamicEntry::TAG::VERDEF);
    DynamicEntry* dt_verdef_num = binary_->get(DynamicEntry::TAG::VERDEFNUM);
    if (dt_verdef != nullptr && dt_verdef_num != nullptr) {
      const uint64_t virtual_address = dt_verdef->value();
      const auto size                = static_cast<uint32_t>(dt_verdef_num->value());

      if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
        parse_symbol_version_definition<ELF_T>(*res, size);
      } else {
        LIEF_WARN("Can't convert DT_VERDEF.virtual_address into an offset (0x{:x})", virtual_address);
      }
    }
  }

  return ok();
}

template<typename ELF_T>
ok_error_t Parser::parse_header() {
  using Elf_Half = typename ELF_T::Elf_Half;
  using Elf_Word = typename ELF_T::Elf_Word;
  using Elf_Addr = typename ELF_T::Elf_Addr;
  using Elf_Off  = typename ELF_T::Elf_Off;

  LIEF_DEBUG("[+] Parsing Header");
  stream_->setpos(0);
  if (auto res = stream_->read<Header::identity_t>()) {
    binary_->header_.identity_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_ident");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.file_type_ = Header::FILE_TYPE(*res);
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_type");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.machine_type_ = static_cast<ARCH>(*res);
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_machine");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Word>()) {
    binary_->header_.object_file_version_ = Header::VERSION(*res);
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_version");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Addr>()) {
    binary_->header_.entrypoint_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_entry");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Off>()) {
    binary_->header_.program_headers_offset_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_phoff");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Off>()) {
    binary_->header_.section_headers_offset_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_shoff");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Word>()) {
    binary_->header_.processor_flags_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_flags");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.header_size_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_ehsize");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.program_header_size_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_phentsize");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.numberof_segments_ = *res;
  } else {
    if (auto res = stream_->read<uint8_t>()) {
      binary_->header_.numberof_segments_ = *res;
    } else {
      LIEF_ERR("Can't parse Elf_Ehdr.e_phnum");
      return make_error_code(lief_errors::read_error);
    }
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.section_header_size_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_shentsize");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.numberof_sections_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_shnum");
    return make_error_code(lief_errors::read_error);
  }

  if (auto res = stream_->read<Elf_Half>()) {
    binary_->header_.section_string_table_idx_ = *res;
  } else {
    LIEF_ERR("Can't parse Elf_Ehdr.e_shstrndx");
    return make_error_code(lief_errors::read_error);
  }

  return ok();
}


template<typename ELF_T>
result<uint32_t> Parser::get_numberof_dynamic_symbols(ParserConfig::DYNSYM_COUNT mtd) const {

  switch(mtd) {
    case ParserConfig::DYNSYM_COUNT::HASH:        return nb_dynsym_hash<ELF_T>();
    case ParserConfig::DYNSYM_COUNT::SECTION:     return nb_dynsym_section<ELF_T>();
    case ParserConfig::DYNSYM_COUNT::RELOCATIONS: return nb_dynsym_relocations<ELF_T>();

    case ParserConfig::DYNSYM_COUNT::AUTO:
    default:
      {
        uint32_t nb_reloc   = 0;
        uint32_t nb_section = 0;
        uint32_t nb_hash    = 0;

        if (auto res = get_numberof_dynamic_symbols<ELF_T>(ParserConfig::DYNSYM_COUNT::RELOCATIONS)) {
          nb_reloc = *res;
        }

        if (auto res = get_numberof_dynamic_symbols<ELF_T>(ParserConfig::DYNSYM_COUNT::SECTION)) {
          nb_section = *res;
        }

        if (auto res = get_numberof_dynamic_symbols<ELF_T>(ParserConfig::DYNSYM_COUNT::HASH)) {
          nb_hash = *res;
        }

        LIEF_DEBUG("#dynsym.reloc: {}",   nb_reloc);
        LIEF_DEBUG("#dynsym.section: {}", nb_section);
        LIEF_DEBUG("#dynsym.hash: {}",    nb_hash);

        if (nb_hash > 0 && nb_section == nb_hash) {
          return nb_hash;
        }

        uint32_t candidate = nb_reloc;
        if (nb_section < Parser::NB_MAX_SYMBOLS &&
            nb_section > nb_reloc               &&
            (nb_section - nb_reloc) < Parser::DELTA_NB_SYMBOLS)
        {
          candidate = nb_section;
        }

        if (nb_hash == 0) {
          return candidate;
        }

        if (nb_hash < Parser::NB_MAX_SYMBOLS &&
            nb_hash > candidate              &&
            (nb_hash - candidate) < Parser::DELTA_NB_SYMBOLS)
        {
          candidate = nb_hash;
        }

        return candidate;
      }
  }
}

template<typename ELF_T>
result<uint32_t> Parser::nb_dynsym_relocations() const {
  using rela_t = typename ELF_T::Elf_Rela;
  using rel_t  = typename ELF_T::Elf_Rel;
  uint32_t nb_symbols = 0;

  // Dynamic Relocations
  // ===================

  // RELA
  // ----
  DynamicEntry* dt_rela   = binary_->get(DynamicEntry::TAG::RELA);
  DynamicEntry* dt_relasz = binary_->get(DynamicEntry::TAG::RELASZ);
  if (dt_rela != nullptr && dt_relasz != nullptr) {
    const uint64_t virtual_address = dt_rela->value();
    const uint64_t size            = dt_relasz->value();
    if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
      nb_symbols = std::max(nb_symbols, max_relocation_index<ELF_T, rela_t>(*res, size));
    }
  }


  // REL
  // ---
  DynamicEntry* dt_rel   = binary_->get(DynamicEntry::TAG::REL);
  DynamicEntry* dt_relsz = binary_->get(DynamicEntry::TAG::RELSZ);

  if (dt_rel != nullptr && dt_relsz != nullptr) {
    const uint64_t virtual_address = dt_rel->value();
    const uint64_t size            = dt_relsz->value();
    if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
      nb_symbols = std::max(nb_symbols, max_relocation_index<ELF_T, rel_t>(*res, size));
    }
  }

  // Parse PLT/GOT Relocations
  // ==========================

  DynamicEntry* dt_jmprel   = binary_->get(DynamicEntry::TAG::JMPREL);
  DynamicEntry* dt_pltrelsz = binary_->get(DynamicEntry::TAG::PLTRELSZ);
  if (dt_jmprel != nullptr && dt_pltrelsz != nullptr) {
    const uint64_t virtual_address = dt_jmprel->value();
    const uint64_t size            = dt_pltrelsz->value();
    DynamicEntry* dt_pltrel        = binary_->get(DynamicEntry::TAG::PLTREL);
    DynamicEntry::TAG type;
    if (dt_pltrel != nullptr) {
      type = DynamicEntry::from_value(dt_pltrel->value(), binary_->header().machine_type());
    } else {
      // Try to guess: We assume that on ELF64 -> DT_RELA and on ELF32 -> DT_REL
      if constexpr (std::is_same_v<ELF_T, details::ELF64>) {
        type = DynamicEntry::TAG::RELA;
      } else {
        type = DynamicEntry::TAG::REL;
      }
    }
    if (auto res = binary_->virtual_address_to_offset(virtual_address)) {
      if (type == DynamicEntry::TAG::RELA) {
        nb_symbols = std::max(nb_symbols, max_relocation_index<ELF_T, rela_t>(*res, size));
      } else {
        nb_symbols = std::max(nb_symbols, max_relocation_index<ELF_T, rel_t>(*res, size));
      }
    }
  }

  return nb_symbols;
}

template<typename ELF_T, typename REL_T>
uint32_t Parser::max_relocation_index(uint64_t relocations_offset, uint64_t size) const {
  static_assert(std::is_same<REL_T, typename ELF_T::Elf_Rel>::value ||
                std::is_same<REL_T, typename ELF_T::Elf_Rela>::value, "REL_T must be Elf_Rel || Elf_Rela");

  const uint8_t shift = ELF_T::r_info_shift;;

  const auto nb_entries = static_cast<uint32_t>(size / sizeof(REL_T));

  uint32_t idx = 0;
  stream_->setpos(relocations_offset);
  for (uint32_t i = 0; i < nb_entries; ++i) {
    auto reloc_entry = stream_->read<REL_T>();
    if (!reloc_entry) {
      break;
    }
    idx = std::max(idx, static_cast<uint32_t>(reloc_entry->r_info >> shift));
  }
  return idx + 1;
} // max_relocation_index

template<typename ELF_T>
result<uint32_t> Parser::nb_dynsym_section() const {
  using Elf_Sym = typename ELF_T::Elf_Sym;
  using Elf_Off = typename ELF_T::Elf_Off;
  Section* dynsym_sec = binary_->get(Section::TYPE::DYNSYM);

  if (dynsym_sec == nullptr) {
    return 0;
  }

  const Elf_Off section_size = dynsym_sec->size();
  const auto nb_symbols = static_cast<uint32_t>((section_size / sizeof(Elf_Sym)));
  return nb_symbols;
}

template<typename ELF_T>
result<uint32_t> Parser::nb_dynsym_hash() const {

  if (binary_->has(DynamicEntry::TAG::HASH)) {
    return nb_dynsym_sysv_hash<ELF_T>();
  }

  if (binary_->has(DynamicEntry::TAG::GNU_HASH)) {
    return nb_dynsym_gnu_hash<ELF_T>();
  }

  return 0;
}


template<typename ELF_T>
result<uint32_t> Parser::nb_dynsym_sysv_hash() const {
  using Elf_Off  = typename ELF_T::Elf_Off;

  const DynamicEntry* dyn_hash = binary_->get(DynamicEntry::TAG::HASH);
  if (dyn_hash == nullptr) {
    LIEF_ERR("Can't find DT_GNU_HASH");
    return make_error_code(lief_errors::not_found);
  }
  Elf_Off sysv_hash_offset = 0;
  if (auto res = binary_->virtual_address_to_offset(dyn_hash->value())) {
    sysv_hash_offset = *res;
  } else {
    return make_error_code(res.error());
  }

  // From the doc: 'so nchain should equal the number of symbol table entries.'
  stream_->setpos(sysv_hash_offset + sizeof(uint32_t));
  auto nb_symbols = stream_->read<uint32_t>();
  if (nb_symbols) {
    return nb_symbols;
  }

  return 0;
}

template<typename ELF_T>
result<uint32_t> Parser::nb_dynsym_gnu_hash() const {
  const DynamicEntry* dyn_hash = binary_->get(DynamicEntry::TAG::GNU_HASH);
  if (dyn_hash == nullptr) {
    LIEF_ERR("Can't find DT_GNU_HASH");
    return make_error_code(lief_errors::not_found);
  }

  const uint64_t addr = dyn_hash->value();

  if (Segment* seg = binary_->segment_from_virtual_address(addr)) {
    std::unique_ptr<SpanStream> stream = seg->stream();
    if (stream == nullptr) {
      return make_error_code(lief_errors::read_error);
    }
    uint64_t rel_offset = addr - seg->virtual_address();
    stream->setpos(rel_offset);
    stream->set_endian_swap(stream_->should_swap());
    return GnuHash::nb_symbols<ELF_T>(*stream);
  }

  return make_error_code(lief_errors::not_found);
}

template<typename ELF_T>
ok_error_t Parser::parse_sections() {
  using Elf_Shdr = typename ELF_T::Elf_Shdr;

  using Elf_Off  = typename ELF_T::Elf_Off;
  LIEF_DEBUG("Parsing Section");

  const Elf_Off shdr_offset = binary_->header_.section_headers_offset();
  const auto numberof_sections = binary_->header_.numberof_sections();

  stream_->setpos(shdr_offset);
  std::unordered_map<Section*, size_t> sections_names;
  DataHandler::Handler& handler = *binary_->datahandler_;
  const ARCH arch = binary_->header().machine_type();
  for (size_t i = 0; i < numberof_sections; ++i) {
    LIEF_DEBUG("  Elf_Shdr#{:02d}.offset: 0x{:x} ", i, stream_->pos());
    const auto shdr = stream_->read<Elf_Shdr>();
    if (!shdr) {
      LIEF_ERR("  Can't parse section #{:02d}", i);
      break;
    }

    auto section = std::unique_ptr<Section>(new Section(*shdr, arch));
    section->datahandler_ = binary_->datahandler_.get();

    const uint64_t section_start = section->file_offset();
    const uint64_t section_end   = section_start + section->size();
    bool access_content = true;
    if (section_start > stream_->size() || section_end > stream_->size()) {
      access_content = false;
      if (section->type() != Section::TYPE::NOBITS) {
        LIEF_WARN("Can't access the content of section #{}", i);
      }
    }

    if (section->size() == 0 && section->file_offset() > 0 && access_content) {
      // Even if the size is 0, it is worth creating the node
      handler.create(section->file_offset(), 0, DataHandler::Node::SECTION);
    }

    // Only if it contains data (with bits)
    if (section->size() > 0 && access_content) {
      int64_t read_size = section->size();
      if (static_cast<int32_t>(read_size) < 0 ) {
        LIEF_WARN("Section #{} is {} bytes large. Only the first {} bytes will be taken into account",
                  i, read_size, Section::MAX_SECTION_SIZE);
        read_size = Section::MAX_SECTION_SIZE;
      }
      if (read_size > Section::MAX_SECTION_SIZE) {
        LIEF_WARN("Section #{} is {} bytes large. Only the first {} bytes will be taken into account",
                  i, read_size, Section::MAX_SECTION_SIZE);
        read_size = Section::MAX_SECTION_SIZE;
      }

      handler.create(section->file_offset(), read_size,
                     DataHandler::Node::SECTION);

      const Elf_Off offset_to_content = section->file_offset();
      auto alloc = binary_->datahandler_->reserve(section->file_offset(), read_size);
      if (!alloc) {
        LIEF_ERR("Can't allocate memory");
        break;
      }

      /* The DataHandlerStream interface references ELF data that are
       * located in the ELF::DataHandler. Therefore, we can skip reading
       * the data since they are already present in the data handler.
       * This optimization saves memory (which is also performed in parse_segments<>(...))
       */
      if (stream_->type() != BinaryStream::STREAM_TYPE::ELF_DATA_HANDLER) {
        std::vector<uint8_t> sec_content;
        if (!stream_->peek_data(sec_content, offset_to_content, read_size)) {
          if (section->type() != Section::TYPE::NOBITS) {
            LIEF_WARN("  Unable to get content of section #{:d}", i);
          }
        } else {
          section->content(std::move(sec_content));
        }
      }
    }
    sections_idx_[i] = section.get();
    sections_names[section.get()] = shdr->sh_name;
    binary_->sections_.push_back(std::move(section));
  }

  LIEF_DEBUG("    Parse section names");
  // Parse name
  if (binary_->header_.section_name_table_idx() >= binary_->sections_.size()) {
    LIEF_WARN("The .shstr index is out of range of the section table");
    return ok();
  }

  const size_t section_string_index = binary_->header_.section_name_table_idx();
  const std::unique_ptr<Section>& string_section = binary_->sections_[section_string_index];
  for (std::unique_ptr<Section>& section : binary_->sections_) {
    const auto it_name_idx = sections_names.find(section.get());
    if (it_name_idx == std::end(sections_names)) {
      LIEF_WARN("Missing name_idx for section at offset 0x{:x}", section->file_offset());
      continue;
    }
    const size_t name_offset = it_name_idx->second;
    auto name = stream_->peek_string_at(string_section->file_offset() + name_offset);
    if (!name) {
      LIEF_ERR("Can't read section name for section 0x{:x}", section->file_offset());
      break;
    }
    section->name(*name);
  }
  return ok();
}

template<typename ELF_T>
ok_error_t Parser::parse_segments() {
  using Elf_Phdr = typename ELF_T::Elf_Phdr;
  using Elf_Off  = typename ELF_T::Elf_Off;

  LIEF_DEBUG("== Parse Segments ==");
  const Header& hdr = binary_->header();
  const Elf_Off segment_headers_offset = hdr.program_headers_offset();
  const auto nbof_segments = std::min<uint32_t>(hdr.numberof_segments(), Parser::NB_MAX_SEGMENTS);

  stream_->setpos(segment_headers_offset);

  const ARCH arch = hdr.machine_type();
  const Header::OS_ABI os = hdr.identity_os_abi();

  for (size_t i = 0; i < nbof_segments; ++i) {
    const auto elf_phdr = stream_->read<Elf_Phdr>();
    if (!elf_phdr) {
      LIEF_ERR("Can't parse segement #{:d}", i);
      break;
    }

    auto segment = std::unique_ptr<Segment>(new Segment(*elf_phdr, arch, os));
    segment->datahandler_ = binary_->datahandler_.get();

    if (0 < segment->physical_size() && segment->physical_size() < Parser::MAX_SEGMENT_SIZE) {
      uint64_t read_size = segment->physical_size();
      if (read_size > Parser::MAX_SEGMENT_SIZE) {
        LIEF_WARN("Segment #{} is {} bytes large. Only the first {} bytes will be taken into account",
                  i, read_size, Parser::MAX_SEGMENT_SIZE);
        read_size = Parser::MAX_SEGMENT_SIZE;
      }
      if (read_size > stream_->size()) {
        LIEF_WARN("Segment #{} has a physical size larger than the current stream size ({} > {}). "
                  "The content will be truncated with the stream size.",
                  i, read_size, stream_->size());
        read_size = stream_->size();
      }

      segment->datahandler_->create(segment->file_offset(), read_size,
                                    DataHandler::Node::SEGMENT);
      segment->handler_size_ = read_size;

      const bool corrupted_offset = segment->file_offset() > stream_->size() ||
                                    (segment->file_offset() + read_size) > stream_->size();

      if (!corrupted_offset) {
        const Elf_Off offset_to_content = segment->file_offset();
        auto alloc = binary_->datahandler_->reserve(segment->file_offset(), read_size);
        if (!alloc) {
          LIEF_ERR("Can't allocate memory");
          break;
        }
        /* The DataHandlerStream interface references ELF data that are
         * located in the ELF::DataHandler. Therefore, we can skip reading
         * the data since they are already present in the data handler.
         * This optimization saves memory (which is also performed in parse_sections<>(...))
         */
        if (stream_->type() != BinaryStream::STREAM_TYPE::ELF_DATA_HANDLER) {
          std::vector<uint8_t> seg_content;
          if (stream_->peek_data(seg_content, offset_to_content, read_size)) {
            segment->content(std::move(seg_content));
          } else {
            LIEF_ERR("Unable to get the content of segment #{:d}", i);
          }
        }

        if (segment->is_interpreter()) {
          auto interpreter = stream_->peek_string_at(offset_to_content, read_size);
          if (!interpreter) {
            LIEF_ERR("Can't read the interpreter string");
          } else {
            binary_->interpreter_ = *interpreter;
            binary_->sizing_info_->interpreter = read_size;
          }
        }
      }
    } else {
      segment->handler_size_ = segment->physical_size();
      segment->datahandler_->create(segment->file_offset(), segment->physical_size(),
                                    DataHandler::Node::SEGMENT);
    }

    for (std::unique_ptr<Section>& section : binary_->sections_) {
      if (check_section_in_segment(*section, *segment.get())) {
        section->segments_.push_back(segment.get());
        segment->sections_.push_back(section.get());
      }
    }
    binary_->segments_.push_back(std::move(segment));
  }
  return ok();
}


template<typename ELF_T>
ok_error_t Parser::parse_packed_relocations(uint64_t offset, uint64_t size) {
  using Elf_Rela = typename ELF_T::Elf_Rela;
  static constexpr uint64_t GROUPED_BY_INFO_FLAG         = 1 << 0;
  static constexpr uint64_t GROUPED_BY_OFFSET_DELTA_FLAG = 1 << 1;
  static constexpr uint64_t GROUPED_BY_ADDEND_FLAG       = 1 << 2;
  static constexpr uint64_t GROUP_HAS_ADDEND_FLAG        = 1 << 3;

  LIEF_DEBUG("Parsing Android packed relocations");
  if (size < 4) {
    LIEF_ERR("Invalid Android packed relocation header");
    return make_error_code(lief_errors::read_error);
  }
  ScopedStream rel_stream(*stream_, offset);

  const auto H0 = stream_->read<char>().value_or(0);
  const auto H1 = stream_->read<char>().value_or(0);
  const auto H2 = stream_->read<char>().value_or(0);
  const auto H3 = stream_->read<char>().value_or(0);

  LIEF_DEBUG("Header: {} {} {} {}", H0, H1, H2, H3);

  // Check for the Magic: APS2
  if (H0 != 'A' || H1 != 'P' || H2 != 'S' || H3 != '2') {
    LIEF_ERR("Invalid Android packed relocation magic header: "
             "{} {} {} {}", H0, H1, H2, H3);
    return make_error_code(lief_errors::read_error);
  }

  auto res_nb_relocs = rel_stream->read_sleb128();
  if (!res_nb_relocs) {
    LIEF_ERR("Can't read number of relocations");
    return make_error_code(lief_errors::read_error);
  }
  auto res_rels_offset = rel_stream->read_sleb128();
  if (!res_rels_offset) {
    LIEF_ERR("Can't read offset");
    return make_error_code(lief_errors::read_error);
  }

  uint64_t nb_relocs = *res_nb_relocs;
  uint64_t r_offset = *res_rels_offset;
  uint64_t addend = 0;
  const ARCH arch = binary_->header().machine_type();

  LIEF_DEBUG("Nb relocs: {}", nb_relocs);

  while (nb_relocs > 0) {
    auto nb_reloc_group_r = rel_stream->read_sleb128();
    if (!nb_reloc_group_r) {
      break;
    }

    uint64_t nb_reloc_group = *nb_reloc_group_r;
    LIEF_DEBUG("  Nb relocs in group: {}", nb_reloc_group);

    if (nb_reloc_group > nb_relocs) {
      break;
    }

    nb_relocs -= nb_reloc_group;

    auto group_flag_r = rel_stream->read_sleb128();
    if (!group_flag_r) {
      LIEF_ERR("Can't read group flag");
      break;
    }
    uint64_t group_flag = *group_flag_r;

    const bool g_by_info         = group_flag & GROUPED_BY_INFO_FLAG;
    const bool g_by_offset_delta = group_flag & GROUPED_BY_OFFSET_DELTA_FLAG;
    const bool g_by_addend       = group_flag & GROUPED_BY_ADDEND_FLAG;
    const bool g_has_addend      = group_flag & GROUP_HAS_ADDEND_FLAG;

    uint64_t group_off_delta =
      g_by_offset_delta ? rel_stream->read_sleb128().value_or(0) : 0;

    uint64_t groupr_info =
      g_by_info ? rel_stream->read_sleb128().value_or(0) : 0;

    if (g_by_addend && g_has_addend) {
      addend += rel_stream->read_sleb128().value_or(0);
    }

    if (!g_has_addend) {
      addend = 0;
    }

    for (size_t i = 0; i < nb_reloc_group; ++i) {
      if (!*rel_stream) {
        break;
      }
      r_offset += g_by_offset_delta ? group_off_delta : rel_stream->read_sleb128().value_or(0);
      uint64_t info = g_by_info ? groupr_info : rel_stream->read_sleb128().value_or(0);
      if (g_has_addend && !g_by_addend) {
        addend += rel_stream->read_sleb128().value_or(0);
      }

      Elf_Rela R;
      R.r_info = info;
      R.r_addend = addend;
      R.r_offset = r_offset;
      auto reloc = std::unique_ptr<Relocation>(new Relocation(R, Relocation::PURPOSE::DYNAMIC,
        Relocation::ENCODING::ANDROID_SLEB, arch));
      bind_symbol(*reloc);
      insert_relocation(std::move(reloc));
    }
  }
  return ok();
}

template<typename ELF_T>
ok_error_t Parser::parse_relative_relocations(uint64_t offset, uint64_t size) {
  LIEF_DEBUG("Parsing relative relocations");
  using Elf_Relr = typename ELF_T::uint;
  using Elf_Addr = typename ELF_T::uint;
  ScopedStream rel_stream(*stream_, offset);

  Elf_Addr base = 0;
  const ARCH arch = binary_->header().machine_type();
  Relocation::TYPE type = Relocation::TYPE::UNKNOWN;
  switch (arch) {
    case ARCH::AARCH64:
      type = Relocation::TYPE::AARCH64_RELATIVE; break;
    case ARCH::X86_64:
      type = Relocation::TYPE::X86_64_RELATIVE; break;
    case ARCH::ARM:
      type = Relocation::TYPE::ARM_RELATIVE; break;
    case ARCH::HEXAGON:
      type = Relocation::TYPE::HEX_RELATIVE; break;
    case ARCH::PPC64:
      type = Relocation::TYPE::PPC64_RELATIVE; break;
    case ARCH::PPC:
      type = Relocation::TYPE::PPC_RELATIVE; break;
    case ARCH::I386:
    case ARCH::IAMCU:
      type = Relocation::TYPE::X86_RELATIVE; break;
    default:
      break;
  }

  while (rel_stream->pos() < (offset + size)) {
    auto opt_relr = rel_stream->read<Elf_Relr>();
    if (!opt_relr) {
      break;
    }
    Elf_Relr rel = *opt_relr;
    if ((rel & 1) == 0) {
      Elf_Addr r_offset = rel;
      auto reloc = std::make_unique<Relocation>(r_offset, type,
                                                Relocation::ENCODING::RELR);
      reloc->purpose(Relocation::PURPOSE::DYNAMIC);
      insert_relocation(std::move(reloc));
      base = rel + sizeof(Elf_Addr);
    } else {
      for (Elf_Addr offset = base; (rel >>= 1) != 0; offset += sizeof(Elf_Addr)) {
        if ((rel & 1) != 0) {
          Elf_Addr r_offset = offset;
          auto reloc = std::make_unique<Relocation>(r_offset, type,
                                                    Relocation::ENCODING::RELR);
          reloc->purpose(Relocation::PURPOSE::DYNAMIC);
          insert_relocation(std::move(reloc));
        }
      }
      base += (8 * sizeof(Elf_Relr) - 1) * sizeof(Elf_Addr);
    }
  }
  return ok();
}


template<typename ELF_T, typename REL_T>
ok_error_t Parser::parse_dynamic_relocations(uint64_t relocations_offset, uint64_t size) {
  static_assert(std::is_same_v<REL_T, typename ELF_T::Elf_Rel> ||
                std::is_same_v<REL_T, typename ELF_T::Elf_Rela>, "REL_T must be Elf_Rel || Elf_Rela");
  LIEF_DEBUG("== Parsing dynamic relocations ==");

  // Already parsed
  if (binary_->dynamic_relocations().size() > 0) {
    return ok();
  }

  auto nb_entries = static_cast<uint32_t>(size / sizeof(REL_T));

  nb_entries = std::min<uint32_t>(nb_entries, Parser::NB_MAX_RELOCATIONS);
  binary_->relocations_.reserve(nb_entries);

  stream_->setpos(relocations_offset);
  const ARCH arch = binary_->header().machine_type();
  const Relocation::ENCODING enc =
    std::is_same_v<REL_T, typename ELF_T::Elf_Rel> ? Relocation::ENCODING::REL :
                                                     Relocation::ENCODING::RELA;

  for (uint32_t i = 0; i < nb_entries; ++i) {
    const auto raw_reloc = stream_->read<REL_T>();
    if (!raw_reloc) {
      break;
    }

    auto reloc = std::unique_ptr<Relocation>(new Relocation(
        std::move(*raw_reloc), Relocation::PURPOSE::DYNAMIC, enc, arch));
    bind_symbol(*reloc);
    insert_relocation(std::move(reloc));
  }
  return ok();
} // build_dynamic_reclocations

template<typename ELF_T>
ok_error_t Parser::parse_symtab_symbols(uint64_t offset, uint32_t nb_symbols,
                                        const Section& string_section) {
  using Elf_Sym = typename ELF_T::Elf_Sym;
  static constexpr size_t MAX_RESERVED_SYMBOLS = 10000;
  LIEF_DEBUG("== Parsing symtab symbols ==");

  size_t nb_reserved = std::min<size_t>(nb_symbols, MAX_RESERVED_SYMBOLS);
  binary_->symtab_symbols_.reserve(nb_reserved);

  stream_->setpos(offset);
  const ARCH arch = binary_->header().machine_type();
  for (uint32_t i = 0; i < nb_symbols; ++i) {
    const auto raw_sym = stream_->read<Elf_Sym>();
    if (!raw_sym) {
      break;
    }
    auto symbol = std::unique_ptr<Symbol>(new Symbol(std::move(*raw_sym), arch));
    const auto name_offset = string_section.file_offset() + raw_sym->st_name;

    if (auto symbol_name = stream_->peek_string_at(name_offset)) {
      symbol->name(std::move(*symbol_name));
    } else {
      LIEF_ERR("Can't read the symbol's name for symbol #{}", i);
    }
    link_symbol_section(*symbol);
    binary_->symtab_symbols_.push_back(std::move(symbol));
  }
  return ok();
}

template<typename ELF_T>
ok_error_t Parser::parse_dynamic_symbols(uint64_t offset) {
  using Elf_Sym = typename ELF_T::Elf_Sym;
  using Elf_Off = typename ELF_T::Elf_Off;
  static constexpr size_t MAX_RESERVED_SYMBOLS = 10000;

  LIEF_DEBUG("== Parsing dynamics symbols ==");

  auto res = get_numberof_dynamic_symbols<ELF_T>(config_.count_mtd);
  if (!res) {
    LIEF_ERR("Fail to get the number of dynamic symbols with the current counting method");
    return make_error_code(lief_errors::parsing_error);
  }

  const uint32_t nb_symbols = res.value();

  const Elf_Off dynamic_symbols_offset = offset;
  const Elf_Off string_offset          = get_dynamic_string_table();

  LIEF_DEBUG("    - Number of symbols counted: {:d}", nb_symbols);
  LIEF_DEBUG("    - Table Offset:              0x{:x}", dynamic_symbols_offset);
  LIEF_DEBUG("    - String Table Offset:       0x{:x}", string_offset);

  if (string_offset == 0) {
    LIEF_WARN("Unable to find the .dynstr section");
    return make_error_code(lief_errors::parsing_error);
  }

  size_t nb_reserved = std::min<size_t>(nb_symbols, MAX_RESERVED_SYMBOLS);
  binary_->dynamic_symbols_.reserve(nb_reserved);
  stream_->setpos(dynamic_symbols_offset);

  for (size_t i = 0; i < nb_symbols; ++i) {
    const auto symbol_header = stream_->read<Elf_Sym>();
    if (!symbol_header) {
      LIEF_DEBUG("Break on symbol #{:d}", i);
      break;
    }
    auto symbol = std::unique_ptr<Symbol>(new Symbol(std::move(*symbol_header),
                                           binary_->header().machine_type()));

    if (symbol_header->st_name > 0) {
      auto name = stream_->peek_string_at(string_offset + symbol_header->st_name);
      if (!name) {
        break;
      }

      if (name->empty() && i > 0) {
        LIEF_DEBUG("Symbol's name #{:d} is empty!", i);
      }

      symbol->name(std::move(*name));
    }
    link_symbol_section(*symbol);
    binary_->dynamic_symbols_.push_back(std::move(symbol));
  }
  binary_->sizing_info_->dynsym = binary_->dynamic_symbols_.size() * sizeof(Elf_Sym);
  if (const auto* dt_strsz = binary_->get(DynamicEntry::TAG::STRSZ)) {
    binary_->sizing_info_->dynstr = dt_strsz->value();
  }
  return ok();
}


template<typename ELF_T>
ok_error_t Parser::parse_dynamic_entries(BinaryStream& stream) {
  using Elf_Dyn  = typename ELF_T::Elf_Dyn;
  using uint__   = typename ELF_T::uint;
  using Elf_Addr = typename ELF_T::Elf_Addr;
  using Elf_Off  = typename ELF_T::Elf_Off;

  LIEF_DEBUG("Parsing dynamic entries");

  uint32_t max_nb_entries = stream.size() / sizeof(Elf_Dyn);
  max_nb_entries = std::min<uint32_t>(max_nb_entries, Parser::NB_MAX_DYNAMIC_ENTRIES);

  Elf_Off dynamic_string_offset = get_dynamic_string_table(&stream);

  bool end_of_dynamic = false;
  while (stream) {
    const auto res_entry = stream.read<Elf_Dyn>();
    if (!res_entry) {
      break;
    }
    const auto entry = *res_entry;

    std::unique_ptr<DynamicEntry> dynamic_entry;

    const ARCH arch = binary_->header().machine_type();

    switch (DynamicEntry::from_value(entry.d_tag, arch)) {
      case DynamicEntry::TAG::NEEDED :
        {
          dynamic_entry = std::make_unique<DynamicEntryLibrary>(entry, arch);
          auto library_name = stream_->peek_string_at(dynamic_string_offset + dynamic_entry->value());
          if (!library_name) {
            LIEF_ERR("Can't read library name for DT_NEEDED entry");
            break;
          }
          dynamic_entry->as<DynamicEntryLibrary>()->name(std::move(*library_name));
          break;
        }

      case DynamicEntry::TAG::SONAME :
        {
          dynamic_entry = std::make_unique<DynamicSharedObject>(entry, arch);
          auto sharename = stream_->peek_string_at(dynamic_string_offset + dynamic_entry->value());
          if (!sharename) {
            LIEF_ERR("Can't read library name for DT_SONAME entry");
            break;
          }
          dynamic_entry->as<DynamicSharedObject>()->name(std::move(*sharename));
          break;
        }

      case DynamicEntry::TAG::RPATH:
        {
          dynamic_entry = std::make_unique<DynamicEntryRpath>(entry, arch);
          auto name = stream_->peek_string_at(dynamic_string_offset + dynamic_entry->value());
          if (!name) {
            LIEF_ERR("Can't read rpath string value for DT_RPATH");
            break;
          }
          dynamic_entry->as<DynamicEntryRpath>()->rpath(std::move(*name));
          break;
        }

      case DynamicEntry::TAG::RUNPATH:
        {
          dynamic_entry = std::make_unique<DynamicEntryRunPath>(entry, arch);
          auto name = stream_->peek_string_at(dynamic_string_offset + dynamic_entry->value());
          if (!name) {
            LIEF_ERR("Can't read runpath string value for DT_RUNPATH");
            break;
          }
          dynamic_entry->as<DynamicEntryRunPath>()->runpath(std::move(*name));
          break;
        }

      case DynamicEntry::TAG::FLAGS_1:
      case DynamicEntry::TAG::FLAGS:
        {
          dynamic_entry = std::make_unique<DynamicEntryFlags>(entry, arch);
          break;
        }

      case DynamicEntry::TAG::SYMTAB:
      case DynamicEntry::TAG::SYMENT:
      case DynamicEntry::TAG::RELA:
      case DynamicEntry::TAG::RELASZ:
      case DynamicEntry::TAG::REL:
      case DynamicEntry::TAG::RELSZ:
      case DynamicEntry::TAG::JMPREL:
      case DynamicEntry::TAG::PLTRELSZ:
      case DynamicEntry::TAG::PLTREL:
      case DynamicEntry::TAG::VERSYM:
      case DynamicEntry::TAG::VERNEED:
      case DynamicEntry::TAG::VERNEEDNUM:
      case DynamicEntry::TAG::VERDEF:
      case DynamicEntry::TAG::VERDEFNUM:
        {
          dynamic_entry = std::make_unique<DynamicEntry>(entry, arch);
          break;
        }

      case DynamicEntry::TAG::FINI_ARRAY:
      case DynamicEntry::TAG::INIT_ARRAY:
      case DynamicEntry::TAG::PREINIT_ARRAY:
        {
          dynamic_entry = std::make_unique<DynamicEntryArray>(entry, arch);
          break;
        }

      case DynamicEntry::TAG::DT_NULL_:
        {
          dynamic_entry = std::make_unique<DynamicEntry>(entry, arch);
          end_of_dynamic = true;
          break;
        }

      default:
        {
          dynamic_entry = std::make_unique<DynamicEntry>(entry, arch);
        }
    }

    if (dynamic_entry != nullptr) {
      binary_->dynamic_entries_.push_back(std::move(dynamic_entry));
    } else {
      LIEF_WARN("dynamic_entry is nullptr !");
    }

    if (end_of_dynamic) {
      break;
    }
  }

  // Check for INIT array
  // ====================
  if (DynamicEntry* dt_init_array = binary_->get(DynamicEntry::TAG::INIT_ARRAY)) {
    if (DynamicEntry* dt_init_arraysz = binary_->get(DynamicEntry::TAG::INIT_ARRAYSZ)) {
      binary_->sizing_info_->init_array = dt_init_arraysz->value();
      std::vector<uint64_t>& array = dt_init_array->as<DynamicEntryArray>()->array();
      const auto nb_functions = static_cast<uint32_t>(dt_init_arraysz->value() / sizeof(uint__));
      if (auto offset = binary_->virtual_address_to_offset(dt_init_array->value())) {
        stream_->setpos(*offset);
        for (size_t i = 0; i < nb_functions; ++i) {
          if (auto val = stream_->read<Elf_Addr>()) {
            array.push_back(*val);
          } else {
            break;
          }
        }
      }
    } else {
      LIEF_WARN("The binary is not consistent. Found DT_INIT_ARRAY but missing DT_INIT_ARRAYSZ");
    }
  }


  // Check for FINI array
  // ====================
  if (DynamicEntry* dt_fini_array = binary_->get(DynamicEntry::TAG::FINI_ARRAY)) {
    if (DynamicEntry* dt_fini_arraysz = binary_->get(DynamicEntry::TAG::FINI_ARRAYSZ)) {
      binary_->sizing_info_->fini_array = dt_fini_arraysz->value();
      std::vector<uint64_t>& array = dt_fini_array->as<DynamicEntryArray>()->array();

      const auto nb_functions = static_cast<uint32_t>(dt_fini_arraysz->value() / sizeof(uint__));

      if (auto offset = binary_->virtual_address_to_offset(dt_fini_array->value())) {
        stream_->setpos(*offset);
        for (size_t i = 0; i < nb_functions; ++i) {
          if (auto val = stream_->read<Elf_Addr>()) {
            array.push_back(*val);
          } else {
            break;
          }
        }
      }
    } else {
      LIEF_WARN("The binary is not consistent. Found DT_FINI_ARRAY but missing DT_FINI_ARRAYSZ");
    }
  }

  // Check for PREINIT array
  // =======================
  if (DynamicEntry* dt_preini_array = binary_->get(DynamicEntry::TAG::PREINIT_ARRAY)) {
    if (DynamicEntry* dt_preinit_arraysz = binary_->get(DynamicEntry::TAG::PREINIT_ARRAYSZ)) {
      binary_->sizing_info_->preinit_array = dt_preinit_arraysz->value();
      std::vector<uint64_t>& array = dt_preini_array->as<DynamicEntryArray>()->array();

      const auto nb_functions = static_cast<uint32_t>(dt_preinit_arraysz->value() / sizeof(uint__));

      if (auto offset = binary_->virtual_address_to_offset(dt_preini_array->value())) {
        stream_->setpos(static_cast<Elf_Off>(*offset));
        for (size_t i = 0; i < nb_functions; ++i) {
          if (auto val = stream_->read<Elf_Addr>()) {
            array.push_back(*val);
          } else {
            break;
          }
        }
      }
    } else {
      LIEF_WARN("The binary is not consistent. Found DT_PREINIT_ARRAY but missing DT_PREINIT_ARRAYSZ");
    }
  }
  return ok();
}


template<typename ELF_T, typename REL_T>
ok_error_t Parser::parse_pltgot_relocations(uint64_t offset, uint64_t size) {
  static_assert(std::is_same<REL_T, typename ELF_T::Elf_Rel>::value ||
                std::is_same<REL_T, typename ELF_T::Elf_Rela>::value, "REL_T must be Elf_Rel or Elf_Rela");
  using Elf_Off  = typename ELF_T::Elf_Off;

  // Already Parsed
  if (binary_->pltgot_relocations().size() > 0) {
    return ok();
  }

  const Elf_Off offset_relocations = offset;

  auto nb_entries = static_cast<uint32_t>(size / sizeof(REL_T));

  nb_entries = std::min<uint32_t>(nb_entries, Parser::NB_MAX_RELOCATIONS);

  const ARCH arch = binary_->header_.machine_type();
  const Relocation::ENCODING enc =
    std::is_same_v<REL_T, typename ELF_T::Elf_Rel> ? Relocation::ENCODING::REL :
                                                     Relocation::ENCODING::RELA;
  stream_->setpos(offset_relocations);
  for (uint32_t i = 0; i < nb_entries; ++i) {
    const auto rel_hdr = stream_->read<REL_T>();
    if (!rel_hdr) {
      break;
    }

    auto reloc = std::unique_ptr<Relocation>(new Relocation(
        std::move(*rel_hdr), Relocation::PURPOSE::PLTGOT, enc, arch));
    bind_symbol(*reloc);
    insert_relocation(std::move(reloc));
  }
  return ok();
}

struct RelocationSetEq {
  bool operator()(const Relocation* lhs, const Relocation* rhs) const {
    bool check = lhs->address() == rhs->address() &&
                 lhs->type()    == rhs->type()    &&
                 lhs->addend()  == rhs->addend()  &&
                 lhs->info()    == rhs->info()    &&
                 lhs->has_symbol() == rhs->has_symbol();

    if (!check) {
      return false;
    }

    if (lhs->has_symbol()) { // The fact that rhs->has_symbol is checked previously
      return lhs->symbol()->name() == rhs->symbol()->name();
    }
    return check;
  }
};

struct RelocationSetHash {
  size_t operator()(const Relocation* reloc) const {
    Hash hasher;
    hasher.process(reloc->address())
          .process(reloc->type())
          .process(reloc->info())
          .process(reloc->addend());

    const Symbol* sym = reloc->symbol();
    if (sym != nullptr) {
      hasher.process(sym->name());
    }
    return hasher.value();
  }
};

template<typename ELF_T, typename REL_T>
ok_error_t Parser::parse_section_relocations(const Section& section) {
  using Elf_Rel = typename ELF_T::Elf_Rel;
  using Elf_Rela = typename ELF_T::Elf_Rela;

  static_assert(std::is_same<REL_T, Elf_Rel>::value ||
                std::is_same<REL_T, Elf_Rela>::value, "REL_T must be Elf_Rel || Elf_Rela");

  // A relocation section can reference two other sections: a symbol table,
  // identified by the sh_link section header entry, and a section to modify,
  // identified by the sh_info
  // See Figure 4-12 in https://refspecs.linuxbase.org/elf/gabi4+/ch4.sheader.html#sh_link
  Section* applies_to = nullptr;
  const size_t sh_info = section.information();
  if (sh_info > 0 && sh_info < binary_->sections_.size()) {
    applies_to = binary_->sections_[sh_info].get();
  }

  Section* symbol_table = nullptr;
  if (section.link() > 0 && section.link() < binary_->sections_.size()) {
    const size_t sh_link = section.link();
    symbol_table = binary_->sections_[sh_link].get();
  }

  constexpr uint8_t shift = ELF_T::r_info_shift;

  const ARCH arch = binary_->header_.machine_type();

  constexpr Relocation::ENCODING enc =
    std::is_same_v<REL_T, typename ELF_T::Elf_Rel> ? Relocation::ENCODING::REL :
                                                     Relocation::ENCODING::RELA;

  auto nb_entries = static_cast<uint32_t>(section.size() / sizeof(REL_T));
  nb_entries = std::min<uint32_t>(nb_entries, Parser::NB_MAX_RELOCATIONS);

  std::unordered_set<Relocation*, RelocationSetHash, RelocationSetEq> reloc_hash;

  SpanStream reloc_stream(section.content());
  const bool is_object_file =
    binary_->header().file_type() == Header::FILE_TYPE::REL &&
    binary_->segments_.empty();

  size_t count = 0;
  while (reloc_stream) {
    auto rel_hdr = reloc_stream.read<REL_T>();
    if (!rel_hdr) {
      LIEF_WARN("Can't parse relocation at offset: 0x{:04x} in {}",
                reloc_stream.pos(), section.name());
      break;
    }
    auto reloc = std::unique_ptr<Relocation>(new Relocation(
      *rel_hdr, Relocation::PURPOSE::NONE, enc, arch));

    reloc->section_      = applies_to;
    reloc->symbol_table_ = symbol_table;

    if (is_object_file) {
      reloc->purpose(Relocation::PURPOSE::OBJECT);
    }

    const auto idx  = static_cast<uint32_t>(rel_hdr->r_info >> shift);

    const bool is_from_dynsym = idx > 0 && idx < binary_->dynamic_symbols_.size() &&
               (symbol_table == nullptr ||
                symbol_table->type() == Section::TYPE::DYNSYM);

    const bool is_from_symtab = idx < binary_->symtab_symbols_.size() &&
                                (symbol_table == nullptr ||
                                 symbol_table->type() == Section::TYPE::SYMTAB);
    if (is_from_dynsym) {
      reloc->symbol_ = binary_->dynamic_symbols_[idx].get();
    } else if (is_from_symtab) {
      reloc->symbol_ = binary_->symtab_symbols_[idx].get();
    }

    if (reloc_hash.insert(reloc.get()).second) {
      ++count;
      insert_relocation(std::move(reloc));
    }
  }
  LIEF_DEBUG("#{} relocations found in {}", count, section.name());
  return ok();
}

template<typename ELF_T>
ok_error_t Parser::parse_symbol_version_requirement(uint64_t offset, uint32_t nb_entries) {
  using Elf_Verneed = typename ELF_T::Elf_Verneed;
  using Elf_Vernaux = typename ELF_T::Elf_Vernaux;

  LIEF_DEBUG("== Parser Symbol version requirement ==");

  const uint64_t svr_offset = offset;

  LIEF_DEBUG("svr offset: 0x{:x}", svr_offset);

  const uint64_t string_offset = get_dynamic_string_table();

  uint32_t next_symbol_offset = 0;

  for (size_t sym_idx = 0; sym_idx < nb_entries; ++sym_idx) {
    const auto header = stream_->peek<Elf_Verneed>(svr_offset + next_symbol_offset);
    if (!header) {
      break;
    }

    auto symbol_version_requirement = std::make_unique<SymbolVersionRequirement>(*header);
    if (string_offset != 0) {
      auto name = stream_->peek_string_at(string_offset + header->vn_file);
      if (name) {
        symbol_version_requirement->name(std::move(*name));
      }
    }

    const uint32_t nb_symbol_aux = header->vn_cnt;

    if (nb_symbol_aux > 0 && header->vn_aux > 0) {
      uint32_t next_aux_offset = 0;
      for (size_t j = 0; j < nb_symbol_aux; ++j) {
        const uint64_t aux_hdr_off = svr_offset + next_symbol_offset +
                                     header->vn_aux + next_aux_offset;

        const auto aux_header = stream_->peek<Elf_Vernaux>(aux_hdr_off);
        if (!aux_header) {
          break;
        }

        auto svar = std::make_unique<SymbolVersionAuxRequirement>(*aux_header);
        if (string_offset != 0) {
          auto name = stream_->peek_string_at(string_offset + aux_header->vna_name);
          if (name) {
            svar->name(std::move(*name));
          }
        }

        symbol_version_requirement->aux_requirements_.push_back(std::move(svar));
        if (aux_header->vna_next == 0) {
          break;
        }
        next_aux_offset += aux_header->vna_next;
      }

      binary_->symbol_version_requirements_.push_back(std::move(symbol_version_requirement));
    }

    if (header->vn_next == 0) {
      break;
    }
    next_symbol_offset += header->vn_next;
  }


  // Associate Symbol Version with auxiliary symbol
  // Symbol version requirement is used to map
  // SymbolVersion::SymbolVersionAux <------> SymbolVersionAuxRequirement
  //
  // We mask the 15th (7FFF) bit because it sets if this symbol is a hidden on or not
  // but we don't care
  for (const std::unique_ptr<SymbolVersionRequirement>& svr : binary_->symbol_version_requirements_) {
    binary_->sizing_info_->verneed += sizeof(Elf_Verneed);
    for (std::unique_ptr<SymbolVersionAuxRequirement>& svar : svr->aux_requirements_) {
        binary_->sizing_info_->verneed += sizeof(Elf_Vernaux);
        for (const std::unique_ptr<SymbolVersion>& sv : binary_->symbol_version_table_) {
          if ((sv->value() & 0x7FFF) == svar->other()) {
            sv->symbol_aux_ = svar.get();
          }
        }
    }
  }
  return ok();
}

template<typename ELF_T>
ok_error_t Parser::parse_symbol_version_definition(uint64_t offset, uint32_t nb_entries) {
  using Elf_Verdef  = typename ELF_T::Elf_Verdef;
  using Elf_Verdaux = typename ELF_T::Elf_Verdaux;

  const uint64_t string_offset = get_dynamic_string_table();
  ScopedStream verdef_stream(*stream_, offset);
  uint64_t def_size = 0;

  for (size_t i = 0; i < nb_entries; ++i) {
    const auto svd_header = verdef_stream->peek<Elf_Verdef>();
    def_size = std::max(def_size, verdef_stream->pos() - offset + sizeof(Elf_Verdef));
    if (!svd_header) {
      break;
    }

    auto symbol_version_definition = std::make_unique<SymbolVersionDefinition>(*svd_header);
    uint32_t nb_aux_symbols = svd_header->vd_cnt;
    {
      ScopedStream aux_stream(*stream_, verdef_stream->pos() + svd_header->vd_aux);
      for (size_t j = 0; j < nb_aux_symbols; ++j) {
        const auto svda_header = aux_stream->peek<Elf_Verdaux>();
        def_size = std::max(def_size, aux_stream->pos() - offset + sizeof(Elf_Verdaux));
        if (!svda_header) {
          break;
        }

        if (string_offset != 0) {
          auto name  = stream_->peek_string_at(string_offset + svda_header->vda_name);
          if (name) {
            symbol_version_definition->symbol_version_aux_.emplace_back(new SymbolVersionAux{std::move(*name)});
          }
        }

        // Additional check
        if (svda_header->vda_next == 0) {
          break;
        }
        aux_stream->increment_pos(svda_header->vda_next);
      }
    }

    binary_->symbol_version_definition_.push_back(std::move(symbol_version_definition));

    // Additional check
    if (svd_header->vd_next == 0) {
      break;
    }
    verdef_stream->increment_pos(svd_header->vd_next);
  }

  binary_->sizing_info_->verdef = def_size;

  // Associate Symbol Version with auxiliary symbol
  // We mask the 15th bit because it sets if this symbol is a hidden on or not
  // but we don't care
  for (std::unique_ptr<SymbolVersionDefinition>& svd : binary_->symbol_version_definition_) {
    for (std::unique_ptr<SymbolVersionAux>& sva : svd->symbol_version_aux_) {
      for (std::unique_ptr<SymbolVersion>& sv : binary_->symbol_version_table_) {
        if (svd->ndx() > 1 && (sv->value() & 0x7FFF) == svd->ndx() && !sv->symbol_aux_) {
          sv->symbol_aux_ = sva.get();
        }
      }
    }
  }
  return ok();
}

}
}
