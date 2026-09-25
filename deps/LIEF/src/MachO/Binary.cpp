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
#include <algorithm>

#include "logging.hpp"

#include "Object.tcc"
#include "Binary.tcc"
#include "paging.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/utils.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/BuildVersion.hpp"
#include "LIEF/MachO/Builder.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/CodeSignature.hpp"
#include "LIEF/MachO/CodeSignatureDir.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldEnvironment.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/DylinkerCommand.hpp"
#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "LIEF/MachO/EncryptionInfo.hpp"
#include "LIEF/MachO/ExportInfo.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/FunctionVariants.hpp"
#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/AtomInfo.hpp"
#include "LIEF/MachO/IndirectBindingInfo.hpp"
#include "LIEF/MachO/LinkEdit.hpp"
#include "LIEF/MachO/LinkerOptHint.hpp"
#include "LIEF/MachO/MainCommand.hpp"
#include "LIEF/MachO/NoteCommand.hpp"
#include "LIEF/MachO/RPathCommand.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/Routine.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/SourceVersion.hpp"
#include "LIEF/MachO/SubClient.hpp"
#include "LIEF/MachO/SubFramework.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/ThreadCommand.hpp"
#include "LIEF/MachO/TwoLevelHints.hpp"
#include "LIEF/MachO/UUIDCommand.hpp"
#include "LIEF/MachO/VersionMin.hpp"
#include "MachO/Structures.hpp"

#include "internal_utils.hpp"

namespace LIEF {
namespace MachO {

bool Binary::KeyCmp::operator() (const Relocation* lhs, const Relocation* rhs) const {
  return *lhs < *rhs;
}

Binary::Binary() :
  LIEF::Binary(LIEF::Binary::FORMATS::MACHO)
{}

LIEF::Binary::sections_t Binary::get_abstract_sections() {
  LIEF::Binary::sections_t result;
  it_sections sections = this->sections();
  std::transform(std::begin(sections), std::end(sections),
                 std::back_inserter(result),
                 [] (Section& s) { return &s; });

  return result;
}
// LIEF Interface
// ==============

void Binary::patch_address(uint64_t address,
                           const std::vector<uint8_t>& patch_value,
                           LIEF::Binary::VA_TYPES)
{
  // Find the segment associated with the virtual address
  SegmentCommand* segment_topatch = segment_from_virtual_address(address);
  if (segment_topatch == nullptr) {
    LIEF_ERR("Unable to find segment associated with address: 0x{:x}", address);
    return;
  }
  const uint64_t offset = address - segment_topatch->virtual_address();
  span<uint8_t> content = segment_topatch->writable_content();
  if (offset > content.size() || (offset + patch_value.size()) > content.size()) {
    LIEF_ERR("The patch value ({} bytes @0x{:x}) is out of bounds of the segment (limit: 0x{:x})",
             patch_value.size(), offset, content.size());
    return;
  }
  std::move(std::begin(patch_value), std::end(patch_value), content.data() + offset);
}

void Binary::patch_address(uint64_t address, uint64_t patch_value, size_t size, LIEF::Binary::VA_TYPES) {
  if (size > sizeof(patch_value)) {
    LIEF_ERR("Invalid size: 0x{:x}", size);
    return;
  }

  SegmentCommand* segment_topatch = segment_from_virtual_address(address);

  if (segment_topatch == nullptr) {
    LIEF_ERR("Unable to find segment associated with address: 0x{:x}", address);
    return;
  }
  const uint64_t offset = address - segment_topatch->virtual_address();
  span<uint8_t> content = segment_topatch->writable_content();

  if (offset > content.size() || (offset + size) > content.size()) {
    LIEF_ERR("The patch value ({} bytes @0x{:x}) is out of bounds of the segment (limit: 0x{:x})",
             size, offset, content.size());
    return;
  }

  switch (size) {
    case sizeof(uint8_t):
      {
        auto X = static_cast<uint8_t>(patch_value);
        memcpy(content.data() + offset, &X, sizeof(uint8_t));
        break;
      }

    case sizeof(uint16_t):
      {
        auto X = static_cast<uint16_t>(patch_value);
        memcpy(content.data() + offset, &X, sizeof(uint16_t));
        break;
      }

    case sizeof(uint32_t):
      {
        auto X = static_cast<uint32_t>(patch_value);
        memcpy(content.data() + offset, &X, sizeof(uint32_t));
        break;
      }

    case sizeof(uint64_t):
      {
        auto X = static_cast<uint64_t>(patch_value);
        memcpy(content.data() + offset, &X, sizeof(uint64_t));
        break;
      }

    default:
      {
        LIEF_ERR("The provided size ({}) does not match the size of an integer", size);
        return;
      }
  }
}

span<const uint8_t> Binary::get_content_from_virtual_address(
    uint64_t virtual_address, uint64_t size, LIEF::Binary::VA_TYPES) const
{
  const SegmentCommand* segment = segment_from_virtual_address(virtual_address);

  if (segment == nullptr) {
    LIEF_ERR("Unable to find segment associated with address: 0x{:x}", virtual_address);
    return {};
  }

  span<const uint8_t> content = segment->content();
  const uint64_t offset = virtual_address - segment->virtual_address();

  uint64_t checked_size = size;
  if (offset > content.size() || (offset + checked_size) > content.size()) {
    checked_size = checked_size - (offset + checked_size - content.size());
  }

  return {content.data() + offset, static_cast<size_t>(checked_size)};
}


uint64_t Binary::entrypoint() const {
  if (const MainCommand* cmd = main_command()) {
    return imagebase() + cmd->entrypoint();
  }

  if (const ThreadCommand* cmd = thread_command()) {
    auto range = va_ranges();
    const uint64_t pc = cmd->pc();
    if (range.start <= pc && pc < range.end) {
      return pc;
    }
    return imagebase() + cmd->pc();
  }

  LIEF_WARN("Can't find LC_MAIN nor LC_THREAD/LC_UNIXTHREAD");
  return 0;
}

LIEF::Binary::symbols_t Binary::get_abstract_symbols() {
  LIEF::Binary::symbols_t syms;
  syms.reserve(symbols_.size());
  std::transform(std::begin(symbols_), std::end(symbols_),
                 std::back_inserter(syms),
                 [] (const std::unique_ptr<Symbol>& s) {
                   return s.get();
                 });
  return syms;
}


LIEF::Binary::functions_t Binary::get_abstract_exported_functions() const {
  LIEF::Binary::functions_t result;
  it_const_exported_symbols syms = exported_symbols();
  std::transform(std::begin(syms), std::end(syms), std::back_inserter(result),
    [] (const Symbol& s) {
      return Function(s.name(), s.value(), Function::FLAGS::EXPORTED);
    }
  );
  return result;
}

LIEF::Binary::functions_t Binary::get_abstract_imported_functions() const {
  LIEF::Binary::functions_t result;
  it_const_imported_symbols syms = imported_symbols();
  std::transform(std::begin(syms), std::end(syms), std::back_inserter(result),
    [] (const Symbol& s) {
      return Function(s.name(), s.value(), Function::FLAGS::IMPORTED);
    }
  );
  return result;
}


std::vector<std::string> Binary::get_abstract_imported_libraries() const {
  std::vector<std::string> result;
  for (const DylibCommand& lib : libraries()) {
    result.push_back(lib.name());
  }
  return result;
}

// Relocations
Binary::it_relocations Binary::relocations() {
  relocations_t result;
  for (SegmentCommand* segment : segments_) {
    std::transform(std::begin(segment->relocations_), std::end(segment->relocations_),
                   std::inserter(result, std::begin(result)),
                   [] (const std::unique_ptr<Relocation>& r) {
                     return r.get();
                   });
  }

  for (Section* section : sections_) {
    std::transform(std::begin(section->relocations_), std::end(section->relocations_),
                   std::inserter(result, std::begin(result)),
                   [] (const std::unique_ptr<Relocation>& r) {
                     return r.get();
                   });
  }

  relocations_ = std::move(result);
  return relocations_;
}

Binary::it_const_relocations Binary::relocations() const {
  relocations_t result;
  for (const SegmentCommand* segment : segments_) {
    std::transform(std::begin(segment->relocations_), std::end(segment->relocations_),
                   std::inserter(result, std::begin(result)),
                   [] (const std::unique_ptr<Relocation>& r) {
                     return r.get();
                   });
  }

  for (const Section* section : sections_) {
    std::transform(std::begin(section->relocations_), std::end(section->relocations_),
                   std::inserter(result, std::begin(result)),
                   [] (const std::unique_ptr<Relocation>& r) {
                     return r.get();
                   });
  }

  relocations_ = std::move(result);
  return relocations_;
}


LIEF::Binary::relocations_t Binary::get_abstract_relocations() {
  LIEF::Binary::relocations_t relocations;
  it_relocations macho_relocations = this->relocations();
  relocations.reserve(macho_relocations.size());

  for (Relocation& r : macho_relocations) {
    relocations.push_back(&r);
  }

  return relocations;
}


// Symbols
// =======

bool Binary::is_exported(const Symbol& symbol) {
  return !symbol.is_external() && symbol.has_export_info();
}


bool Binary::is_imported(const Symbol& symbol) {
  return symbol.is_external() && !symbol.has_export_info();
}

const Symbol* Binary::get_symbol(const std::string& name) const {
  const auto it_symbol = std::find_if(
      std::begin(symbols_), std::end(symbols_),
      [&name] (const std::unique_ptr<Symbol>& sym) {
        return sym->name() == name;
      });

  if (it_symbol == std::end(symbols_)) {
    return nullptr;
  }

  return it_symbol->get();
}

void Binary::write(const std::string& filename) {
  Builder::write(*this, filename);
}

void Binary::write(const std::string& filename, Builder::config_t config) {
  Builder::write(*this, filename, config);
}

void Binary::write(std::ostream& os) {
  Builder::write(*this, os);
}

void Binary::write(std::ostream& os, Builder::config_t config) {
  Builder::write(*this, os, config);
}

const Section* Binary::section_from_offset(uint64_t offset) const {
  const auto it_section = std::find_if(
      sections_.cbegin(), sections_.cend(),
      [offset] (const Section* section) {
        return section->offset() <= offset &&
               offset < (section->offset() + section->size());
      });

  if (it_section == sections_.cend()) {
    return nullptr;
  }

  return *it_section;
}

const Section* Binary::section_from_virtual_address(uint64_t address) const {
  const auto it_section = std::find_if(
      std::begin(sections_), std::end(sections_),
      [address] (const Section* section) {
        return section->virtual_address() <= address &&
               address < (section->virtual_address() + section->size());
      });

  if (it_section == std::end(sections_)) {
    return nullptr;
  }

  return *it_section;
}

const SegmentCommand* Binary::segment_from_virtual_address(uint64_t virtual_address) const {
  auto it_segment = std::find_if(
      std::begin(segments_), std::end(segments_),
      [virtual_address] (const SegmentCommand* segment) {
        return segment->virtual_address() <= virtual_address &&
               virtual_address < (segment->virtual_address() + segment->virtual_size());
      });

  if (it_segment == std::end(segments_)) {
    return nullptr;
  }

  return *it_segment;
}

size_t Binary::segment_index(const SegmentCommand& segment) const {
  return segment.index();
}

const SegmentCommand* Binary::segment_from_offset(uint64_t offset) const {
  if (offset_seg_.empty()) {
    return nullptr;
  }

  const auto it_begin = std::begin(offset_seg_);
  if (offset < it_begin->first) {
    return nullptr;
  }

  auto it = offset_seg_.lower_bound(offset);
  if (it != std::end(offset_seg_) && (it->first == offset || it == it_begin)) {
    SegmentCommand* seg = it->second;
    if (seg->file_offset() <= offset && offset < (seg->file_offset() + seg->file_size())) {
      return seg;
    }
  }

  const auto it_end = offset_seg_.crbegin();
  if (it == std::end(offset_seg_) && offset >= it_end->first) {
    SegmentCommand* seg = it_end->second;
    if (seg->file_offset() <= offset && offset < (seg->file_offset() + seg->file_size())) {
      return seg;
    }
  }

  if (it == it_begin) {
    /* It did not pass the first check */
    return nullptr;
  }

  --it;

  SegmentCommand* seg = it->second;
  if (seg->file_offset() <= offset && offset < (seg->file_offset() + seg->file_size())) {
    return seg;
  }
  return nullptr;
}

ok_error_t Binary::shift_linkedit(size_t width) {
  SegmentCommand* linkedit = get_segment("__LINKEDIT");
  if (linkedit == nullptr) {
    LIEF_INFO("Can't find __LINKEDIT");
    return make_error_code(lief_errors::not_found);
  }
  const uint64_t lnk_offset = linkedit->file_offset();
  /* const uint64_t lnk_size   = linkedit->file_size(); */
  /* const uint64_t lnk_end    = lnk_offset + lnk_size; */

  if (SymbolCommand* sym_cmd = symbol_command()) {
    if (lnk_offset <= sym_cmd->symbol_offset()) {
      sym_cmd->symbol_offset(sym_cmd->symbol_offset() + width);
    }

    if (lnk_offset <= sym_cmd->strings_offset()) {
      sym_cmd->strings_offset(sym_cmd->strings_offset() + width);
    }
  }


  if (DataInCode* data_code_cmd = data_in_code()) {
    if (lnk_offset <= data_code_cmd->data_offset()) {
      data_code_cmd->data_offset(data_code_cmd->data_offset() + width);
    }
  }

  if (CodeSignature* sig = code_signature()) {
    if (lnk_offset <= sig->data_offset()) {
      sig->data_offset(sig->data_offset() + width);
    }
  }

  if (CodeSignatureDir* sig_dir = code_signature_dir()) {
    if (lnk_offset <= sig_dir->data_offset()) {
      sig_dir->data_offset(sig_dir->data_offset() + width);
    }
  }

  if (SegmentSplitInfo* ssi = segment_split_info()) {
    if (lnk_offset <= ssi->data_offset()) {
      ssi->data_offset(ssi->data_offset() + width);
    }
  }

  if (FunctionStarts* fs = function_starts()) {
    if (lnk_offset <= fs->data_offset()) {
      fs->data_offset(fs->data_offset() + width);
    }
  }

  if (DynamicSymbolCommand* dyn_cmd = dynamic_symbol_command()) {
    if (lnk_offset <= dyn_cmd->toc_offset()) {
      dyn_cmd->toc_offset(dyn_cmd->toc_offset() + width);
    }

    if (lnk_offset <= dyn_cmd->module_table_offset()) {
      dyn_cmd->module_table_offset(dyn_cmd->module_table_offset() + width);
    }

    if (lnk_offset <= dyn_cmd->external_reference_symbol_offset()) {
      dyn_cmd->external_reference_symbol_offset(dyn_cmd->external_reference_symbol_offset() + width);
    }

    if (lnk_offset <= dyn_cmd->indirect_symbol_offset()) {
      dyn_cmd->indirect_symbol_offset(dyn_cmd->indirect_symbol_offset() + width);
    }

    if (lnk_offset <= dyn_cmd->external_relocation_offset()) {
      dyn_cmd->external_relocation_offset(dyn_cmd->external_relocation_offset() + width);
    }

    if (lnk_offset <= dyn_cmd->local_relocation_offset()) {
      dyn_cmd->local_relocation_offset(dyn_cmd->local_relocation_offset() + width);
    }
  }

  if (DyldInfo* dyld = dyld_info()) {
    if (lnk_offset <= dyld->rebase().first) {
      dyld->set_rebase_offset(dyld->rebase().first + width);
    }

    if (lnk_offset <= dyld->bind().first) {
      dyld->set_bind_offset(dyld->bind().first + width);
    }

    if (lnk_offset <= dyld->weak_bind().first) {
      dyld->set_weak_bind_offset(dyld->weak_bind().first + width);
    }

    if (lnk_offset <= dyld->lazy_bind().first) {
      dyld->set_lazy_bind_offset(dyld->lazy_bind().first + width);
    }

    if (lnk_offset <= dyld->export_info().first) {
      dyld->set_export_offset(dyld->export_info().first + width);
    }
  }

  if (DyldChainedFixups* fixups = dyld_chained_fixups()) {
    fixups->data_offset(fixups->data_offset() + width);
  }

  if (DyldExportsTrie* exports = dyld_exports_trie()) {
    exports->data_offset(exports->data_offset() + width);
  }

  if (LinkerOptHint* opt = linker_opt_hint()) {
    opt->data_offset(opt->data_offset() + width);
  }

  if (TwoLevelHints* two = two_level_hints()) {
    two->offset(two->offset() + width);
  }

  if (AtomInfo* info = atom_info()) {
    info->data_offset(info->data_offset() + width);
  }

  if (FunctionVariants* func_variants = function_variants()) {
    func_variants->data_offset(func_variants->data_offset() + width);
  }

  if (FunctionVariantFixups* func_variant_fixups = function_variant_fixups()) {
    func_variant_fixups->data_offset(func_variant_fixups->data_offset() + width);
  }

  linkedit->file_offset(linkedit->file_offset() + width);
  linkedit->virtual_address(linkedit->virtual_address() + width);
  for (const std::unique_ptr<Section>& section : linkedit->sections_) {
    if (lnk_offset <= section->offset()) {
      section->offset(section->offset() + width);
      section->virtual_address(section->virtual_address() + width);
    }
  }
  refresh_seg_offset();
  return ok();
}

void Binary::sort_segments() {
  commands_t::iterator start = commands_.end();
  commands_t::iterator end = commands_.end();

  for (auto it = commands_.begin(); it != commands_.end(); ++it) {
    if (start == commands_.end() && SegmentCommand::classof(it->get())) {
      start = it;
    }

    if (SegmentCommand::classof(it->get())) {
      end = it;
    }
  }
  ++end;

  bool all_segments = std::all_of(start, end, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return SegmentCommand::classof(cmd.get());
  });

  if (!all_segments) {
    LIEF_ERR("Segment commands non contiguous. Sort aborted!");
    return;
  }

  std::sort(start, end,
    [] (const std::unique_ptr<LoadCommand>& lhs, const std::unique_ptr<LoadCommand>& rhs) {
      return lhs->as<SegmentCommand>()->virtual_address() < rhs->as<SegmentCommand>()->virtual_address();
    }
  );

  segments_.clear();
  offset_seg_.clear();

  for (auto it = start; it != end; ++it) {
    SegmentCommand& seg = *(*it)->as<SegmentCommand>();
    seg.index_ = segments_.size();
    if (can_cache_segment(seg)) {
      offset_seg_[seg.file_offset()] = &seg;
    }
    segments_.push_back(&seg);
  }
}

void Binary::shift_command(size_t width, uint64_t from_offset) {
  const SegmentCommand* segment = segment_from_offset(from_offset);

  uint64_t __text_base_addr = 0;
  uint64_t virtual_address = 0;

  if (segment != nullptr) {
    virtual_address = segment->virtual_address() + from_offset - segment->file_offset();
  }

  if (const SegmentCommand* text = get_segment("__TEXT")) {
    __text_base_addr = text->virtual_address();
  }


  // Shift symbols command
  // =====================
  if (SymbolCommand* sym_cmd = symbol_command()) {

    if (sym_cmd->symbol_offset() > from_offset) {
      sym_cmd->symbol_offset(sym_cmd->symbol_offset() + width);
    }

    if (sym_cmd->strings_offset() > from_offset) {
      sym_cmd->strings_offset(sym_cmd->strings_offset() + width);
    }

    for (std::unique_ptr<Symbol>& s : symbols_) {
      if (s->type() == Symbol::TYPE::SECTION) {
        if (s->value() > virtual_address) {
          s->value(s->value() + width);
        }
      }
    }
  }

  // Data In Code
  // ============
  if (DataInCode* data_code_cmd = data_in_code()) {
    if (data_code_cmd->data_offset() > from_offset) {
      data_code_cmd->data_offset(data_code_cmd->data_offset() + width);
    }
  }


  // Code Signature
  // ==============
  if (CodeSignature* sig = code_signature()) {
    if (sig->data_offset() > from_offset) {
      sig->data_offset(sig->data_offset() + width);
    }
  }

  if (CodeSignatureDir* sig_dir = code_signature_dir()) {
    if (sig_dir->data_offset() > from_offset) {
      sig_dir->data_offset(sig_dir->data_offset() + width);
    }
  }

  if (SegmentSplitInfo* ssi = segment_split_info()) {
    if (ssi->data_offset() > from_offset) {
      ssi->data_offset(ssi->data_offset() + width);
    }
  }

  // Shift Main Command
  // ==================
  if (MainCommand* main_cmd = main_command()) {
     if ((__text_base_addr + main_cmd->entrypoint()) > virtual_address) {
      main_cmd->entrypoint(main_cmd->entrypoint() + width);
    }
  }

  // Patch function starts
  // =====================
  if (FunctionStarts* fs = function_starts()) {
    if (fs->data_offset() > from_offset) {
      fs->data_offset(fs->data_offset() + width);
    }
    for (uint64_t& address : fs->functions()) {
      if ((__text_base_addr + address) > virtual_address) {
        address += width;
      }
    }
  }

  // Dynamic symbol command
  // ======================
  if (DynamicSymbolCommand* dyn_cmd = dynamic_symbol_command()) {
    if (dyn_cmd->toc_offset() > from_offset) {
      dyn_cmd->toc_offset(dyn_cmd->toc_offset() + width);
    }

    if (dyn_cmd->module_table_offset() > from_offset) {
      dyn_cmd->module_table_offset(dyn_cmd->module_table_offset() + width);
    }

    if (dyn_cmd->external_reference_symbol_offset() > from_offset) {
      dyn_cmd->external_reference_symbol_offset(dyn_cmd->external_reference_symbol_offset() + width);
    }

    if (dyn_cmd->indirect_symbol_offset() > from_offset) {
      dyn_cmd->indirect_symbol_offset(dyn_cmd->indirect_symbol_offset() + width);
    }

    if (dyn_cmd->external_relocation_offset() > from_offset) {
      dyn_cmd->external_relocation_offset(dyn_cmd->external_relocation_offset() + width);
    }

    if (dyn_cmd->local_relocation_offset() > from_offset) {
      dyn_cmd->local_relocation_offset(dyn_cmd->local_relocation_offset() + width);
    }
  }

  // Patch Dyld
  // ==========
  if (DyldInfo* dyld = dyld_info()) {

    // Shift underlying containers offset
    if (dyld->rebase().first > from_offset) {
      dyld->set_rebase_offset(dyld->rebase().first + width);
    }

    if (dyld->bind().first > from_offset) {
      dyld->set_bind_offset(dyld->bind().first + width);
    }

    if (dyld->weak_bind().first > from_offset) {
      dyld->set_weak_bind_offset(dyld->weak_bind().first + width);
    }

    if (dyld->lazy_bind().first > from_offset) {
      dyld->set_lazy_bind_offset(dyld->lazy_bind().first + width);
    }

    if (dyld->export_info().first > from_offset) {
      dyld->set_export_offset(dyld->export_info().first + width);
    }


    // Shift Relocations
    // -----------------
    // TODO: Optimize this code
    for (Relocation& reloc : relocations()) {
      if (reloc.address() > virtual_address) {
        if (is64_) {
          patch_relocation<uint64_t>(reloc, /* from */ virtual_address, /* shift */ width);
        } else {
          patch_relocation<uint32_t>(reloc, /* from */ virtual_address, /* shift */ width);
        }
        reloc.address(reloc.address() + width);
      }
    }

    // Shift Export Info
    // -----------------
    for (ExportInfo& info : dyld->exports()) {
      if (info.address() > from_offset) {
        info.address(info.address() + width);
      }
    }

    // Shift bindings
    // --------------
    for (DyldBindingInfo& info : dyld->bindings()) {
      if (info.address() > virtual_address) {
        info.address(info.address() + width);
      }
    }
  }

  if (DyldChainedFixups* fixups = dyld_chained_fixups()) {
    fixups->data_offset(fixups->data_offset() + width);

    // Update relocations
    for (auto& entry : fixups->chained_starts_in_segments()) {
      for (auto& reloc : entry.segment.relocations()) {
        if (auto* fixup = reloc.cast<RelocationFixup>()) {
          if (fixup->offset() > from_offset) {
            fixup->offset(fixup->offset() + width);
          }
          if (fixup->target() > virtual_address) {
            fixup->target(fixup->target() + width);
          }
          // No need to update the virtual address since
          // it is bound to the offset
        }
      }
    }
    for (ChainedBindingInfo& bind : fixups->bindings()) {
      if (bind.offset() > from_offset) {
        bind.offset(bind.offset() + width);
      }

      if (bind.address() > virtual_address) {
        bind.address(bind.address() + width);
      }
      // We don't need to update the virtual address,
      // as it is bound to the offset
    }
  }

  if (DyldExportsTrie* exports = dyld_exports_trie()) {
    for (ExportInfo& info : exports->exports()) {
      if (info.address() > from_offset) {
        info.address(info.address() + width);
      }
    }
    if (exports->data_offset() > from_offset) {
      exports->data_offset(exports->data_offset() + width);
    }
  }

  if (LinkerOptHint* opt = linker_opt_hint()) {
    if (opt->data_offset() > from_offset) {
      opt->data_offset(opt->data_offset() + width);
    }
  }

  if (TwoLevelHints* two = two_level_hints()) {
    if (two->offset() > from_offset) {
      two->offset(two->offset() + width);
    }
  }

  if (Routine* routine = routine_command()) {
    if (routine->init_address() > virtual_address) {
      routine->init_address(routine->init_address() + width);
    }
  }

  if (AtomInfo* info = atom_info()) {
    if (info->data_offset() > from_offset) {
      info->data_offset(info->data_offset() + width);
    }
  }

  if (FunctionVariants* func_variants = function_variants()) {
    if (func_variants->data_offset() > from_offset) {
      func_variants->data_offset(func_variants->data_offset() + width);
    }
  }

  if (FunctionVariantFixups* func_variant_fixups = function_variant_fixups()) {
    if (func_variant_fixups->data_offset() > from_offset) {
      func_variant_fixups->data_offset(func_variant_fixups->data_offset() + width);
    }
  }

  for_commands<EncryptionInfo>([from_offset, width] (EncryptionInfo& enc) {
    if (enc.crypt_offset() > from_offset) {
      enc.crypt_offset(enc.crypt_offset() + width);
    }
  });

  for_commands<NoteCommand>([from_offset, width] (NoteCommand& note) {
    if (note.note_offset() > from_offset) {
      note.note_offset(note.note_offset() + width);
    }
  });

}

ok_error_t Binary::shift(size_t value) {
  value = align(value, page_size());

  Header& header = this->header();

  // Offset of the load commands table
  const uint64_t loadcommands_start = is64_ ? sizeof(details::mach_header_64) :
                                              sizeof(details::mach_header);

  // +------------------------+ <---------- __TEXT.start
  // |      Mach-O Header     |
  // +------------------------+ <===== loadcommands_start
  // |                        |
  // | Load Command Table     |
  // |                        |
  // +------------------------+ <===== loadcommands_end
  // |************************|
  // |************************| Assembly code
  // |************************|
  // +------------------------+ <---------- __TEXT.end
  const uint64_t loadcommands_end = loadcommands_start + header.sizeof_cmds();

  // Segment that wraps this load command table
  SegmentCommand* load_cmd_segment = segment_from_offset(loadcommands_end);
  if (load_cmd_segment == nullptr) {
    LIEF_ERR("Can't find segment associated with load command space");
    return make_error_code(lief_errors::file_format_error);
  }
  LIEF_DEBUG("LC Table wrapped by {} / End offset: 0x{:x} (size: {:x})",
             load_cmd_segment->name(), loadcommands_end, load_cmd_segment->data_.size());
  load_cmd_segment->content_insert(loadcommands_end, value);

  // 1. Shift all commands
  // =====================
  for (std::unique_ptr<LoadCommand>& cmd : commands_) {
    if (cmd->command_offset() >= loadcommands_end) {
      cmd->command_offset(cmd->command_offset() + value);
    }
  }

  shift_command(value, loadcommands_end);
  const uint64_t loadcommands_end_va = loadcommands_end + load_cmd_segment->virtual_address();

  LIEF_DEBUG("loadcommands_end:    0x{:016x}", loadcommands_end);
  LIEF_DEBUG("loadcommands_end_va: 0x{:016x}", loadcommands_end_va);

  // Shift Segment and sections
  // ==========================
  for (SegmentCommand* segment : segments_) {
    // Extend the virtual size of the segment containing our shift
    if (segment->file_offset() <= loadcommands_end &&
        loadcommands_end < (segment->file_offset() + segment->file_size()))
    {
      LIEF_DEBUG("Extending '{}' by {:x}", segment->name(), value);
      segment->virtual_size(segment->virtual_size() + value);
      segment->file_size(segment->file_size() + value);

      for (const std::unique_ptr<Section>& section : segment->sections_) {
        if (section->offset() >= loadcommands_end) {
          section->offset(section->offset() + value);
          section->virtual_address(section->virtual_address() + value);
        }
      }
    } else {
      if (segment->virtual_address() >= loadcommands_end_va) {
        segment->virtual_address(segment->virtual_address() + value);
      }

      if (segment->file_offset() >= loadcommands_end) {
        segment->file_offset(segment->file_offset() + value);
      }

      for (const std::unique_ptr<Section>& section : segment->sections_) {
        if (section->virtual_address() >= loadcommands_end_va) {
          section->virtual_address(section->virtual_address() + value);
        }
        if (section->offset() >= loadcommands_end) {
          section->offset(section->offset() + value);
        }
      }
    }
  }
  refresh_seg_offset();
  available_command_space_ += value;
  return ok();
}

LoadCommand* Binary::add(std::unique_ptr<LoadCommand> command) {
  const int32_t size_aligned = align(command->size(), pointer_size());

  // Check there is enough space between the
  // load command table and the raw content
  if (auto result = ensure_command_space(size_aligned); is_err(result)) {
    LIEF_ERR("Failed to ensure command space {}: {}", size_aligned, to_string(get_error(result)));
    return nullptr;
  }
  available_command_space_ -= size_aligned;

  Header& header = this->header();

  // Get border of the load command table
  const uint64_t loadcommands_start = is64_ ? sizeof(details::mach_header_64) :
                                              sizeof(details::mach_header);
  const uint64_t loadcommands_end = loadcommands_start + header.sizeof_cmds();

  // Update the Header according to the command that will be added
  header.sizeof_cmds(header.sizeof_cmds() + size_aligned);
  header.nb_cmds(header.nb_cmds() + 1);

  // Get the segment handling the LC table
  SegmentCommand* load_cmd_segment = segment_from_offset(loadcommands_end);
  if (load_cmd_segment == nullptr) {
    LIEF_WARN("Can't get the last load command");
    return nullptr;
  }

  span<const uint8_t> content_ref = load_cmd_segment->content();
  std::vector<uint8_t> content = {std::begin(content_ref), std::end(content_ref)};

  // Copy the command data
  std::copy(std::begin(command->data()), std::end(command->data()),
            std::begin(content) + loadcommands_end);

  load_cmd_segment->content(std::move(content));

  // Add the command in the Binary
  command->command_offset(loadcommands_end);


  // Update cache
  if (DylibCommand::classof(command.get())) {
    libraries_.push_back(command->as<DylibCommand>());
  }

  if (SegmentCommand::classof(command.get())) {
    add_cached_segment(*command->as<SegmentCommand>());
  }
  LoadCommand* ptr = command.get();
  commands_.push_back(std::move(command));
  return ptr;
}

LoadCommand* Binary::add(const LoadCommand& command, size_t index) {
  // If index is "too" large <=> push_back
  if (index >= commands_.size()) {
    return add(command);
  }

  const size_t size_aligned = align(command.size(), pointer_size());
  LIEF_DEBUG("available_command_space_: 0x{:06x} (required: 0x{:06x})",
             available_command_space_, size_aligned);

  if (auto result = ensure_command_space(size_aligned); is_err(result)) {
    LIEF_ERR("Failed to ensure command space {}: {}", size_aligned, to_string(get_error(result)));
    return nullptr;
  }
  available_command_space_ -= size_aligned;

  // Update the Header according to the new command
  Header& header = this->header();

  header.sizeof_cmds(header.sizeof_cmds() + size_aligned);
  header.nb_cmds(header.nb_cmds() + 1);

  // Get offset of the LC border
  LoadCommand* cmd_border = commands_[index].get();
  uint64_t border_off = cmd_border->command_offset();

  std::unique_ptr<LoadCommand> copy{command.clone()};
  copy->command_offset(cmd_border->command_offset());

  // Patch LC offsets that follow the LC border
  for (std::unique_ptr<LoadCommand>& lc : commands_) {
    if (lc->command_offset() >= border_off) {
      lc->command_offset(lc->command_offset() + size_aligned);
    }
  }

  if (auto* lib = copy->cast<DylibCommand>()) {
    libraries_.push_back(lib);
  }

  if (auto* segment = copy->cast<SegmentCommand>()) {
    add_cached_segment(*segment);
  }
  LoadCommand* copy_ptr = copy.get();
  commands_.insert(std::begin(commands_) + index, std::move(copy));
  return copy_ptr;
}

bool Binary::remove(const LoadCommand& command) {

  const auto it = std::find_if(
      std::begin(commands_), std::end(commands_),
      [&command] (const std::unique_ptr<LoadCommand>& cmd) {
        return *cmd == command;
      });

  if (it == std::end(commands_)) {
    LIEF_ERR("Unable to find command: {}", to_string(command));
    return false;
  }

  LoadCommand* cmd_rm = it->get();

  if (auto* lib = cmd_rm->cast<DylibCommand>()) {
    auto it_cache = std::find(std::begin(libraries_), std::end(libraries_), cmd_rm);
    if (it_cache == std::end(libraries_)) {
      LIEF_WARN("Library {} not found in cache. The binary object is likely in an inconsistent state", lib->name());
    } else {
      libraries_.erase(it_cache);
    }
  }

  if (const auto* seg = cmd_rm->cast<const SegmentCommand>()) {
    auto it_cache = std::find(std::begin(segments_), std::end(segments_), cmd_rm);
    if (it_cache == std::end(segments_)) {
      LIEF_WARN("Segment {} not found in cache. The binary object is likely in an inconsistent state", seg->name());
    } else {
      // Update the indexes to keep a consistent state
      for (auto it = it_cache; it != std::end(segments_); ++it) {
        (*it)->index_--;
      }
      segments_.erase(it_cache);
    }
  }

  const uint64_t cmd_rm_offset = cmd_rm->command_offset();
  for (std::unique_ptr<LoadCommand>& cmd : commands_) {
    if (cmd->command_offset() >= cmd_rm_offset) {
      cmd->command_offset(cmd->command_offset() - cmd_rm->size());
    }
  }


  Header& header = this->header();
  header.sizeof_cmds(header.sizeof_cmds() - cmd_rm->size());
  header.nb_cmds(header.nb_cmds() - 1);
  available_command_space_ += cmd_rm->size();

  commands_.erase(it);
  refresh_seg_offset();
  return true;
}


bool Binary::remove(LoadCommand::TYPE type) {
  bool removed = false;

  while (LoadCommand* cmd = get(type)) {
    removed = remove(*cmd);
  }
  return removed;
}

bool Binary::remove_command(size_t index) {
  if (index >= commands_.size()) {
    return false;
  }
  return remove(*commands_[index]);
}

bool Binary::has(LoadCommand::TYPE type) const {
  const auto it = std::find_if(
      std::begin(commands_), std::end(commands_),
      [type] (const std::unique_ptr<LoadCommand>& cmd) {
        return cmd->command() == type;
      });
  return it != std::end(commands_);
}

const LoadCommand* Binary::get(LoadCommand::TYPE type) const {
  const auto it = std::find_if(
      std::begin(commands_), std::end(commands_),
      [type] (const std::unique_ptr<LoadCommand>& cmd) {
        return cmd->command() == type;
      });

  if (it == std::end(commands_)) {
    return nullptr;
  }
  return it->get();
}

bool Binary::extend(const LoadCommand& command, uint64_t size) {
  const auto it = std::find_if(
      std::begin(commands_), std::end(commands_),
      [&command] (const std::unique_ptr<LoadCommand>& cmd) {
        return *cmd == command;
      });

  if (it == std::end(commands_)) {
    LIEF_ERR("Unable to find command: {}", to_string(command));
    return false;
  }

  LoadCommand* cmd = it->get();
  const size_t size_aligned = align(size, pointer_size());
  if (auto result = ensure_command_space(size_aligned); is_err(result)) {
    LIEF_ERR("Failed to ensure command space {}: {}", size_aligned, to_string(get_error(result)));
    return false;
  }
  available_command_space_ -= size_aligned;

  for (std::unique_ptr<LoadCommand>& lc : commands_) {
    if (lc->command_offset() > cmd->command_offset()) {
      lc->command_offset(lc->command_offset() + size_aligned);
    }
  }

  cmd->size(cmd->size() + size_aligned);
  cmd->original_data_.resize(cmd->original_data_.size() + size_aligned);

  // Update Header
  // =============
  Header& header = this->header();
  header.sizeof_cmds(header.sizeof_cmds() + size_aligned);

  return true;
}


bool Binary::extend_segment(const SegmentCommand& segment, size_t size) {

  const auto it_segment = std::find_if(
      std::begin(segments_), std::end(segments_),
      [&segment] (const SegmentCommand* s) {
        return segment == *s;
      });

  if (it_segment == std::end(segments_)) {
    LIEF_ERR("Unable to find segment: '{}'", segment.name());
    return false;
  }

  SegmentCommand* target_segment = *it_segment;
  const uint64_t last_offset = target_segment->file_offset() + target_segment->file_size();
  const uint64_t last_va     = target_segment->virtual_address() + target_segment->virtual_size();

  const int32_t size_aligned = align(size, pointer_size());

  shift_command(size_aligned, last_offset - 4);

  // Shift Segment and sections
  // ==========================
  for (SegmentCommand* segment : segments_) {
    if (segment->virtual_address() >= last_va) {
      segment->virtual_address(segment->virtual_address() + size_aligned);
    }
    if (segment->file_offset() >= last_offset) {
      segment->file_offset(segment->file_offset() + size_aligned);
    }

    for (const std::unique_ptr<Section>& section : segment->sections_) {
      if (section->virtual_address() >= last_va) {
        section->virtual_address(section->virtual_address() + size_aligned);
      }
      if (section->offset() >= last_offset) {
        section->offset(section->offset() + size_aligned);
      }
    }
  }


  target_segment->virtual_size(target_segment->virtual_size() + size_aligned);
  target_segment->file_size(target_segment->file_size() + size_aligned);
  target_segment->content_resize(target_segment->file_size());
  refresh_seg_offset();
  return true;
}

bool Binary::extend_section(Section& section, size_t size) {
  // All sections must keep their requested alignment.
  //
  // As per current implementation of `shift` method, space is allocated between
  // the last load command and the first section by shifting everything to the "right".
  // After that we shift `section` and all other sections that come before it to the "left",
  // so that we create a gap of at least `size` wide after the current `section`.
  // Finally, we assign new size to the `section`.
  //
  // Let's say we are extending section S.
  // There might be sections P that come prior S, and there might be sections A that come after S.
  // We try to keep relative relationships between sections in groups P and A,
  // such that relative offsets from one section to another one are unchanged,
  // however preserving the same relationship between sections from different groups is impossible.
  // We achieve this by shifting P and S to the left by size rounded up to the maximum common alignment factor.

  const uint64_t loadcommands_start = is64_ ? sizeof(details::mach_header_64) :
                                              sizeof(details::mach_header);
  const uint64_t loadcommands_end = loadcommands_start + header().sizeof_cmds();
  SegmentCommand* load_cmd_segment = segment_from_offset(loadcommands_end);
  if (load_cmd_segment == nullptr) {
    LIEF_ERR("Can't find segment associated with load command space");
    return false;
  }

  if (section.segment() != load_cmd_segment) {
    LIEF_ERR("Can't extend section that belongs to segment '{}' which is not the first one", section.segment_name());
    return false;
  }

  // Note: if we are extending an empty section, then there may be many zero-sized sections at
  // this offset `section.offset()`, and one non-empty section.

  // Select sections that we need to shift to the left.
  // Sections that come after the current one are not considered for shifting.
  // Note: we compare sections by end_offset to exclude non-empty section that starts at the current offset.
  sections_cache_t sections_to_shift;
  for (Section& s : sections()) {
    if (s.offset() == 0 || s.offset() + s.size() > section.offset() + section.size()) {
      continue;
    }
    sections_to_shift.push_back(&s);
  }
  assert(!sections_to_shift.empty());

  // Stable-sort by offset in ascending order as well as preserving original order.
  std::stable_sort(sections_to_shift.begin(), sections_to_shift.end(),
            [](const Section* a, const Section* b) { return a->offset() < b->offset(); });

  // We do not want to shift empty sections that were added after the current one.
  auto it = std::find(sections_to_shift.begin(), sections_to_shift.end(), &section);
  assert(it != sections_to_shift.end());
  sections_to_shift.erase(std::next(it), sections_to_shift.end());

  // Find maximum alignment
  auto it_maxa = std::max_element(sections_to_shift.begin(), sections_to_shift.end(),
            [](const Section* a, const Section* b) {
              return a->alignment() < b->alignment();
            });
  const size_t max_alignment = 1 << (*it_maxa)->alignment();

  // Resize command space, if needed.
  const size_t shift_value = align(size, max_alignment);
  if (auto result = ensure_command_space(shift_value); is_err(result)) {
    LIEF_ERR("Failed to ensure command space {}: {}", shift_value, to_string(get_error(result)));
    return false;
  }
  available_command_space_ -= shift_value;

  // Shift selected sections to allocate requested space for `section`.
  for (Section* s : sections_to_shift) {
    s->offset(s->offset() - shift_value);
    s->address(s->address() - shift_value);
  }

  // Extend the given `section`.
  section.size(section.size() + shift_value);

  return true;
}

void Binary::remove_section(const std::string& name, bool clear) {
  Section* sec_to_delete = get_section(name);
  if (sec_to_delete == nullptr) {
    LIEF_ERR("Can't find section '{}'", name);
    return;
  }
  SegmentCommand* segment = sec_to_delete->segment();
  if (segment == nullptr) {
    LIEF_ERR("The section {} is in an inconsistent state (missing segment). Can't remove it",
             sec_to_delete->name());
    return;
  }

  remove_section(segment->name(), name, clear);
}

void Binary::remove_section(const std::string& segname, const std::string& secname, bool clear) {
  Section* sec_to_delete = get_section(segname, secname);
  if (sec_to_delete == nullptr) {
    LIEF_ERR("Can't find section '{}' in segment '{}'", secname, segname);
    return;
  }
  SegmentCommand* segment = sec_to_delete->segment();
  if (segment == nullptr) {
    LIEF_ERR("The section {} is in an inconsistent state (missing segment). Can't remove it",
             sec_to_delete->name());
    return;
  }

  if (clear) {
    sec_to_delete->clear(0);
  }

  segment->numberof_sections(segment->numberof_sections() - 1);
  auto it_section = std::find_if(
      std::begin(segment->sections_), std::end(segment->sections_),
      [sec_to_delete] (const std::unique_ptr<Section>& s) {
        return *s == *sec_to_delete;
      });

  if (it_section == std::end(segment->sections_)) {
    LIEF_WARN("Can't find the section");
    return;
  }

  const size_t lc_offset = segment->command_offset();
  const size_t section_struct_size = is64_ ? sizeof(details::section_64) :
                                             sizeof(details::section_32);
  segment->size_ -= section_struct_size;

  header().sizeof_cmds(header().sizeof_cmds() - section_struct_size);

  for (std::unique_ptr<LoadCommand>& lc : commands_) {
    if (lc->command_offset() > lc_offset) {
      lc->command_offset(lc->command_offset() - section_struct_size);
    }
  }

  available_command_space_ += section_struct_size;

  std::unique_ptr<Section>& section = *it_section;
  // Remove from cache
  auto it_cache = std::find_if(std::begin(sections_), std::end(sections_),
      [&section] (const Section* sec) {
        return section.get() == sec;
      });

  if (it_cache == std::end(sections_)) {
    LIEF_WARN("Can find the section {} in the cache. The binary object is likely in an inconsistent state",
              section->name());
  } else {
    sections_.erase(it_cache);
  }

  segment->sections_.erase(it_section);
}

Section* Binary::add_section(const Section& section) {
  SegmentCommand* _TEXT_segment = get_segment("__TEXT");
  if (_TEXT_segment == nullptr) {
    LIEF_ERR("Unable to get '__TEXT' segment");
    return nullptr;
  }
  return add_section(*_TEXT_segment, section);
}

Section* Binary::add_section(const SegmentCommand& segment, const Section& section) {

  const auto it_segment = std::find_if(
      std::begin(segments_), std::end(segments_),
      [&segment] (const SegmentCommand* s) {
        return segment == *s;
      });

  if (it_segment == std::end(segments_)) {
    LIEF_ERR("Unable to find segment: '{}'", segment.name());
    return nullptr;
  }
  SegmentCommand* target_segment = *it_segment;

  span<const uint8_t> content_ref = section.content();
  Section::content_t content = {std::begin(content_ref), std::end(content_ref)};

  auto new_section = std::make_unique<Section>(section);

  if (section.offset() == 0) {
    // Section offset is not defined: we need to allocate space enough to fit its content.
    const size_t hdr_size = is64_ ? sizeof(details::section_64) :
                                    sizeof(details::section_32);
    const size_t alignment = 1 << section.alignment();
    const size_t needed_size = hdr_size + content.size() + alignment;

    // Request size with a gap of alignment, so we would have enough room
    // to adjust section's offset to satisfy its alignment requirements.
    if (auto result = ensure_command_space(needed_size); is_err(result)) {
      LIEF_ERR("Failed to ensure command space {}: {}", needed_size, to_string(get_error(result)));
      return nullptr;
    }

    if (!extend(*target_segment, hdr_size)) { // adjusts available_command_space_
      LIEF_ERR("Unable to extend segment '{}' by 0x{:x}", segment.name(), hdr_size);
      return nullptr;
    }

    const uint64_t loadcommands_start = is64_ ? sizeof(details::mach_header_64) :
                                                sizeof(details::mach_header);
    const uint64_t loadcommands_end = loadcommands_start + header().sizeof_cmds();

    // let new_offset supposedly point to the contents of the first section
    uint64_t new_offset = loadcommands_end + available_command_space_;
    new_offset -= content.size();
    new_offset = align_down(new_offset, alignment);

    // put section data in front of the first section
    new_section->offset(new_offset);

    available_command_space_ = new_offset - loadcommands_end;
  }

  // Compute offset, virtual address etc for the new section
  // =======================================================
  if (section.size() == 0) {
    new_section->size(content.size());
  }

  if (section.virtual_address() == 0) {
    new_section->virtual_address(target_segment->virtual_address() + new_section->offset());
  }

  new_section->segment_ = target_segment;
  target_segment->numberof_sections(target_segment->numberof_sections() + 1);

  // Copy the new section in the cache
  sections_.push_back(new_section.get());

  // Copy data to segment
  const uint64_t relative_offset = new_section->offset() - target_segment->file_offset();

  std::move(std::begin(content), std::end(content),
            std::begin(target_segment->data_) + relative_offset);

  target_segment->sections_.push_back(std::move(new_section));
  return target_segment->sections_.back().get();
}


LoadCommand* Binary::add(const SegmentCommand& segment) {
  /*
   * To add a new segment in a Mach-O file, we need to:
   *
   * 1. Allocate space for a new Load command: LC_SEGMENT_64 / LC_SEGMENT
   *    which must include the sections
   * 2. Allocate space for the content of the provided segment
   *
   * For #1, the logic is to shift all the content after the end of the load command table.
   * This modification is described in doc/sphinx/tutorials/11_macho_modification.rst.
   *
   * For #2, the easiest way is to place the content at the end of the Mach-O file and
   * to make the LC_SEGMENT point to this area. It works as expected as long as
   * the binary does not need to be signed.
   *
   * If the binary has to be signed, codesign and the underlying Apple libraries
   * enforce that there is not data after the __LINKEDIT segment, otherwise we get
   * this kind of error: "main executable failed strict validation".
   * To comply with this check, we can shift the __LINKEDIT segment (c.f. ``shift_linkedit(...)``)
   * such as the data of the new segment are located before __LINKEDIT.
   * Nevertheless, we can't shift __LINKEDIT by an arbitrary value. For ARM and ARM64,
   * ld/dyld enforces a segment alignment of "4 * 4096" as coded in ``Options::reconfigureDefaults``
   * of ``ld64-609/src/ld/Option.cpp``:
   *
   * ```cpp
   * ...
   * <rdar://problem/13070042> Only third party apps should have 16KB page segments by default
   * if (fEncryptable) {
   *  if (fSegmentAlignment == 4096)
   *    fSegmentAlignment = 4096*4;
   * }
   *
   * // <rdar://problem/12258065> ARM64 needs 16KB page size for user land code
   * // <rdar://problem/15974532> make armv7[s] use 16KB pages in user land code for iOS 8 or later
   * if (fArchitecture == CPU_TYPE_ARM64 || (fArchitecture == CPU_TYPE_ARM) ) {
   *   fSegmentAlignment = 4096*4;
   * }
   * ```
   * Therefore, we must shift __LINKEDIT by at least 4 * 0x1000 for Mach-O files targeting ARM
   */
  LIEF_DEBUG("Adding the new segment '{}' ({} bytes)", segment.name(), segment.content().size());
  const uint32_t alignment = page_size();
  const uint64_t new_fsize = align(segment.content().size(), alignment);
  SegmentCommand new_segment = segment;

  if (new_segment.file_size() == 0) {
    new_segment.file_size(new_fsize);
    new_segment.content_resize(new_fsize);
  }

  if (new_segment.virtual_size() == 0) {
    const uint64_t new_size = align(new_segment.file_size(), alignment);
    new_segment.virtual_size(new_size);
  }

  if (segment.sections().size() > 0) {
    new_segment.nb_sections_ = segment.sections().size();
  }

  if (is64_) {
    new_segment.command(LoadCommand::TYPE::SEGMENT_64);
    size_t needed_size = sizeof(details::segment_command_64);
    needed_size += new_segment.numberof_sections() * sizeof(details::section_64);
    new_segment.size(needed_size);
  } else {
    new_segment.command(LoadCommand::TYPE::SEGMENT);
    size_t needed_size = sizeof(details::segment_command_32);
    needed_size += new_segment.numberof_sections() * sizeof(details::section_32);
    new_segment.size(needed_size);
  }

  LIEF_DEBUG(" -> sizeof(LC_SEGMENT): {}", new_segment.size());

  // Insert the segment before __LINKEDIT
  const auto it_linkedit = std::find_if(std::begin(commands_), std::end(commands_),
      [] (const std::unique_ptr<LoadCommand>& cmd) {
        if (!SegmentCommand::classof(cmd.get())) {
          return false;
        }
        return cmd->as<SegmentCommand>()->name() == "__LINKEDIT";
      });

  const bool has_linkedit = it_linkedit != std::end(commands_);

  size_t pos = std::distance(std::begin(commands_), it_linkedit);

  LIEF_DEBUG(" -> index: {}", pos);

  auto* new_cmd = add(new_segment, pos);

  if (new_cmd == nullptr) {
    LIEF_WARN("Fail to insert new '{}' segment", segment.name());
    return nullptr;
  }

  auto* segment_added = new_cmd->as<SegmentCommand>();

  if (!has_linkedit) {
    /* If there are not __LINKEDIT segment we can point the Segment's content to the EOF
     * NOTE(romain): I don't know if a binary without a __LINKEDIT segment exists
     */
    range_t new_va_ranges  = this->va_ranges();
    range_t new_off_ranges = off_ranges();
    if (segment.virtual_address() == 0 && segment_added->virtual_size() != 0) {
      const uint64_t new_va = align(new_va_ranges.end, alignment);
      segment_added->virtual_address(new_va);
      uint64_t current_va = segment_added->virtual_address();
      for (Section& section : segment_added->sections()) {
        section.virtual_address(current_va);
        current_va += section.size();
      }
    }

    if (segment.file_offset() == 0 && segment_added->virtual_size() != 0) {
      const uint64_t new_offset = align(new_off_ranges.end, alignment);
      segment_added->file_offset(new_offset);
      uint64_t current_offset = new_offset;
      for (Section& section : segment_added->sections()) {
        section.offset(current_offset);
        current_offset += section.size();
      }
    }
    refresh_seg_offset();
    return segment_added;
  }

  uint64_t lnk_offset = 0;
  uint64_t lnk_va     = 0;

  if (const SegmentCommand* lnk = get_segment("__LINKEDIT")) {
    lnk_offset = lnk->file_offset();
    lnk_va     = lnk->virtual_address();
  }

  // Make space for the content of the new segment
  shift_linkedit(new_fsize);
  LIEF_DEBUG(" -> offset         : 0x{:06x}", lnk_offset);
  LIEF_DEBUG(" -> virtual address: 0x{:06x}", lnk_va);

  segment_added->virtual_address(lnk_va);
  segment_added->virtual_size(segment_added->virtual_size());
  uint64_t current_va = segment_added->virtual_address();
  for (Section& section : segment_added->sections()) {
    section.virtual_address(current_va);
    current_va += section.size();
  }

  segment_added->file_offset(lnk_offset);
  uint64_t current_offset = lnk_offset;
  for (Section& section : segment_added->sections()) {
    section.offset(current_offset);
    current_offset += section.size();
  }

  if (DyldChainedFixups* fixup = dyld_chained_fixups()) {
    DyldChainedFixups::chained_starts_in_segment new_info =
      DyldChainedFixups::chained_starts_in_segment::create_empty_chained(*segment_added);
    fixup->add(std::move(new_info));
  }

  refresh_seg_offset();
  return segment_added;
}

size_t Binary::add_cached_segment(SegmentCommand& segment) {
  // The new segement should be put **before** the __LINKEDIT segment
  const auto it_linkedit = std::find_if(std::begin(segments_), std::end(segments_),
      [] (SegmentCommand* cmd) { return cmd->name() == "__LINKEDIT"; });

  if (it_linkedit == std::end(segments_)) {
    LIEF_DEBUG("No __LINKEDIT segment found!");
    segment.index_ = segments_.size();
    segments_.push_back(&segment);
  } else {
    segment.index_ = (*it_linkedit)->index();

    // Update indexes
    for (auto it = it_linkedit; it != std::end(segments_); ++it) {
      (*it)->index_++;
    }
    segments_.insert(it_linkedit, &segment);
  }

  offset_seg_[segment.file_offset()] = &segment;
  if (LinkEdit::segmentof(segment)) {
    auto& linkedit = static_cast<LinkEdit&>(segment);
    linkedit.dyld_           = dyld_info();
    linkedit.chained_fixups_ = dyld_chained_fixups();
  }
  refresh_seg_offset();
  return segment.index();
}

bool Binary::unexport(const std::string& name) {
  for (const std::unique_ptr<Symbol>& s : symbols_) {
    if (s->name() == name && s->has_export_info()) {
      return unexport(*s);
    }
  }
  return false;
}

bool Binary::unexport(const Symbol& sym) {
  if (DyldInfo* dyld = dyld_info()) {
    const auto it_export = std::find_if(
        std::begin(dyld->export_info_), std::end(dyld->export_info_),
        [&sym] (const std::unique_ptr<ExportInfo>& info) {
          return info->has_symbol() && *info->symbol() == sym;
        });

    // The symbol is not exported
    if (it_export == std::end(dyld->export_info_)) {
      return false;
    }

    dyld->export_info_.erase(it_export);
    return true;
  }


  if (DyldExportsTrie* exports = dyld_exports_trie()) {
    const auto it_export = std::find_if(
        std::begin(exports->export_info_), std::end(exports->export_info_),
        [&sym] (const std::unique_ptr<ExportInfo>& info) {
          return info->has_symbol() && *info->symbol() == sym;
        });

    // The symbol is not exported
    if (it_export == std::end(exports->export_info_)) {
      return false;
    }

    exports->export_info_.erase(it_export);
    return true;
  }

  LIEF_INFO("Can't find neither LC_DYLD_INFO / LC_DYLD_CHAINED_FIXUPS");
  return false;
}

bool Binary::remove(const Symbol& sym) {
  unexport(sym);
  const auto it_sym = std::find_if(std::begin(symbols_), std::end(symbols_),
      [&sym] (const std::unique_ptr<Symbol>& s) {
        return s.get() == &sym;
      });

  if (it_sym == std::end(symbols_)) {
    return false;
  }

  if (DynamicSymbolCommand* dyst = dynamic_symbol_command()) {
    dyst->indirect_symbols_.erase(
        std::remove_if(std::begin(dyst->indirect_symbols_), std::end(dyst->indirect_symbols_),
                       [&sym] (const Symbol* s) { return s == &sym; }),
        std::end(dyst->indirect_symbols_));
  }

  symbols_.erase(it_sym);
  return true;
}

bool Binary::remove_symbol(const std::string& name) {
  bool removed = false;
  while (const Symbol* s = get_symbol(name)) {
    if (!remove(*s)) {
      break;
    }
    removed = true;
  }
  return removed;
}


bool Binary::can_remove(const Symbol& sym) const {
  /*
   * We consider that a symbol can be removed, if and only if
   * there are no binding associated with
   */
  if (const DyldInfo* dyld = dyld_info()) {
    for (const DyldBindingInfo& binding : dyld->bindings()) {
      if (binding.has_symbol() && binding.symbol()->name() == sym.name()) {
        return false;
      }
    }
  }

  if (const DyldChainedFixups* fixups = dyld_chained_fixups()) {
    for (const ChainedBindingInfo& binding : fixups->bindings()) {
      if (binding.has_symbol() && binding.symbol()->name() == sym.name()) {
        return false;
      }
    }
  }

  return true;
}

bool Binary::can_remove_symbol(const std::string& name) const {
  std::vector<const Symbol*> syms;
  for (const std::unique_ptr<Symbol>& s : symbols_) {
    if (s->name() == name) {
      syms.push_back(s.get());
    }
  }
  return std::all_of(std::begin(syms), std::end(syms),
                     [this] (const Symbol* s) { return can_remove(*s); });
}


bool Binary::remove_signature() {
  if (const CodeSignature* cs = code_signature()) {
    return remove(*cs);
  }
  LIEF_WARN("No signature found");
  return false;
}

LoadCommand* Binary::add(const DylibCommand& library) {
  return add(*library.as<LoadCommand>());
}

LoadCommand* Binary::add_library(const std::string& name) {
  return add(DylibCommand::load_dylib(name));
}

std::vector<uint8_t> Binary::raw() {
  std::vector<uint8_t> buffer;
  Builder::write(*this, buffer);
  return buffer;
}

result<uint64_t> Binary::virtual_address_to_offset(uint64_t virtual_address) const {
  const SegmentCommand* segment = segment_from_virtual_address(virtual_address);
  if (segment == nullptr) {
    return make_error_code(lief_errors::conversion_error);
  }
  const uint64_t base_address = segment->virtual_address() - segment->file_offset();
  return virtual_address - base_address;
}

result<uint64_t> Binary::offset_to_virtual_address(uint64_t offset, uint64_t slide) const {
  const SegmentCommand* segment = segment_from_offset(offset);
  if (segment == nullptr) {
    return slide + offset;
  }
  const uint64_t delta = segment->virtual_address() - segment->file_offset();
  const uint64_t imgbase = imagebase();

  if (slide == 0) {
    return delta + offset;
  }

  if (imgbase == 0) {
    return slide +
           segment->virtual_address() + (offset - segment->file_offset());
  }
  return (segment->virtual_address() - imgbase) + slide + (offset - segment->file_offset());
}

bool Binary::disable_pie() {
  if (is_pie()) {
    header().remove(Header::FLAGS::PIE);
    return true;
  }
  return false;
}

const Section* Binary::get_section(const std::string& name) const {
  const auto it_section = std::find_if(
      std::begin(sections_), std::end(sections_),
      [&name] (const Section* sec) {
        return sec->name() == name;
      });

  if (it_section == std::end(sections_)) {
    return nullptr;
  }

  return *it_section;
}


const Section* Binary::get_section(const std::string& segname, const std::string& secname) const {
  if (const SegmentCommand* seg = get_segment(segname)) {
    if (const Section* sec = seg->get_section(secname)) {
      return sec;
    }
  }
  return nullptr;
}

const SegmentCommand* Binary::get_segment(const std::string& name) const {
  const auto it_segment = std::find_if(
      std::begin(segments_), std::end(segments_),
      [&name] (const SegmentCommand* seg) {
        return seg->name() == name;
      });

  if (it_segment == std::end(segments_)) {
    return nullptr;
  }

  return *it_segment;
}

uint64_t Binary::imagebase() const {
  if (const SegmentCommand* _TEXT = get_segment("__TEXT")) {
    return _TEXT->virtual_address();
  }
  return 0;
}

std::string Binary::loader() const {
  if (const DylinkerCommand* cmd = dylinker()) {
    return cmd->name();
  }
  return "";
}

Binary::range_t Binary::va_ranges() const {
  uint64_t min = uint64_t(-1);
  uint64_t max = 0;

  for (const SegmentCommand* segment : segments_) {
    if (segment->name_ == "__PAGEZERO") {
      continue;
    }
    min = std::min<uint64_t>(min, segment->virtual_address());
    max = std::max(max, segment->virtual_address() + segment->virtual_size());
  }

  if (min == uint64_t(-1)) {
    return {0, 0};
  }

  return {min, max};
}

Binary::range_t Binary::off_ranges() const {
  uint64_t min = uint64_t(-1);
  uint64_t max = 0;

  for (const SegmentCommand* segment : segments_) {
    min = std::min<uint64_t>(min, segment->file_offset());
    max = std::max(max, segment->file_offset() + segment->file_size());
  }

  if (min == uint64_t(-1)) {
    return {0, 0};
  }

  return {min, max};
}


LIEF::Binary::functions_t Binary::ctor_functions() const {
  LIEF::Binary::functions_t functions;
  for (const Section& section : sections()) {
    if (section.type() != Section::TYPE::MOD_INIT_FUNC_POINTERS) {
      continue;
    }

    span<const uint8_t> content = section.content();
    if (is64_) {
      const size_t nb_fnc = content.size() / sizeof(uint64_t);
      const auto* aptr = reinterpret_cast<const uint64_t*>(content.data());
      for (size_t i = 0; i < nb_fnc; ++i) {
        functions.emplace_back("ctor_" + std::to_string(i), aptr[i],
                               Function::FLAGS::CONSTRUCTOR);
      }

    } else {
      const size_t nb_fnc = content.size() / sizeof(uint32_t);
      const auto* aptr = reinterpret_cast<const uint32_t*>(content.data());
      for (size_t i = 0; i < nb_fnc; ++i) {
        functions.emplace_back("ctor_" + std::to_string(i), aptr[i],
                               Function::FLAGS::CONSTRUCTOR);
      }
    }
  }
  return functions;
}


LIEF::Binary::functions_t Binary::functions() const {
  static const auto func_cmd = [] (const Function& lhs, const Function& rhs) {
    return lhs.address() < rhs.address();
  };
  std::set<Function, decltype(func_cmd)> functions_set(func_cmd);

  LIEF::Binary::functions_t unwind_functions = this->unwind_functions();
  LIEF::Binary::functions_t ctor_functions   = this->ctor_functions();
  LIEF::Binary::functions_t exported         = get_abstract_exported_functions();

  std::move(std::begin(unwind_functions), std::end(unwind_functions),
            std::inserter(functions_set, std::end(functions_set)));

  std::move(std::begin(ctor_functions), std::end(ctor_functions),
            std::inserter(functions_set, std::end(functions_set)));

  std::move(std::begin(exported), std::end(exported),
            std::inserter(functions_set, std::end(functions_set)));

  return {std::begin(functions_set), std::end(functions_set)};

}

LIEF::Binary::functions_t Binary::unwind_functions() const {
  static constexpr size_t UNWIND_COMPRESSED = 3;
  static constexpr size_t UNWIND_UNCOMPRESSED = 2;

  // Set container to have functions with unique address
  static const auto fcmd = [] (const Function& l, const Function& r) {
    return l.address() < r.address();
  };
  std::set<Function, decltype(fcmd)> functions(fcmd);

  // Look for the __unwind_info section
  const Section* unwind_section = get_section("__unwind_info");
  if (unwind_section == nullptr) {
    return {};
  }

  SpanStream vs = unwind_section->content();

  // Get section content
  const auto hdr = vs.read<details::unwind_info_section_header>();
  if (!hdr) {
    LIEF_ERR("Can't read unwind section header!");
    return {};
  }
  vs.setpos(hdr->index_section_offset);

  size_t lsda_start = -1lu;
  size_t lsda_stop = 0;
  for (size_t i = 0; i < hdr->index_count; ++i) {
    const auto section_hdr = vs.read<details::unwind_info_section_header_index_entry>();
    if (!section_hdr) {
      LIEF_ERR("Can't read function information at index #{:d}", i);
      break;
    }

    functions.emplace(section_hdr->function_offset);
    const size_t second_lvl_off = section_hdr->second_level_pages_section_offset;
    const size_t lsda_off       = section_hdr->lsda_index_array_section_offset;

    lsda_start = std::min(lsda_off, lsda_start);
    lsda_stop  = std::max(lsda_off, lsda_stop);

    if (second_lvl_off > 0 && vs.can_read<details::unwind_info_regular_second_level_page_header>(second_lvl_off)) {
      const size_t saved_pos = vs.pos();
      {
        vs.setpos(second_lvl_off);
        const auto lvl_hdr = vs.peek<details::unwind_info_regular_second_level_page_header>(second_lvl_off);
        if (!lvl_hdr) {
          break;
        }
        if (lvl_hdr->kind == UNWIND_COMPRESSED) {
          const auto lvl_compressed_hdr = vs.read<details::unwind_info_compressed_second_level_page_header>();
          if (!lvl_compressed_hdr) {
            LIEF_ERR("Can't read lvl_compressed_hdr");
            break;
          }

          vs.setpos(second_lvl_off + lvl_compressed_hdr->entry_page_offset);
          for (size_t j = 0; j < lvl_compressed_hdr->entry_count; ++j) {
            auto entry = vs.read<uint32_t>();
            if (!entry) {
              break;
            }
            uint32_t func_off = section_hdr->function_offset + (*entry & 0xffffff);
            functions.emplace(func_off);
          }
        }
        else if (lvl_hdr->kind == UNWIND_UNCOMPRESSED) {
          LIEF_WARN("UNWIND_UNCOMPRESSED is not supported yet!");
        }
        else {
          LIEF_WARN("Unknown 2nd level kind: {:d}", lvl_hdr->kind);
        }
      }
      vs.setpos(saved_pos);
    }

  }

  const size_t nb_lsda = lsda_stop > lsda_start ? (lsda_stop - lsda_start) / sizeof(details::unwind_info_section_header_lsda_index_entry) : 0;
  vs.setpos(lsda_start);
  for (size_t i = 0; i < nb_lsda; ++i) {
    const auto hdr = vs.read<details::unwind_info_section_header_lsda_index_entry>();
    if (!hdr) {
      LIEF_ERR("Can't read LSDA at index #{:d}", i);
      break;
    }
    functions.emplace(hdr->function_offset);
  }

  return {std::begin(functions), std::end(functions)};
}

// UUID
// ++++
UUIDCommand* Binary::uuid() {
  return command<UUIDCommand>();
}

const UUIDCommand* Binary::uuid() const {
  return command<UUIDCommand>();
}

// MainCommand
// +++++++++++
MainCommand* Binary::main_command() {
  return command<MainCommand>();
}

const MainCommand* Binary::main_command() const {
  return command<MainCommand>();
}

// DylinkerCommand
// +++++++++++++++
DylinkerCommand* Binary::dylinker() {
  return command<DylinkerCommand>();
}

const DylinkerCommand* Binary::dylinker() const {
  return command<DylinkerCommand>();
}

// DyldInfo
// ++++++++
DyldInfo* Binary::dyld_info() {
  return command<DyldInfo>();
}

const DyldInfo* Binary::dyld_info() const {
  return command<DyldInfo>();
}

// Function Starts
// +++++++++++++++
FunctionStarts* Binary::function_starts() {
  return command<FunctionStarts>();
}

const FunctionStarts* Binary::function_starts() const {
  return command<FunctionStarts>();
}

// Source Version
// ++++++++++++++
SourceVersion* Binary::source_version() {
  return command<SourceVersion>();
}

const SourceVersion* Binary::source_version() const {
  return command<SourceVersion>();
}

// Version Min
// +++++++++++
VersionMin* Binary::version_min() {
  return command<VersionMin>();
}

const VersionMin* Binary::version_min() const {
  return command<VersionMin>();
}

// Routine Command
// +++++++++++++++
Routine* Binary::routine_command() {
  return command<Routine>();
}

const Routine* Binary::routine_command() const {
  return command<Routine>();
}

// Thread command
// ++++++++++++++
ThreadCommand* Binary::thread_command() {
  return command<ThreadCommand>();
}

const ThreadCommand* Binary::thread_command() const {
  return command<ThreadCommand>();
}

// RPath command
// +++++++++++++
RPathCommand* Binary::rpath() {
  return command<RPathCommand>();
}

const RPathCommand* Binary::rpath() const {
  return command<RPathCommand>();
}

Binary::it_rpaths Binary::rpaths() {
  return {commands_, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return RPathCommand::classof(cmd.get());
  }};
}

Binary::it_const_rpaths Binary::rpaths() const {
  return {commands_, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return RPathCommand::classof(cmd.get());
  }};
}

// SymbolCommand command
// +++++++++++++++++++++
SymbolCommand* Binary::symbol_command() {
  return command<SymbolCommand>();
}

const SymbolCommand* Binary::symbol_command() const {
  return command<SymbolCommand>();
}

// DynamicSymbolCommand command
// ++++++++++++++++++++++++++++

DynamicSymbolCommand* Binary::dynamic_symbol_command() {
  return command<DynamicSymbolCommand>();
}

const DynamicSymbolCommand* Binary::dynamic_symbol_command() const {
  return command<DynamicSymbolCommand>();
}

// CodeSignature command
// +++++++++++++++++++++
const CodeSignature* Binary::code_signature() const {
  if (const auto* cmd = get(LoadCommand::TYPE::CODE_SIGNATURE)) {
    return cmd->as<const CodeSignature>();
  }
  return nullptr;
}


// CodeSignatureDir command
// ++++++++++++++++++++++++
const CodeSignatureDir* Binary::code_signature_dir() const {
  if (const auto* cmd = get(LoadCommand::TYPE::DYLIB_CODE_SIGN_DRS)) {
    return cmd->as<const CodeSignatureDir>();
  }
  return nullptr;
}


// DataInCode command
// ++++++++++++++++++
DataInCode* Binary::data_in_code() {
  return command<DataInCode>();
}

const DataInCode* Binary::data_in_code() const {
  return command<DataInCode>();
}


// SegmentSplitInfo command
// ++++++++++++++++++++++++
SegmentSplitInfo* Binary::segment_split_info() {
  return command<SegmentSplitInfo>();
}

const SegmentSplitInfo* Binary::segment_split_info() const {
  return command<SegmentSplitInfo>();
}


// SubClient command
// ++++++++++++++++++++
Binary::it_sub_clients Binary::subclients() {
  return {commands_, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return SubClient::classof(cmd.get());
  }};
}

Binary::it_const_sub_clients Binary::subclients() const {
  return {commands_, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return SubClient::classof(cmd.get());
  }};
}

bool Binary::has_subclients() const {
  return has_command<SubClient>();
}

// SubFramework command
// ++++++++++++++++++++
SubFramework* Binary::sub_framework() {
  return command<SubFramework>();
}

const SubFramework* Binary::sub_framework() const {
  return command<SubFramework>();
}

// DyldEnvironment command
// +++++++++++++++++++++++
DyldEnvironment* Binary::dyld_environment() {
  return command<DyldEnvironment>();
}

const DyldEnvironment* Binary::dyld_environment() const {
  return command<DyldEnvironment>();
}

// EncryptionInfo command
// +++++++++++++++++++++++
EncryptionInfo* Binary::encryption_info() {
  return command<EncryptionInfo>();
}

const EncryptionInfo* Binary::encryption_info() const {
  return command<EncryptionInfo>();
}


// BuildVersion command
// ++++++++++++++++++++
BuildVersion* Binary::build_version() {
  return command<BuildVersion>();
}

const BuildVersion* Binary::build_version() const {
  return command<BuildVersion>();
}

// DyldChainedFixups command
// ++++++++++++++++++++
DyldChainedFixups* Binary::dyld_chained_fixups() {
  return command<DyldChainedFixups>();
}

const DyldChainedFixups* Binary::dyld_chained_fixups() const {
  return command<DyldChainedFixups>();
}

// DyldExportsTrie command
// +++++++++++++++++++++++
DyldExportsTrie* Binary::dyld_exports_trie() {
  return command<DyldExportsTrie>();
}

const DyldExportsTrie* Binary::dyld_exports_trie() const {
  return command<DyldExportsTrie>();
}

// Linker Optimization Hint command
// ++++++++++++++++++++++++++++++++
const LinkerOptHint* Binary::linker_opt_hint() const {
  if (const auto* cmd = get(LoadCommand::TYPE::LINKER_OPTIMIZATION_HINT)) {
    return cmd->as<const LinkerOptHint>();
  }
  return nullptr;
}

// Two Level Hints Command
// ++++++++++++++++++++++++++++++++
const TwoLevelHints* Binary::two_level_hints() const {
  if (const auto* cmd = get(LoadCommand::TYPE::TWOLEVEL_HINTS)) {
    return cmd->as<const TwoLevelHints>();
  }
  return nullptr;
}

// AtomInfo
// ++++++++++++++++++++++++++++++++
const AtomInfo* Binary::atom_info() const {
  if (const auto* cmd = get(LoadCommand::TYPE::ATOM_INFO)) {
    return cmd->as<const AtomInfo>();
  }
  return nullptr;
}

// Notes
// ++++++++++++++++++++++++++++++++
Binary::it_notes Binary::notes() {
  return {commands_, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return NoteCommand::classof(cmd.get());
  }};
}

Binary::it_const_notes Binary::notes() const {
  return {commands_, [] (const std::unique_ptr<LoadCommand>& cmd) {
    return NoteCommand::classof(cmd.get());
  }};
}

// FunctionVariants
// ++++++++++++++++++++++++++++++++
const FunctionVariants* Binary::function_variants() const {
  if (const auto* cmd = get(LoadCommand::TYPE::FUNCTION_VARIANTS)) {
    return cmd->as<const FunctionVariants>();
  }
  return nullptr;
}

// FunctionVariantFixups
// ++++++++++++++++++++++++++++++++
const FunctionVariantFixups* Binary::function_variant_fixups() const {
  if (const auto* cmd = get(LoadCommand::TYPE::FUNCTION_VARIANT_FIXUPS)) {
    return cmd->as<const FunctionVariantFixups>();
  }
  return nullptr;
}

Binary::it_bindings Binary::bindings() const {
  if (const DyldInfo* dyld = dyld_info()) {
    auto begin = BindingInfoIterator(*dyld, 0);
    auto end = BindingInfoIterator(*dyld, dyld->binding_info_.size());
    return make_range(std::move(begin), std::move(end));
  }

  if (const DyldChainedFixups* fixup = dyld_chained_fixups()) {
    auto begin = BindingInfoIterator(*fixup, 0);
    auto end = BindingInfoIterator(*fixup, fixup->all_bindings_.size());
    return make_range(std::move(begin), std::move(end));
  }

  auto begin = BindingInfoIterator(*this, 0);
  auto end = BindingInfoIterator(*this, indirect_bindings_.size());

  return make_range(std::move(begin), std::move(end));
}

result<uint64_t> Binary::get_function_address(const std::string& name) const {
  const std::string alt_name = '_' + name;
  for (const Symbol& sym : symbols()) {
    if (sym.value() == 0) {
      continue;
    }
    if (sym.name() == name || sym.name() == alt_name) {
      return sym.value();
    }
  }
  return LIEF::Binary::get_function_address(name);
}

void Binary::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

Symbol& Binary::add(const Symbol& symbol) {
  symbols_.push_back(std::make_unique<Symbol>(symbol));
  return *symbols_.back();
}

Symbol* Binary::add_local_symbol(uint64_t address, const std::string& name) {
  Symbol* symbol = nullptr;

  auto sym = std::make_unique<Symbol>();
  sym->category_          = Symbol::CATEGORY::LOCAL;
  sym->origin_            = Symbol::ORIGIN::SYMTAB;
  sym->numberof_sections_ = 0;
  sym->description_       = static_cast<uint16_t>(/* N_NO_DEAD_STRIP */0x20);

  sym->value(address);
  sym->name(name);
  symbol = sym.get();
  symbols_.push_back(std::move(sym));
  return symbol;
}

ExportInfo* Binary::add_exported_function(uint64_t address, const std::string& name) {
  if (Symbol* symbol = add_local_symbol(address, name)) {
    if (DyldExportsTrie* exports = dyld_exports_trie()) {
      auto export_info = std::make_unique<ExportInfo>(address, 0);
      export_info->symbol_ = symbol;
      export_info->address(address);
      symbol->export_info_ = export_info.get();

      auto* info = export_info.get();
      exports->add(std::move(export_info));
      return info;
    }

    if (DyldInfo* info = dyld_info()) {
      auto export_info = std::make_unique<ExportInfo>(address, 0);
      export_info->symbol_ = symbol;
      export_info->address(address);
      symbol->export_info_ = export_info.get();

      auto* info_ptr = export_info.get();
      info->add(std::move(export_info));
      return info_ptr;
    }
  }
  return nullptr;
}


const DylibCommand* Binary::find_library(const std::string& name) const {
  auto it = std::find_if(libraries_.begin(), libraries_.end(),
    [&name] (const DylibCommand* cmd) {
      const std::string& libpath = cmd->name();
      return libpath == name || libname(libpath).value_or("") == name;
    }
  );
  return it == libraries_.end() ? nullptr : *it;
}


void Binary::refresh_seg_offset() {
  offset_seg_.clear();
  for (SegmentCommand* segment : segments_) {
    if (!can_cache_segment(*segment)) {
      continue;
    }
    offset_seg_[segment->file_offset()] = segment;
  }
}

Binary::stub_iterator Binary::symbol_stubs() const {
  static stub_iterator empty_iterator(
    Stub::Iterator{},
    Stub::Iterator{}
  );

  std::vector<const Section*> stub_sections;
  stub_sections.reserve(3);

  uint32_t total = 0;

  for (const Section& section : sections()) {
    if (section.type() != Section::TYPE::SYMBOL_STUBS) {
      continue;
    }

    const uint32_t stride = section.reserved2();
    if (stride == 0) {
      continue;
    }

    const uint32_t count = section.content().size() / stride;
    if (count == 0) {
      continue;
    }

    total += count;
    stub_sections.push_back(&section);
  }

  if (stub_sections.empty() || total == 0) {
    return empty_iterator;
  }
  Stub::Iterator begin(
      {header_.cpu_type(), header_.cpu_subtype()},
      std::move(stub_sections), 0
  );
  Stub::Iterator end({}, {}, total);

  return make_range(std::move(begin), std::move(end));
}

bool Binary::can_cache_segment(const SegmentCommand& segment) {
  if (segment.file_offset() > 0 && segment.file_size() > 0) {
    return true;
  }

  if (segment.name() == "__TEXT") {
    // In some cases (c.f. <samples>/MachO/issue_1130.macho)
    // the __TEXT segment can have a file_size set to 0 while it is logically
    // revelant to cache it
    return true;
  }

  return false;
}

Binary::~Binary() = default;

std::ostream& Binary::print(std::ostream& os) const {
  os << "Header" << '\n';
  os << "======" << '\n';

  os << header();
  os << '\n';


  os << "Commands" << '\n';
  os << "========" << '\n';
  for (const LoadCommand& cmd : commands()) {
    os << cmd << '\n';
  }

  os << '\n';

  os << "Sections" << '\n';
  os << "========" << '\n';
  for (const Section& section : sections()) {
    os << section << '\n';
  }

  os << '\n';

  os << "Symbols" << '\n';
  os << "=======" << '\n';
  for (const Symbol& symbol : symbols()) {
    os << symbol << '\n';
  }

  os << '\n';
  return os;
}

}
}

