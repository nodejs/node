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
#include <string>

#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/ChainedPointerAnalysis.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldChainedFixupsCreator.hpp"

#include "MachO/ChainedBindingInfoList.hpp"
#include "MachO/ChainedFixup.hpp"

#include "MachO/Structures.hpp"

#include "logging.hpp"

namespace LIEF::MachO {

struct strong_symbol_t {
  std::string symbol;
  size_t libord = 0;
  friend bool operator==(const strong_symbol_t& lhs, const strong_symbol_t& rhs) {
    return lhs.libord == rhs.libord && lhs.symbol == rhs.symbol;
  }
};

class StrongSymbolHash {
  public:
  size_t operator()(const strong_symbol_t& sym) const noexcept{
    return std::hash<std::string>{}(sym.symbol + std::to_string(sym.libord));
  }
};

class strong_map_t : public std::unordered_map<
  strong_symbol_t, ChainedBindingInfoList*, StrongSymbolHash
>
{};

uint64_t DyldChainedFixupsCreator::binding_rebase_t::addr() const {
  return type == binding_rebase_t::BINDING ?
         binding->address() : fixup->address();
}


DYLD_CHAINED_PTR_FORMAT
  DyldChainedFixupsCreator::pointer_format(const Binary& bin, size_t imp_count)
{
  if (imp_count > MAX_IMPORTS) {
    LIEF_ERR("Too many imports (0x{:06x})", imp_count);
    return DYLD_CHAINED_PTR_FORMAT::NONE;
  }
  // Logic taken from ld64 -- OutputFile::chainedPointerFormat()
  const Header::CPU_TYPE arch = bin.header().cpu_type();
  if (arch == Header::CPU_TYPE::ARM64) {
    if (bin.support_arm64_ptr_auth()) {
      if (imp_count > BIND24_THRESHOLD) {
        return DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24;
      }
      return DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E;
    }
  }
  return DYLD_CHAINED_PTR_FORMAT::PTR_64;
}

result<size_t> DyldChainedFixupsCreator::lib2ord(
    const Binary& bin, const Symbol& sym, const std::string& lib)
{
  {
    int ord = sym.library_ordinal();
    if (Symbol::is_valid_index_ordinal(ord)) {
      return sym.library_ordinal();
    }
    if (lib.empty()) {
      return ord;
    }
  }


  if (auto it = lib2ord_.find(lib); it != lib2ord_.end()) {
    return it->second;
  }

  if (lib.empty()) {
    LIEF_ERR("No library for the symbol: '{}'", sym.name());
    return make_error_code(lief_errors::not_found);
  }

  auto libs = bin.libraries();

  auto it = std::find_if(libs.begin(), libs.end(),
    [&lib] (const DylibCommand& dylib) {
      return dylib.name() == lib;
    }
  );

  if (it == libs.end()) {
    LIEF_ERR("Library '{}' not found!", lib);
    return make_error_code(lief_errors::not_found);
  }

  size_t pos = std::distance(libs.begin(), it);
  lib2ord_.insert({lib, pos});
  return pos;
}


const Symbol* DyldChainedFixupsCreator::find_symbol(
  const Binary& bin, const std::string& name)
{
  for (const Symbol& sym : bin.symbols()) {
    if (sym.name() == name) {
      return &sym;
    }
  }
  return nullptr;
}

DyldChainedFixupsCreator& DyldChainedFixupsCreator::add_binding(
    uint64_t address, std::string symbol, std::string library,
    uint64_t addend, bool weak)
{
  bindings_.push_back({
    address, std::move(library), std::move(symbol), weak, addend
  });
  return *this;
}


ok_error_t DyldChainedFixupsCreator::process_relocations(
  Binary& target, DYLD_CHAINED_PTR_FORMAT ptr_fmt)
{
  const uint64_t imagebase = target.imagebase();
  for (const reloc_info_t& info : relocations_) {
    auto fixup = std::make_unique<RelocationFixup>(ptr_fmt, target.imagebase());
    fixup->address_ = info.addr;
    SegmentCommand* segment = target.segment_from_virtual_address(fixup->address());
    if (segment == nullptr) {
      LIEF_WARN("Can't find segment for the relocation: 0x{:016x}", fixup->address());
      continue;
    }

    switch (ptr_fmt) {
      case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
      case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
        {
          details::dyld_chained_ptr_arm64e_rebase raw;
          std::memset(&raw, 0, sizeof(raw));
          details::pack_target(raw, info.target - imagebase);
          fixup->set(raw);
          break;
        }
      case DYLD_CHAINED_PTR_FORMAT::PTR_64:
        {
          details::dyld_chained_ptr_64_rebase raw;
          std::memset(&raw, 0, sizeof(raw));
          details::pack_target(raw, info.target - imagebase);
          fixup->set(raw);
          break;
        }
      default:
        {
          LIEF_WARN("Chained ptr format: {} is not supported", to_string(ptr_fmt));
          break;
        }
    }
    segment_chains_[segment].emplace_back(*fixup);
    segment->relocations_.push_back(std::move(fixup));
  }
  return ok();
}

ok_error_t DyldChainedFixupsCreator::process_bindings(
  Binary& target, strong_map_t& strong_map,
  std::unordered_map<std::string, size_t>& symbols_idx,
  DyldChainedFixups* cmd,
  DyldChainedFixups::binding_info_t& all_bindings
)
{
  for (const binding_info_t& info : bindings_) {
    const Symbol* sym = find_symbol(target, info.symbol);

    if (sym == nullptr) {
      LIEF_ERR("Can't resolve symbol: '{}", info.symbol);
      continue;
    }

    result<size_t> libord = lib2ord(target, *sym, info.library);
    if (!libord) {
      continue;
    }

    SegmentCommand* segment = target.segment_from_virtual_address(info.address);
    if (segment == nullptr) {
      LIEF_ERR("Can't find segment associated with address: 0x{:016x}", info.address);
      continue;
    }

    const size_t ord = *libord;

    auto binding = std::make_unique<ChainedBindingInfo>(imports_format_, info.weak);
    binding->symbol_ = const_cast<Symbol*>(sym);
    binding->library_ordinal_ = ord;
    binding->addend_ = info.addend;
    binding->address_ = info.address;
    binding->segment_ = segment;

    auto libraries = target.libraries();

    if (ord > 0 && ord < libraries.size()) {
      binding->library_ = &libraries[ord - 1];
    }

    segment_chains_[segment].emplace_back(*binding);
    strong_symbol_t strong_sym{sym->name(), ord};
    auto it = strong_map.find(strong_sym);
    if (it != strong_map.end()) {
      it->second->elements_.push_back(binding.get());
    } else {
      symbols_idx.insert({info.symbol, cmd->internal_bindings_.size()});
      auto binding_list = ChainedBindingInfoList::create(*binding);
      binding_list->elements_.push_back(binding.get());
      strong_map.insert({strong_sym, binding_list.get()});
      cmd->internal_bindings_.push_back(std::move(binding_list));
    }

    all_bindings.push_back(std::move(binding));
  }
  return ok();
}

DyldChainedFixups* DyldChainedFixupsCreator::create(Binary& target) {
  const uint64_t imagebase = target.imagebase();
  auto cmd = std::make_unique<DyldChainedFixups>();
  cmd->command_ = LoadCommand::TYPE::DYLD_CHAINED_FIXUPS;
  cmd->size_ = sizeof(details::linkedit_data_command);

  cmd->fixups_version_ = fixups_version_;
  cmd->symbols_format_ = /* uncompressed */0;
  cmd->imports_format_ = DYLD_CHAINED_FORMAT::IMPORT;

  DyldChainedFixups::binding_info_t all_bindings;
  all_bindings.reserve(bindings_.size());

  std::unordered_map<std::string, size_t> symbols_idx;
  strong_map_t strong_map;
  if (!process_bindings(target, strong_map, symbols_idx, cmd.get(), all_bindings)) {
    return nullptr;
  }

  const size_t import_count = cmd->internal_bindings_.size();
  const DYLD_CHAINED_PTR_FORMAT import_ptr_fmt = pointer_format(target, import_count);

  if (!process_relocations(target, import_ptr_fmt)) {
    return nullptr;
  }

  for (auto& [k, v] : segment_chains_) {
    std::sort(v.begin(), v.end());
  }

  const uint32_t page_size = target.page_size();

  for (SegmentCommand& segment : target.segments()) {
    LIEF_DEBUG("Processing segment: {}", segment.name());
    DyldChainedFixups::chained_starts_in_segment info(/*offset=*/0, segment);
    info.page_size = page_size;
    info.pointer_format = import_ptr_fmt;

    const size_t stride = ChainedPointerAnalysis::stride(info.pointer_format);

    auto it = segment_chains_.find(&segment);
    if (it != segment_chains_.end()) {
      std::vector<binding_rebase_t>& chains = it->second;
      LIEF_DEBUG("  {}: #{} values", segment.name(), chains.size());
      const uint64_t segment_offset = segment.virtual_address() - imagebase;

      for (size_t i = 0; i < chains.size(); ++i) {
        uint32_t next = 0;
        binding_rebase_t& element = chains[i];
        //LIEF_DEBUG("    0x{:016x}: {}", binding->address(), binding->symbol()->name());
        uint64_t address = element.addr();
        uint64_t target_offset = address - imagebase;
        LIEF_DEBUG("      address:        0x{:016x}", address);
        LIEF_DEBUG("      target  offset: 0x{:016x}", target_offset);
        LIEF_DEBUG("      segment offset: 0x{:016x}", segment_offset);
        assert (target_offset >= segment_offset);
        uint64_t offset = target_offset - segment_offset;

        uint32_t page_idx = offset / page_size;
        uint32_t delta = offset % page_size;
        LIEF_DEBUG("      page_idx: {}", page_idx);
        LIEF_DEBUG("      delta: 0x{:016x}", delta);
        LIEF_DEBUG("      offset: 0x{:016x}" , offset);

        if (i < chains.size() - 1) {
          binding_rebase_t& next_element = chains[i + 1];
          uint64_t next_address = next_element.addr();
          uint64_t next_target_offset = next_address - imagebase;
          uint64_t next_offset = next_target_offset - segment_offset;
          assert (next_target_offset >= segment_offset);

          uint32_t next_page_idx = next_offset / page_size;
          uint32_t next_delta = next_offset % page_size;
          LIEF_DEBUG("        [next]address:        0x{:016x}", address);
          LIEF_DEBUG("        [next]target  offset: 0x{:016x}", next_target_offset);

          LIEF_DEBUG("        [next]page_idx: {}", next_page_idx);
          LIEF_DEBUG("        [next]delta: 0x{:016x}", next_delta);
          LIEF_DEBUG("        [next]offset: 0x{:016x}" , next_offset);

          if (next_page_idx == page_idx) {
            assert((next_delta - delta) % stride == 0);
            next = (next_delta - delta) / stride;
          }
        }
        LIEF_DEBUG("      next: {} (stride: {})", next, stride);

        if (page_idx >= info.page_start.size()) {
          info.page_start.resize(page_idx + 1, /* DYLD_CHAINED_PTR_START_NONE */0xFFFF);
          info.page_start[page_idx] = delta;
        }
        if (element.is_binding()) {
          const uint16_t ordinal = symbols_idx.at(element.binding->symbol()->name());
          switch (import_ptr_fmt) {
            case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
              {
                details::dyld_chained_ptr_arm64e_bind24 raw;
                std::memset(&raw, 0, sizeof(raw));
                raw.bind = 1;
                raw.next = next;
                raw.ordinal = ordinal;
                element.binding->set(raw);
                break;
              }

            case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
              {
                details::dyld_chained_ptr_arm64e_bind raw;
                std::memset(&raw, 0, sizeof(raw));
                raw.bind = 1;
                raw.next = next;
                raw.ordinal = ordinal;
                element.binding->set(raw);
                break;
              }

            case DYLD_CHAINED_PTR_FORMAT::PTR_64:
              {
                details::dyld_chained_ptr_64_bind raw;
                std::memset(&raw, 0, sizeof(raw));
                raw.bind = 1;
                raw.next = next;
                raw.ordinal = ordinal;
                element.binding->set(raw);
                break;
              }
            default:
              {
                LIEF_WARN("Chained ptr format: {} is not supported", to_string(import_ptr_fmt));
                break;
              }
          }

          element.binding->offset(
              (element.binding->address() - segment.virtual_address()) +
              segment.file_offset()
          );
        } else {
          assert(element.is_fixup());
          element.fixup->next(next);
          element.fixup->offset(
              (element.binding->address() - segment.virtual_address()) +
              segment.file_offset()
          );
        }
      }
    }
    LIEF_DEBUG("Adding chained_starts_in_segment[{}] ({} pages)",
        info.segment.name(), info.page_count());
    cmd->chained_starts_in_segment_.push_back(info);
  }

  cmd->all_bindings_ = std::move(all_bindings);
  LoadCommand* added = target.add(std::move(cmd));
  if (added == nullptr) {
    return nullptr;
  }
  return static_cast<DyldChainedFixups*>(added);
}


DyldChainedFixupsCreator&
  DyldChainedFixupsCreator::add_relocation(uint64_t address, uint64_t target)
{
  relocations_.push_back({address, target});
  return *this;
}

}
