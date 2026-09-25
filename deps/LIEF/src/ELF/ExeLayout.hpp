/* Copyright 2021 - 2025 R. Thomas
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
#ifndef LIEF_ELF_EXE_LAYOUT_H
#define LIEF_ELF_EXE_LAYOUT_H
#include <LIEF/types.hpp>
#include <LIEF/visibility.h>
#include <LIEF/ELF/Binary.hpp>
#include <LIEF/ELF/Builder.hpp>
#include <LIEF/ELF/Symbol.hpp>
#include <LIEF/ELF/DynamicEntryArray.hpp>
#include <LIEF/ELF/DynamicEntryLibrary.hpp>
#include <LIEF/ELF/DynamicEntryRpath.hpp>
#include <LIEF/ELF/DynamicEntryRunPath.hpp>
#include <LIEF/ELF/DynamicSharedObject.hpp>
#include <LIEF/ELF/SymbolVersionDefinition.hpp>
#include <LIEF/ELF/SymbolVersionAux.hpp>
#include <LIEF/ELF/SymbolVersionRequirement.hpp>
#include <LIEF/ELF/SymbolVersionAuxRequirement.hpp>
#include <LIEF/ELF/EnumToString.hpp>
#include <LIEF/ELF/Segment.hpp>
#include <LIEF/ELF/Section.hpp>
#include <LIEF/ELF/Relocation.hpp>
#include <LIEF/ELF/Note.hpp>
#include <LIEF/ELF/GnuHash.hpp>
#include <LIEF/ELF/SysvHash.hpp>
#include <LIEF/ELF/utils.hpp>
#include <LIEF/iostream.hpp>
#include <LIEF/errors.hpp>
#include <algorithm>
#include <iterator>

#include "ELF/Structures.hpp"
#include "internal_utils.hpp"
#include "logging.hpp"
#include "Layout.hpp"

namespace LIEF {
namespace ELF {

inline Relocation::TYPE relative_from_arch(ARCH arch) {
  using TYPE = Relocation::TYPE;
  switch (arch) {
    case ARCH::AARCH64:
      return TYPE::AARCH64_RELATIVE;
    case ARCH::ARM:
      return TYPE::ARM_RELATIVE;
    case ARCH::X86_64:
      return TYPE::X86_64_RELATIVE;
    case ARCH::I386:
      return TYPE::X86_RELATIVE;
    case ARCH::PPC:
      return TYPE::PPC_RELATIVE;
    case ARCH::PPC64:
      return TYPE::PPC64_RELATIVE;
    case ARCH::HEXAGON:
      return TYPE::HEX_RELATIVE;
    default:
      return TYPE::UNKNOWN;
  }
  return TYPE::UNKNOWN;
}

/// Compute the size and the offset of the elements
/// needed to rebuild the ELF file.
class LIEF_LOCAL ExeLayout : public Layout {
  public:
  struct sym_verdef_info_t {
    using list_names_t = std::vector<std::string>;
    std::set<list_names_t> names_list;
    std::unordered_map<const SymbolVersionDefinition*, const list_names_t*> def_to_names;
    std::unordered_map<const list_names_t*, size_t> names_offset;
  };
  using Layout::Layout;

  ExeLayout(const ExeLayout&) = delete;
  ExeLayout& operator=(const ExeLayout&) = delete;

  ExeLayout(ExeLayout&&) = default;
  ExeLayout& operator=(ExeLayout&&) = default;

  template<class ELF_T>
  size_t dynamic_size() {
    // The size of the .dynamic / PT_DYNAMIC area
    // is the number of elements times the size of each element (Elf64_Dyn or Elf32_Dyn)
    using Elf_Dyn = typename ELF_T::Elf_Dyn;
    return binary_->dynamic_entries_.size() * sizeof(Elf_Dyn);
  }

  template<class ELF_T>
  size_t dynstr_size() {
    // The .dynstr section contains:
    // - library names (DT_NEEDED / DT_SONAME / DT_RPATH / DT_RUNPATH)
    // - The symbol names from the .dynsym section
    // - Names associated with:
    //   * Symbol definition
    //   * Symbol version requirement
    //   * Symbol version definition
    if (!raw_dynstr_.empty()) {
      return raw_dynstr_.size();
    }

    // Start with dynamic entries: NEEDED / SONAME etc
    vector_iostream raw_dynstr(should_swap());
    raw_dynstr.write<uint8_t>(0);

    std::vector<std::string> opt_list;

    std::transform(binary_->dynamic_symbols_.begin(),
                   binary_->dynamic_symbols_.end(),
                   std::back_inserter(opt_list),
                   [] (const std::unique_ptr<Symbol>& sym) {
                     return sym->name();
                   });

    for (std::unique_ptr<DynamicEntry>& entry : binary_->dynamic_entries_) {
      switch (entry->tag()) {
      case DynamicEntry::TAG::NEEDED:
        {
          const std::string& name = entry->as<DynamicEntryLibrary>()->name();
          opt_list.push_back(name);
          break;
        }

      case DynamicEntry::TAG::SONAME:
        {
          const std::string& name = entry->as<DynamicSharedObject>()->name();
          opt_list.push_back(name);
          break;
        }

      case DynamicEntry::TAG::RPATH:
        {
          const std::string& name = entry->as<DynamicEntryRpath>()->rpath();
          opt_list.push_back(name);
          break;
        }

      case DynamicEntry::TAG::RUNPATH:
        {
          const std::string& name = entry->as<DynamicEntryRunPath>()->runpath();
          opt_list.push_back(name);
          break;
        }

      default: {}
      }
    }
    // Symbol definition
    for (const SymbolVersionDefinition& svd: binary_->symbols_version_definition()) {
      sym_verdef_info_t::list_names_t aux_names;
      auto saux = svd.symbols_aux();
      aux_names.reserve(saux.size());
      for (const SymbolVersionAux& sva : saux) {
        const std::string& sva_name = sva.name();
        aux_names.push_back(sva_name);
        opt_list.push_back(sva_name);
      }
      auto res = verdef_info_.names_list.insert(std::move(aux_names));
      verdef_info_.def_to_names[&svd] = &*res.first;
    }

    // Symbol version requirement
    for (const SymbolVersionRequirement& svr: binary_->symbols_version_requirement()) {
      const std::string& libname = svr.name();
      opt_list.push_back(libname);
      for (const SymbolVersionAuxRequirement& svar : svr.auxiliary_symbols()) {
        const std::string& name = svar.name();
        opt_list.push_back(name);
      }
    }

    size_t offset_counter = raw_dynstr.tellp();

    std::vector<std::string> string_table_optimized = optimize(opt_list,
                     [] (const std::string& name) { return name; },
                     offset_counter, &offset_name_map_);

    for (const std::string& name : string_table_optimized) {
      raw_dynstr.write(name);
    }

    raw_dynstr.move(raw_dynstr_);
    return raw_dynstr_.size();
  }

  template<class ELF_T>
  size_t dynsym_size() {
    using Elf_Sym = typename ELF_T::Elf_Sym;
    return binary_->dynamic_symbols_.size() * sizeof(Elf_Sym);
  }

  template<class ELF_T>
  size_t static_sym_size() {
    using Elf_Sym = typename ELF_T::Elf_Sym;
    return binary_->symtab_symbols_.size() * sizeof(Elf_Sym);
  }

  template<class ELF_T>
  size_t dynamic_arraysize(DynamicEntry::TAG tag) {
    using uint = typename ELF_T::uint;
    DynamicEntry* entry = binary_->get(tag);
    if (entry == nullptr || !DynamicEntryArray::classof(entry)) {
      return 0;
    }
    return entry->as<const DynamicEntryArray&>()->size() * sizeof(uint);
  }

  template<class ELF_T>
  size_t note_size() {
    if (!raw_notes_.empty()) {
      return raw_notes_.size();
    }

    vector_iostream raw_notes(should_swap());
    for (const Note& note : binary_->notes()) {
      size_t pos = raw_notes.tellp();
      // First we have to write the length of the Note's name
      const auto namesz = static_cast<uint32_t>(note.name().size() + 1);
      raw_notes.write<uint32_t>(namesz);

      // Then the length of the Note's description
      const auto descsz = static_cast<uint32_t>(note.description().size());
      raw_notes.write<uint32_t>(descsz);

      // Then the note's type
      const uint32_t type = note.original_type();
      raw_notes.write<uint32_t>(type);

      // Then we write the note's name
      const std::string& name = note.name();
      raw_notes.write(name);

      // Alignment
      raw_notes.align(sizeof(uint32_t), 0);

      // description content (manipulated in 4 byte/uint32_t chunks)
      span<const uint8_t> description = note.description();
      const auto* desc_ptr = reinterpret_cast<const uint32_t*>(description.data());
      size_t i = 0;
      for (; i < description.size() / sizeof(uint32_t); i++) {
        raw_notes.write<uint32_t>(desc_ptr[i]);
      }
      if (description.size() % sizeof(uint32_t) != 0) {
        uint32_t padded = 0;
        auto *ptr = reinterpret_cast<uint8_t*>(&padded);
        memcpy(ptr, desc_ptr + i, description.size() % sizeof(uint32_t));
        raw_notes.write<uint32_t>(padded);
      }
      notes_off_map_.emplace(&note, pos);
    }
    raw_notes.move(raw_notes_);
    return raw_notes_.size();
  }

  template<class ELF_T>
  size_t symbol_sysv_hash_size() {
    const SysvHash* sysv_hash = binary_->sysv_hash();
    if (sysv_hash == nullptr) {
      return 0;
    }
    nchain_ = sysv_hash->nchain();
    if (nchain_ != binary_->dynamic_symbols_.size()) {
      LIEF_DEBUG("nchain of .hash section changes from {:d} to {:d}",
                 nchain_, binary_->dynamic_symbols_.size());
      nchain_ = binary_->dynamic_symbols_.size();
    }
    return (sysv_hash->nbucket() + nchain_ + /* header */ 2) * sizeof(uint32_t);
  }

  template<class ELF_T>
  size_t section_table_size() {
    using Elf_Shdr = typename ELF_T::Elf_Shdr;
    return binary_->sections_.size() * sizeof(Elf_Shdr);
  }

  template<class ELF_T>
  size_t symbol_gnu_hash_size() {
    // Mainly inspired from
    // * https://github.com/llvm-mirror/lld/blob/master/ELF/SyntheticSections.cpp
    //
    // Checking is performed here:
    // * https://github.com/lattera/glibc/blob/a2f34833b1042d5d8eeb263b4cf4caaea138c4ad/elf/dl-lookup.c#L228
    //
    // See also:
    // * p.9, https://www.akkadia.org/drepper/dsohowto.pdf
    using uint = typename ELF_T::uint;
    if (!raw_gnu_hash_.empty()) {
      return raw_gnu_hash_.size();
    }
    uint32_t first_exported_symbol_index = 0;
    if (new_symndx_ >= 0) {
      first_exported_symbol_index = new_symndx_;
    } else {
      LIEF_WARN("First exported symbol index not set");
    }

    const GnuHash* gnu_hash = binary_->gnu_hash();
    if (gnu_hash == nullptr) {
      return 0;
    }

    const uint32_t nb_buckets = gnu_hash->nb_buckets();
    const uint32_t symndx     = first_exported_symbol_index;
    const uint32_t maskwords  = gnu_hash->maskwords();
    const uint32_t shift2     = gnu_hash->shift2();

    const std::vector<uint64_t>& filters = gnu_hash->bloom_filters();
    if (!filters.empty() && filters[0] == 0) {
      LIEF_DEBUG("Bloom filter is null");
    }

    if (shift2 == 0) {
      LIEF_DEBUG("Shift2 is null");
    }

    LIEF_DEBUG("Number of buckets       : 0x{:x}", nb_buckets);
    LIEF_DEBUG("First symbol idx        : 0x{:x}", symndx);
    LIEF_DEBUG("Number of bloom filters : 0x{:x}", maskwords);
    LIEF_DEBUG("Shift                   : 0x{:x}", shift2);

    // MANDATORY !
    std::stable_sort(
        std::begin(binary_->dynamic_symbols_) + symndx, std::end(binary_->dynamic_symbols_),
        [&nb_buckets] (const std::unique_ptr<Symbol>& lhs, const std::unique_ptr<Symbol>& rhs) {
          return (dl_new_hash(lhs->name().c_str()) % nb_buckets) <
                 (dl_new_hash(rhs->name().c_str()) % nb_buckets);
      });
    Binary::it_dynamic_symbols dynamic_symbols = binary_->dynamic_symbols();

    vector_iostream raw_gnuhash(should_swap());
    raw_gnuhash.reserve(
        4 * sizeof(uint32_t) +          // header
        maskwords * sizeof(uint) +    // bloom filters
        nb_buckets * sizeof(uint32_t) + // buckets
        (dynamic_symbols.size() - symndx) * sizeof(uint32_t)); // hash values

    // Write header
    // =================================
    raw_gnuhash
      .write<uint32_t>(nb_buckets)
      .write<uint32_t>(symndx)
      .write<uint32_t>(maskwords)
      .write<uint32_t>(shift2);

    // Compute Bloom filters
    // =================================
    std::vector<uint> bloom_filters(maskwords, 0);
    size_t C = sizeof(uint) * 8; // 32 for ELF, 64 for ELF64

    for (size_t i = symndx; i < dynamic_symbols.size(); ++i) {
      const uint32_t hash = dl_new_hash(dynamic_symbols[i].name().c_str());
      const size_t pos = (hash / C) & (gnu_hash->maskwords() - 1);
      uint V = (static_cast<uint>(1) << (hash % C)) |
               (static_cast<uint>(1) << ((hash >> gnu_hash->shift2()) % C));
      bloom_filters[pos] |= V;
    }
    for (size_t idx = 0; idx < bloom_filters.size(); ++idx) {
     LIEF_DEBUG("Bloom filter [{:d}]: 0x{:x}", idx, bloom_filters[idx]);
    }

    raw_gnuhash.write(bloom_filters);

    // Write buckets and hash
    // =================================
    int previous_bucket = -1;
    size_t hash_value_idx = 0;
    std::vector<uint32_t> buckets(nb_buckets, 0);
    std::vector<uint32_t> hash_values(dynamic_symbols.size() - symndx, 0);

    for (size_t i = symndx; i < dynamic_symbols.size(); ++i) {
      LIEF_DEBUG("Dealing with symbol {}", to_string(dynamic_symbols[i]));
      const uint32_t hash = dl_new_hash(dynamic_symbols[i].name().c_str());
      int bucket = hash % nb_buckets;

      if (bucket < previous_bucket) {
        LIEF_ERR("Previous bucket is greater than the current one ({} < {})",
                 bucket, previous_bucket);
        return 0;
      }

      if (bucket != previous_bucket) {
        buckets[bucket] = i;
        previous_bucket = bucket;
        if (hash_value_idx > 0) {
          hash_values[hash_value_idx - 1] |= 1;
        }
      }

      hash_values[hash_value_idx] = hash & ~1;
      ++hash_value_idx;
    }

    if (hash_value_idx > 0) {
      hash_values[hash_value_idx - 1] |= 1;
    }

    raw_gnuhash
      .write(buckets)
      .write(hash_values);
    raw_gnuhash.move(raw_gnu_hash_);
    return raw_gnu_hash_.size();
  }

  template<class ELF_T>
  size_t dynamic_relocations_size() {
    using Elf_Rela = typename ELF_T::Elf_Rela;
    using Elf_Rel  = typename ELF_T::Elf_Rel;
    const Binary::it_dynamic_relocations& dyn_relocs = binary_->dynamic_relocations();
    const size_t nb_rel_a = std::count_if(dyn_relocs.begin(), dyn_relocs.end(),
      [] (const Relocation& R) {
        return R.is_rel() || R.is_rela();
      }
    );

    const size_t computed_size = binary_->has(DynamicEntry::TAG::RELA) ?
                                 nb_rel_a * sizeof(Elf_Rela) :
                                 nb_rel_a * sizeof(Elf_Rel);
    return computed_size;
  }

  template<class ELF_T>
  size_t pltgot_relocations_size() {
    using Elf_Rela   = typename ELF_T::Elf_Rela;
    using Elf_Rel    = typename ELF_T::Elf_Rel;
    const Binary::it_pltgot_relocations& pltgot_relocs = binary_->pltgot_relocations();

    const DynamicEntry* dt_rela = binary_->get(DynamicEntry::TAG::PLTREL);

    const ARCH arch = binary_->header().machine_type();

    const bool is_rela = dt_rela != nullptr &&
                         DynamicEntry::from_value(dt_rela->value(), arch) == DynamicEntry::TAG::RELA;

    if (is_rela) {
      return pltgot_relocs.size() * sizeof(Elf_Rela);
    }
    return pltgot_relocs.size() * sizeof(Elf_Rel);
  }

  template<class ELF_T>
  size_t symbol_version() {
    return binary_->symbol_version_table_.size() * sizeof(uint16_t);
  }

  template<class ELF_T>
  size_t symbol_vdef_size() {
    using Elf_Verdef  = typename ELF_T::Elf_Verdef;
    using Elf_Verdaux = typename ELF_T::Elf_Verdaux;
    CHECK_FATAL(!binary_->symbols_version_definition().empty() &&
                verdef_info_.def_to_names.empty(), "Inconsistent state");
    size_t computed_size = binary_->symbols_version_definition().size() * sizeof(Elf_Verdef);

    for (const sym_verdef_info_t::list_names_t& names : verdef_info_.names_list) {
      computed_size += sizeof(Elf_Verdaux) * names.size();
    }
    return computed_size;
  }

  template<class ELF_T>
  size_t symbol_vreq_size() {
    using Elf_Verneed = typename ELF_T::Elf_Verneed;
    using Elf_Vernaux = typename ELF_T::Elf_Vernaux;
    size_t computed_size = 0;
    for (const SymbolVersionRequirement& svr: binary_->symbols_version_requirement()) {
      computed_size += sizeof(Elf_Verneed) + svr.auxiliary_symbols().size() * sizeof(Elf_Vernaux);
    }
    return computed_size;
  }

  template<class ELF_T>
  size_t interpreter_size() {
    // Access private field directly as
    // we want to avoid has_interpreter() check
    return binary_->interpreter_.size() + 1;
  }

  template<class ELF_T>
  size_t android_relocations_size(bool force = false) {
    static constexpr uint64_t GROUPED_BY_INFO_FLAG         = 1 << 0;
    static constexpr uint64_t GROUPED_BY_OFFSET_DELTA_FLAG = 1 << 1;
    /* static constexpr uint64_t GROUPED_BY_ADDEND_FLAG    = 1 << 2;  */
    static constexpr uint64_t GROUP_HAS_ADDEND_FLAG        = 1 << 3;

    using Elf_Xword = typename ELF_T::Elf_Xword;

    // This code reproduces what the lld linker is doing for generating the
    // packed relocations. See lld/ELF/SyntheticSections.cpp -
    // AndroidPackedRelocationSection:updateAllocSize
    constexpr size_t wordsize = sizeof(typename ELF_T::Elf_Addr);
    const bool is_rela = binary_->has(DynamicEntry::TAG::ANDROID_RELA);
    const Relocation::TYPE relative_reloc = relative_from_arch(binary_->header().machine_type());
    const uint64_t raw_relative_reloc = Relocation::to_value(relative_reloc);

    const Header::CLASS elf_class = std::is_same_v<ELF_T, details::ELF32> ?
                                    Header::CLASS::ELF32 : Header::CLASS::ELF64;

    if (force) {
      raw_android_rela_.clear();
    }

    if (!raw_android_rela_.empty()) {
      return raw_android_rela_.size();
    }

    std::vector<const Relocation*> android_relocs;
    std::vector<const Relocation*> relative_rels;
    std::vector<const Relocation*> non_relative_rels;
    android_relocs.reserve(20);

    for (const Relocation& R : binary_->relocations()) {
      if (!R.is_android_packed()) {
        continue;
      }
      android_relocs.push_back(&R);
      R.is_relative() ? relative_rels.push_back(&R) :
                        non_relative_rels.push_back(&R);
    }

    std::sort(relative_rels.begin(), relative_rels.end(),
      [] (const Relocation* lhs, const Relocation* rhs) {
        return lhs->address() < rhs->address();
      }
    );

    std::vector<const Relocation*> ungrouped_relative;
    std::vector<std::vector<const Relocation*>> relative_groups;
    for (auto i = relative_rels.begin(), e = relative_rels.end(); i != e;) {
      std::vector<const Relocation*> group;
      do {
        group.push_back(*i++);
      } while (i != e && (*(i - 1))->address() + wordsize == (*i)->address());

      if (group.size() < 8) {
        ungrouped_relative.insert(ungrouped_relative.end(),
                                  group.begin(), group.end());
      } else {
        relative_groups.emplace_back(std::move(group));
      }
    }


    std::sort(non_relative_rels.begin(), non_relative_rels.end(),
      [elf_class] (const Relocation* lhs, const Relocation* rhs) {
        if (lhs->r_info(elf_class) != rhs->r_info(elf_class)) {
          return lhs->r_info(elf_class) < rhs->r_info(elf_class);
        }
        if (lhs->addend() != rhs->addend()) {
          return lhs->addend() < rhs->addend();
        }
        return lhs->address() < rhs->address();
      }
    );

    std::vector<const Relocation*> ungrouped_non_relative;
    std::vector<std::vector<const Relocation*>> non_relative_group;

    for (auto i = non_relative_rels.begin(),
              e = non_relative_rels.end(); i != e;)
    {
      auto j = i + 1;
      while (j != e && (*i)->r_info(elf_class) == (*j)->r_info(elf_class) &&
             (!is_rela || (*i)->addend() == (*j)->addend()))
      {
        ++j;
      }

      if ((j - i) < 3 || (is_rela && (*i)->addend() != 0)) {
        ungrouped_non_relative.insert(ungrouped_non_relative.end(), i, j);
      } else {
        non_relative_group.emplace_back(i, j);
      }
      i = j;
    }

    std::sort(ungrouped_non_relative.begin(), ungrouped_non_relative.end(),
      [] (const Relocation* lhs, const Relocation* rhs) {
        return lhs->address() < rhs->address();
      }
    );

    const unsigned has_addend_with_rela = is_rela ? GROUP_HAS_ADDEND_FLAG : 0;
    uint64_t offset = 0;
    uint64_t addend = 0;

    vector_iostream ios(should_swap());
    ios.write('A')
       .write('P')
       .write('S')
       .write('2');

    ios.write_sleb128(android_relocs.size());
    ios.write_sleb128(0);

    for (const std::vector<const Relocation*>& g : relative_groups) {
      ios.write_sleb128(1);
      ios.write_sleb128(GROUPED_BY_OFFSET_DELTA_FLAG | GROUPED_BY_INFO_FLAG |
                        has_addend_with_rela);
      ios.write_sleb128(g[0]->address() - offset);
      ios.write_sleb128(raw_relative_reloc);
      if (is_rela) {
        ios.write_sleb128(g[0]->addend() - addend);
        addend = g[0]->addend();
      }

      ios.write_sleb128(g.size() - 1);
      ios.write_sleb128(GROUPED_BY_OFFSET_DELTA_FLAG | GROUPED_BY_INFO_FLAG |
                        has_addend_with_rela);
      ios.write_sleb128(wordsize);
      ios.write_sleb128(raw_relative_reloc);
      if (is_rela) {
        auto it = g.begin(); ++it;
        for (; it != g.end(); ++it) {
          ios.write_sleb128((*it)->addend() - addend);
          addend = (*it)->addend();
        }
      }
      offset = g.back()->address();
    }

    if (!ungrouped_relative.empty()) {
      ios.write_sleb128(ungrouped_relative.size());
      ios.write_sleb128(GROUPED_BY_INFO_FLAG | has_addend_with_rela);
      ios.write_sleb128(raw_relative_reloc);
      for (const Relocation* R : ungrouped_relative) {
        ios.write_sleb128(R->address() - offset);
        offset = R->address();
        if (is_rela) {
          ios.write_sleb128(R->addend() - addend);
          addend = R->addend();
        }
      }
    }

    for (const std::vector<const Relocation*>& g: non_relative_group) {
      ios.write_sleb128(g.size());
      ios.write_sleb128(GROUPED_BY_INFO_FLAG);
      ios.write_sleb128(static_cast<Elf_Xword>(g[0]->r_info(elf_class)));

      for (const Relocation* R : g) {
        ios.write_sleb128(R->address() - offset);
        offset = R->address();
      }
      addend = 0;
    }

    if (!ungrouped_non_relative.empty()) {
      ios.write_sleb128(ungrouped_non_relative.size());
      ios.write_sleb128(has_addend_with_rela);
      for (const Relocation* R : ungrouped_non_relative) {
        ios.write_sleb128(R->address() - offset);
        offset = R->address();
        ios.write_sleb128(static_cast<Elf_Xword>(R->r_info(elf_class)));
        if (is_rela) {
          ios.write_sleb128(R->addend() - addend);
          addend = R->addend();
        }
      }
    }

    ios.move(raw_android_rela_);
    return raw_android_rela_.size();
  }

  template<class ELF_T>
  size_t relative_relocations_size(bool force = false) {
    // This code is inspired from LLVM-lld:
    // lld/ELF/SyntheticSections.cpp - RelrSection<ELFT>::updateAllocSize
    // https://github.com/llvm/llvm-project/blob/754a8add57098ef71e4a51a9caa0cc175e94377d/lld/ELF/SyntheticSections.cpp#L1997-L2078
    using Elf_Addr = typename ELF_T::Elf_Addr;

    if (force) {
      raw_relr_.clear();
    }

    if (!raw_relr_.empty()) {
      return raw_relr_.size();
    }

    std::vector<const Relocation*> relr_relocs;
    relr_relocs.reserve(20);

    for (const Relocation& R : binary_->relocations()) {
      if (R.is_relatively_encoded()) {
        relr_relocs.push_back(&R);
      }
    }

    std::unique_ptr<uint64_t[]> offsets(new uint64_t[relr_relocs.size()]);
    for (size_t i = 0; i < relr_relocs.size(); ++i) {
      offsets[i] = relr_relocs[i]->address();
    }
    std::sort(offsets.get(), offsets.get() + relr_relocs.size());

    const size_t wordsize = sizeof(Elf_Addr);
    const size_t nbits = wordsize * 8 - 1;

    vector_iostream raw_relr(should_swap());

    for (size_t i = 0, e = relr_relocs.size(); i != e;) {
      raw_relr.write<Elf_Addr>(offsets[i]);
      uint64_t base = offsets[i] + wordsize;
      ++i;

      for (;;) {
        uint64_t bitmap = 0;
        for (; i != e; ++i) {
          uint64_t d = offsets[i] - base;
          if (d >= (nbits * wordsize) || (d % wordsize) != 0) {
            break;
          }
          bitmap |= uint64_t(1) << (d / wordsize);
        }
        if (!bitmap) {
          break;
        }
        raw_relr.write<Elf_Addr>((bitmap << 1) | 1);
        base += nbits * wordsize;
      }
    }
    raw_relr.move(raw_relr_);
    return raw_relr_.size();
  }

  void relocate_dynamic(uint64_t size) {
    dynamic_size_ = size;
  }

  void relocate_dynstr(bool val) {
    relocate_dynstr_ = val;
  }

  void relocate_relr(bool val) {
    relocate_relr_ = val;
  }

  void relocate_android_rela(bool val) {
    relocate_android_rela_ = val;
  }

  void relocate_shstr(bool val) {
    relocate_shstrtab_ = val;
  }

  void relocate_strtab(bool val) {
    relocate_strtab_ = val;
  }

  void relocate_gnu_hash(bool val) {
    relocate_gnu_hash_ = val;
  }

  void relocate_sysv_hash(uint64_t size) {
    sysv_size_ = size;
  }

  void relocate_dynsym(uint64_t size) {
    dynsym_size_ = size;
  }

  void relocate_symver(uint64_t size) {
    sver_size_ = size;
  }

  void relocate_symverd(uint64_t size) {
    sverd_size_ = size;
  }

  void relocate_symverr(uint64_t size) {
    sverr_size_ = size;
  }

  void relocate_preinit_array(uint64_t size) {
    preinit_size_ = size;
  }

  void relocate_init_array(uint64_t size) {
    init_size_ = size;
  }

  void relocate_fini_array(uint64_t size) {
    fini_size_ = size;
  }

  void relocate_dyn_reloc(uint64_t size) {
    dynamic_reloc_size_ = size;
  }

  void relocate_plt_reloc(uint64_t size) {
    pltgot_reloc_size_ = size;
  }

  void relocate_interpreter(uint64_t size) {
    interp_size_ = size;
  }

  void relocate_notes(bool value) {
    relocate_notes_ = value;
  }

  void relocate_symtab(size_t size) {
    symtab_size_ = size;
  }

  const std::vector<uint8_t>& raw_dynstr() const {
    return raw_dynstr_;
  }

  const std::vector<uint8_t>& raw_shstr() const override {
    return raw_shstrtab_;
  }

  const std::vector<uint8_t>& raw_gnuhash() const {
    return raw_gnu_hash_;
  }

  const std::vector<uint8_t>& raw_notes() const {
    return raw_notes_;
  }

  sym_verdef_info_t& verdef_info() {
    return verdef_info_;
  }

  const sym_verdef_info_t& verdef_info() const {
    return verdef_info_;
  }

  const std::vector<uint8_t>& raw_relr() const {
    return raw_relr_;
  }

  const std::vector<uint8_t>& raw_android_rela() const {
    return raw_android_rela_;
  }

  result<bool> relocate() {
    /* PT_INTERP segment (optional)
     *
     */
    if (interp_size_ > 0 && !binary_->has(Segment::TYPE::INTERP)) {
      Segment interp_segment;
      interp_segment.alignment(0x8);
      interp_segment.type(Segment::TYPE::INTERP);
      interp_segment.add(Segment::FLAGS::R);
      interp_segment.content(std::vector<uint8_t>(interp_size_));
      if (Segment* interp = binary_->add(interp_segment)) {
        LIEF_DEBUG("Interp Segment: 0x{:x}:0x{:x}", interp->virtual_address(), interp->virtual_size());
      } else {
        LIEF_ERR("Can't add a new PT_INTERP");
      }
    }

    /* Segment 1.
     *    .interp
     *    .note.*
     *    .gnu.hash
     *    .hash
     *    .dynsym
     *    .dynstr
     *    .gnu.version
     *    .gnu.version_d
     *    .gnu.version_r
     *    .rela.dyn
     *    .rela.plt
     *    .relr.dyn
     * Perm: READ ONLY
     */
    uint64_t read_segment = interp_size_ +  sysv_size_ + dynsym_size_ +
                            sver_size_ + sverd_size_ + sverr_size_ +
                            dynamic_reloc_size_ + pltgot_reloc_size_;
    if (relocate_relr_) {
      read_segment += raw_relr_.size();
    }

    if (relocate_android_rela_) {
      read_segment += raw_android_rela_.size();
    }

    if (relocate_notes_) {
      read_segment += raw_notes_.size();
    }

    if (relocate_dynstr_) {
      read_segment += raw_dynstr_.size();
    }

    if (relocate_gnu_hash_) {
      read_segment += raw_gnu_hash_.size();
    }

    Segment* new_rsegment = nullptr;

    if (read_segment > 0) {
      Segment rsegment;
      rsegment.type(Segment::TYPE::LOAD);
      rsegment.add(Segment::FLAGS::R);
      rsegment.content(std::vector<uint8_t>(read_segment));
      new_rsegment = binary_->add(rsegment);
      if (new_rsegment != nullptr) {
        LIEF_DEBUG("R-Segment: 0x{:x}:0x{:x}", new_rsegment->virtual_address(), new_rsegment->virtual_size());
      } else {
        LIEF_ERR("Can't add a new R-Segment");
        return make_error_code(lief_errors::build_error);
      }
    }

    /* Segment 2
     *
     *  .init_array
     *  .fini_array
     *  .preinit_array
     *  .prefini_array
     *  .dynamic
     *  .got
     *  .got.plt
     * Perm: READ | WRITE
     */
    const uint64_t read_write_segment = init_size_ + preinit_size_ + fini_size_ + dynamic_size_ ;

    Segment* new_rwsegment = nullptr;
    Segment rwsegment;
    if (read_write_segment > 0) {
      rwsegment.type(Segment::TYPE::LOAD);
      rwsegment.add(Segment::FLAGS::R | Segment::FLAGS::W);
      rwsegment.content(std::vector<uint8_t>(read_write_segment));
      new_rwsegment = binary_->add(rwsegment);
      if (new_rwsegment != nullptr) {
        LIEF_DEBUG("RW-Segment: 0x{:x}:0x{:x}", new_rwsegment->virtual_address(), new_rwsegment->virtual_size());
      } else {
        LIEF_ERR("Can't add a new RW-Segment");
        return make_error_code(lief_errors::build_error);
      }
    }

    if (relocate_shstrtab_) {
      LIEF_DEBUG("[-] Relocate .shstrtab");

      // Remove the current .shstrtab section
      Header& hdr = binary_->header();
      if (hdr.section_name_table_idx() >= binary_->sections_.size()) {
        LIEF_ERR("Sections' names table index is out of range");
        return make_error_code(lief_errors::file_format_error);
      }
      std::unique_ptr<Section>& string_names_section = binary_->sections_[hdr.section_name_table_idx()];
      std::string sec_name = binary_->shstrtab_name();
      binary_->remove(*string_names_section, /* clear */ true);
      Section sec_str_section(sec_name, Section::TYPE::STRTAB);
      sec_str_section.content(std::vector<uint8_t>(raw_shstrtab_.size()));
      binary_->add(sec_str_section, /*loaded=*/false,
                   /*pos=*/Binary::SEC_INSERT_POS::POST_SECTION);

      // Default behavior: push_back => index = binary_->sections_.size() - 1
      hdr.section_name_table_idx(binary_->sections_.size() - 1);
    }

    for (std::unique_ptr<Relocation>& reloc : binary_->relocations_) {
      relocations_addresses_[reloc->address()] = reloc.get();
    }

    [[maybe_unused]] uint64_t va_r_base  = new_rsegment  != nullptr ? new_rsegment->virtual_address() : 0;
    [[maybe_unused]] uint64_t va_rw_base = new_rwsegment != nullptr ? new_rwsegment->virtual_address() : 0;

    if (interp_size_ > 0) {
      Segment* pt_interp = binary_->get(Segment::TYPE::INTERP);
      if (pt_interp == nullptr) {
        LIEF_ERR("Can't find the PT_INTERP segment.");
        return make_error_code(lief_errors::file_format_error);
      }

      Section* section = nullptr;
      Segment::it_sections sections = pt_interp->sections();
      if (!sections.empty()) {
        section = &sections[0];
      }
      pt_interp->virtual_address(va_r_base);
      pt_interp->virtual_size(interp_size_);
      pt_interp->physical_address(va_r_base);
      pt_interp->physical_size(interp_size_);
      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      pt_interp->file_offset(offset_r_base);
      if (section != nullptr) {
        section->virtual_address(va_r_base);
        section->size(interp_size_);
        section->offset(offset_r_base);
        section->original_size_ = interp_size_;
      }

      va_r_base += interp_size_;

    }

    if (relocate_notes_) {
      Segment* note_segment = binary_->get(Segment::TYPE::NOTE);
      if (note_segment == nullptr) {
        LIEF_ERR("Can't find the PT_NOTE segment");
        return make_error_code(lief_errors::file_format_error);
      }
      note_segment->virtual_address(va_r_base);
      note_segment->virtual_size(raw_notes_.size());
      note_segment->physical_address(va_r_base);
      note_segment->physical_size(raw_notes_.size());

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      note_segment->file_offset(offset_r_base);
      va_r_base += raw_notes_.size();
    }

    if (dynamic_size_ > 0) {
      // Update .dynamic / PT_DYNAMIC
      // Update relocations associated with .init_array etc
      Segment* dynamic_segment = binary_->get(Segment::TYPE::DYNAMIC);
      if (dynamic_segment == nullptr) {
        LIEF_ERR("Can't find the dynamic section/segment");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_rw_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_rw_base)) {
        offset_rw_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      dynamic_segment->virtual_address(va_rw_base);
      dynamic_segment->virtual_size(dynamic_size_);
      dynamic_segment->physical_address(va_rw_base);
      dynamic_segment->file_offset(offset_rw_base);
      dynamic_segment->physical_size(dynamic_size_);

      if (Section* section = binary_->dynamic_section()) {
        section->virtual_address(va_rw_base);
        section->size(dynamic_size_);
        section->offset(offset_rw_base);
        section->original_size_ = dynamic_size_;
      }

      va_rw_base += dynamic_size_;
    }

    if (dynsym_size_ > 0) {
      // Update .dynsym / DT_SYMTAB
      DynamicEntry* dt_symtab = binary_->get(DynamicEntry::TAG::SYMTAB);

      if (dt_symtab == nullptr) {
        LIEF_ERR("Can't find DT_SYMTAB");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section  = binary_->section_from_virtual_address(dt_symtab->value())) {
        section->virtual_address(va_r_base);
        section->size(dynsym_size_);
        section->offset(offset_r_base);
        section->original_size_ = dynsym_size_;
      }

      dt_symtab->value(va_r_base);

      va_r_base += dynsym_size_;
    }

    if (relocate_dynstr_) {
      // Update .dynstr section, DT_SYMTAB, DT_STRSZ
      DynamicEntry* dt_strtab  = binary_->get(DynamicEntry::TAG::STRTAB);
      DynamicEntry* dt_strsize = binary_->get(DynamicEntry::TAG::STRSZ);

      if (dt_strtab == nullptr || dt_strsize == nullptr) {
        LIEF_ERR("Can't find DT_STRTAB/DT_STRSZ");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_strtab->value())) {
        section->virtual_address(va_r_base);
        section->size(raw_dynstr_.size());
        section->offset(offset_r_base);
        section->original_size_ = raw_dynstr_.size();
      }

      dt_strtab->value(va_r_base);
      dt_strsize->value(raw_dynstr_.size());

      va_r_base += raw_dynstr_.size();
    }


    if (sver_size_ > 0) {
      DynamicEntry* dt_versym = binary_->get(DynamicEntry::TAG::VERSYM);
      if (dt_versym == nullptr) {
        LIEF_ERR("Can't find DT_VERSYM");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_versym->value())) {
        section->virtual_address(va_r_base);
        section->size(sver_size_);
        section->offset(offset_r_base);
        section->original_size_ = sver_size_;
      }

      dt_versym->value(va_r_base);

      va_r_base += sver_size_;
    }

    if (sverd_size_ > 0) {
      DynamicEntry* dt_verdef = binary_->get(DynamicEntry::TAG::VERDEF);

      if (dt_verdef == nullptr) {
        LIEF_ERR("Can't find DT_VERDEF");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_verdef->value())) {
        section->virtual_address(va_r_base);
        section->size(sverd_size_);
        section->offset(offset_r_base);
        section->original_size_ = sverd_size_;
      }

      dt_verdef->value(va_r_base);

      va_r_base += sverd_size_;
    }

    if (sverr_size_ > 0) {
      DynamicEntry* dt_verreq = binary_->get(DynamicEntry::TAG::VERNEED);

      if (dt_verreq == nullptr) {
        LIEF_ERR("Can't find DT_VERNEED");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_verreq->value())) {
        section->virtual_address(va_r_base);
        section->size(sverr_size_);
        section->offset(offset_r_base);
        section->original_size_ = sverr_size_;
      }

      dt_verreq->value(va_r_base);

      va_r_base += sverr_size_;
    }

    if (dynamic_reloc_size_ > 0) {
      // Update:
      // - DT_REL / DT_RELA
      // - DT_RELSZ / DT_RELASZ
      // - .dyn.rel

      DynamicEntry* dt_rela = binary_->get(DynamicEntry::TAG::RELA);

      const bool is_rela = dt_rela != nullptr;
      DynamicEntry* dt_reloc   = is_rela ? dt_rela : binary_->get(DynamicEntry::TAG::REL);
      DynamicEntry* dt_relocsz = is_rela ? binary_->get(DynamicEntry::TAG::RELASZ) :
                                           binary_->get(DynamicEntry::TAG::RELSZ);

      if (dt_reloc == nullptr || dt_relocsz == nullptr) {
        LIEF_ERR("Can't find DT_REL(A) / DT_REL(A)SZ");
        return make_error_code(lief_errors::file_format_error);
      }


      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_reloc->value())) {
        section->virtual_address(va_r_base);
        section->size(dynamic_reloc_size_);
        section->offset(offset_r_base);
        section->original_size_ = dynamic_reloc_size_;
      }

      dt_reloc->value(va_r_base);
      dt_relocsz->value(dynamic_reloc_size_);

      va_r_base += dynamic_reloc_size_;
    }

    if (pltgot_reloc_size_ > 0) {
      // Update:
      // - DT_JMPREL / DT_PLTRELSZ
      // - .plt.rel
      DynamicEntry* dt_reloc = binary_->get(DynamicEntry::TAG::JMPREL);
      DynamicEntry* dt_relocsz = binary_->get(DynamicEntry::TAG::PLTRELSZ);

      if (dt_reloc == nullptr || dt_relocsz == nullptr) {
        LIEF_ERR("Can't find DT_JMPREL, DT_PLTRELSZ");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_reloc->value())) {
        section->virtual_address(va_r_base);
        section->size(pltgot_reloc_size_);
        section->offset(offset_r_base);
        section->original_size_ = pltgot_reloc_size_;
      }


      dt_reloc->value(va_r_base);
      dt_relocsz->value(pltgot_reloc_size_);

      va_r_base += pltgot_reloc_size_;
    }

    if (relocate_relr_) {
      DynamicEntry* dt_relr = binary_->get(DynamicEntry::TAG::RELR);
      if (dt_relr == nullptr) {
        LIEF_ERR("Can't find DT_RELR");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_relr->value())) {
        section->virtual_address(va_r_base);
        section->size(raw_relr_.size());
        section->offset(offset_r_base);
        section->original_size_ = raw_relr_.size();
      }

      dt_relr->value(va_r_base);
      va_r_base += raw_relr_.size();
    }

    if (relocate_android_rela_) {
      DynamicEntry* dt_rel = binary_->get(DynamicEntry::TAG::ANDROID_RELA);
      if (dt_rel == nullptr) {
        dt_rel = binary_->get(DynamicEntry::TAG::ANDROID_REL);
      }

      if (dt_rel == nullptr) {
        LIEF_ERR("Can't find DT_ANDROID_REL[A]");
        return make_error_code(lief_errors::file_format_error);
      }

      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_rel->value())) {
        section->virtual_address(va_r_base);
        section->size(raw_android_rela_.size());
        section->offset(offset_r_base);
        section->original_size_ = raw_android_rela_.size();
      }

      dt_rel->value(va_r_base);
      va_r_base += raw_android_rela_.size();
    }

    if (relocate_gnu_hash_) {
      // Update .gnu.hash section / DT_GNU_HASH
      DynamicEntry* dt_gnu_hash = binary_->get(DynamicEntry::TAG::GNU_HASH);

      if (dt_gnu_hash == nullptr) {
        LIEF_ERR("Can't find DT_GNU_HASH");
        return make_error_code(lief_errors::file_format_error);
      }


      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_gnu_hash->value())) {
        section->virtual_address(va_r_base);
        section->size(raw_gnu_hash_.size());
        section->offset(offset_r_base);
        section->original_size_ = raw_gnu_hash_.size();
      }

      dt_gnu_hash->value(va_r_base);
      va_r_base += raw_gnu_hash_.size();
    }

    if (sysv_size_ > 0) {
      // Update .hash section / DT_HASH
      DynamicEntry* dt_hash = binary_->get(DynamicEntry::TAG::HASH);

      if (dt_hash == nullptr) {
        LIEF_ERR("Can't find DT_HASH");
        return make_error_code(lief_errors::file_format_error);
      }


      uint64_t offset_r_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_r_base)) {
        offset_r_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->section_from_virtual_address(dt_hash->value())) {
        section->virtual_address(va_r_base);
        section->size(sysv_size_);
        section->offset(offset_r_base);
        section->original_size_ = sysv_size_;
      }

      dt_hash->value(va_r_base);
      va_r_base += sysv_size_;
    }


    // RW-Segment
    // ====================================
    if (init_size_ > 0) {  // .init_array
      DynamicEntry* raw_dt_init = binary_->get(DynamicEntry::TAG::INIT_ARRAY);
      if (raw_dt_init == nullptr || !DynamicEntryArray::classof(raw_dt_init)) {
        LIEF_ERR("DT_INIT_ARRAY not found");
        return make_error_code(lief_errors::file_format_error);
      }
      auto* dt_init_array = raw_dt_init->as<DynamicEntryArray>();
      DynamicEntry* dt_init_arraysz = binary_->get(DynamicEntry::TAG::INIT_ARRAYSZ);

      if (dt_init_arraysz == nullptr) {
        LIEF_ERR("Can't find DT_INIT_ARRAYSZ");
        return make_error_code(lief_errors::file_format_error);
      }


      // Update relocation range
      if (binary_->header().file_type() == Header::FILE_TYPE::DYN) {
        LIEF_WARN("Relocating .init_array might not work on Linux.");
        const std::vector<uint64_t>& array = dt_init_array->array();
        const size_t sizeof_p = binary_->type() == Header::CLASS::ELF32 ?
                                sizeof(uint32_t) : sizeof(uint64_t);

        // Since the values of the .init_array have moved elsewhere,
        // we need to change the relocation associated with the former .init_array
        const uint64_t array_base_address = dt_init_array->value();
        for (size_t i = 0; i < array.size(); ++i) {
          auto it_reloc = relocations_addresses_.find(array_base_address + i * sizeof_p);
          if (it_reloc == std::end(relocations_addresses_)) {
            LIEF_ERR("Missing relocation for .init_array[{:d}]: 0x{:x}", i, array[i]);
            continue;
          }
          Relocation* reloc = it_reloc->second;
          reloc->address(va_rw_base + i * sizeof_p);
        }
      }

      uint64_t offset_rw_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_rw_base)) {
        offset_rw_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->get(Section::TYPE::INIT_ARRAY)) {
        section->virtual_address(va_rw_base);
        section->size(init_size_);
        section->offset(offset_rw_base);
        section->original_size_ = init_size_;
      }

      dt_init_array->value(va_rw_base);
      dt_init_arraysz->value(init_size_);

      va_rw_base += init_size_;
    }

    if (preinit_size_ > 0) { // .preinit_array
      DynamicEntry* raw_dt_preinit = binary_->get(DynamicEntry::TAG::PREINIT_ARRAY);
      if (raw_dt_preinit == nullptr || !DynamicEntryArray::classof(raw_dt_preinit)) {
        LIEF_ERR("DT_PREINIT_ARRAY not found");
        return make_error_code(lief_errors::file_format_error);
      }
      auto* dt_preinit_array = raw_dt_preinit->as<DynamicEntryArray>();
      DynamicEntry* dt_preinit_arraysz = binary_->get(DynamicEntry::TAG::PREINIT_ARRAYSZ);

      if (dt_preinit_array == nullptr) {
        LIEF_ERR("Can't find DT_PREINIT_ARRAYSZ");
        return make_error_code(lief_errors::file_format_error);
      }

      if (binary_->header().file_type() == Header::FILE_TYPE::DYN) {
        const std::vector<uint64_t>& array = dt_preinit_array->array();
        const size_t sizeof_p = binary_->type() == Header::CLASS::ELF32 ?
                                sizeof(uint32_t) : sizeof(uint64_t);
        LIEF_WARN("Relocating .preinit_array might not work on Linux.");

        const uint64_t array_base_address = dt_preinit_array->value();
        for (size_t i = 0; i < array.size(); ++i) {
          auto it_reloc = relocations_addresses_.find(array_base_address + i * sizeof_p);
          if (it_reloc == std::end(relocations_addresses_)) {
            LIEF_ERR("Missing relocation for .preinit_array[{:d}]: 0x{:x}", i, array[i]);
            continue;
          }
          Relocation* reloc = it_reloc->second;
          reloc->address(va_rw_base + i * sizeof_p);
        }
      }

      uint64_t offset_rw_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_rw_base)) {
        offset_rw_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->get(Section::TYPE::PREINIT_ARRAY)) {
        section->virtual_address(va_rw_base);
        section->size(preinit_size_);
        section->offset(offset_rw_base);
        section->original_size_ = preinit_size_;
      }

      dt_preinit_array->value(va_rw_base);
      dt_preinit_arraysz->value(preinit_size_);

      va_rw_base += preinit_size_;
    }


    if (fini_size_ > 0) { // .fini_array
      DynamicEntry* raw_dt_fini = binary_->get(DynamicEntry::TAG::FINI_ARRAY);
      if (raw_dt_fini == nullptr || !DynamicEntryArray::classof(raw_dt_fini)) {
        LIEF_ERR("DT_FINI_ARRAY not found");
        return make_error_code(lief_errors::file_format_error);
      }
      auto* dt_fini_array = raw_dt_fini->as<DynamicEntryArray>();
      DynamicEntry* dt_fini_arraysz = binary_->get(DynamicEntry::TAG::FINI_ARRAYSZ);

      if (dt_fini_arraysz == nullptr) {
        LIEF_ERR("Can't find DT_FINI_ARRAYSZ");
        return make_error_code(lief_errors::file_format_error);
      }

      if (binary_->header().file_type() == Header::FILE_TYPE::DYN) {
        const std::vector<uint64_t>& array = dt_fini_array->array();
        const size_t sizeof_p = binary_->type() == Header::CLASS::ELF32 ?
                                sizeof(uint32_t) : sizeof(uint64_t);

        LIEF_WARN("Relocating .fini_array might not work on Linux.");

        const uint64_t array_base_address = dt_fini_array->value();
        for (size_t i = 0; i < array.size(); ++i) {
          auto it_reloc = relocations_addresses_.find(array_base_address + i * sizeof_p);
          if (it_reloc == std::end(relocations_addresses_)) {
            LIEF_ERR("Missing relocation for .fini_array[{:d}]: 0x{:x}", i, array[i]);
            continue;
          }
          Relocation* reloc = it_reloc->second;
          reloc->address(va_rw_base + i * sizeof_p);
        }
      }

      uint64_t offset_rw_base = 0;
      if (auto res = binary_->virtual_address_to_offset(va_rw_base)) {
        offset_rw_base = *res;
      } else {
        return make_error_code(lief_errors::build_error);
      }

      if (Section* section = binary_->get(Section::TYPE::FINI_ARRAY)) {
        section->virtual_address(va_rw_base);
        section->size(fini_size_);
        section->offset(offset_rw_base);
        section->original_size_ = fini_size_;
      }

      dt_fini_array->value(va_rw_base);
      dt_fini_arraysz->value(fini_size_);
      va_rw_base += fini_size_;
    }

    // Check if we need to relocate the .strtab that contains
    // symbol's names associated with debug symbol (not mandatory)
    size_t strtab_idx = 0;
    if (relocate_strtab_) {
      LIEF_DEBUG("Relocate .strtab");
      if (is_strtab_shared_shstrtab()) {
        LIEF_ERR("Inconsistency"); // The strtab should be located in the .shstrtab section
        return make_error_code(lief_errors::file_format_error);
      }
      if (strtab_section_ != nullptr) {
        LIEF_DEBUG("Removing the old section: {} 0x{:x} (size: 0x{:x})",
                   strtab_section_->name(), strtab_section_->file_offset(),
                   strtab_section_->size());
        binary_->remove(*strtab_section_, /* clear */ true);
      }
      Section strtab{".strtab", Section::TYPE::STRTAB};
      strtab.content(raw_strtab_);
      strtab.alignment(1);
      Section* new_strtab = binary_->add(
        strtab, /*loaded=*/false, /*pos=*/Binary::SEC_INSERT_POS::POST_SECTION);

      strtab_idx = binary_->sections().size() - 1;

      if (new_strtab == nullptr) {
        LIEF_ERR("Can't add a new .strtab section");
        return make_error_code(lief_errors::build_error);
      }

      LIEF_DEBUG("New .strtab section: #{:d} {} 0x{:x} (size: {:x})",
                 strtab_idx, new_strtab->name(), new_strtab->file_offset(), new_strtab->size());

      Section* sec_symtab = binary_->get(Section::TYPE::SYMTAB);
      if (sec_symtab != nullptr) {
        LIEF_DEBUG("Link section {} with the new .strtab (idx: #{:d})", sec_symtab->name(), strtab_idx);
        sec_symtab->link(strtab_idx);
      }
      set_strtab_section(*new_strtab);
    }

    if (strtab_section_ != nullptr) {
      strtab_section_->content(raw_strtab_);
    }
    LIEF_DEBUG("strtab_idx: {:d}", strtab_idx);
    // Sections that are not associated with segments (mostly debug information)
    // currently we only handle the symtab symbol table: .symtab
    if (symtab_size_ > 0) {
      LIEF_DEBUG("Relocate .symtab");

      const auto sections = binary_->sections();
      auto it_sec_symtab = std::find_if(sections.begin(), sections.end(),
          [] (const Section& sec) { return sec.type() == Section::TYPE::SYMTAB; }
      );

      if (it_sec_symtab != sections.end()) {
        const size_t pos = std::distance(sections.begin(), it_sec_symtab);

        if (strtab_idx == 0) {
          strtab_idx = it_sec_symtab->link();
        }

        LIEF_DEBUG("Removing the old section: {} 0x{:x} (size: 0x{:x})",
                   it_sec_symtab->name(), it_sec_symtab->file_offset(), it_sec_symtab->size());
        binary_->remove(*it_sec_symtab, /* clear */ true);
        if (pos < strtab_idx) {
          --strtab_idx;
        }
      }

      Section symtab{".symtab", Section::TYPE::SYMTAB};
      symtab.content(std::vector<uint8_t>(symtab_size_));

      const size_t sizeof_sym = binary_->type() == Header::CLASS::ELF32 ?
                                sizeof(details::Elf32_Sym) : sizeof(details::Elf64_Sym);
      symtab.entry_size(sizeof_sym);
      symtab.alignment(8);
      symtab.link(strtab_idx);
      Section* new_symtab = binary_->add(
        symtab, /*loaded=*/false, /*pos=*/Binary::SEC_INSERT_POS::POST_SECTION);
      if (new_symtab == nullptr) {
        LIEF_ERR("Can't add a new .symbtab section");
        return make_error_code(lief_errors::build_error);
      }
      LIEF_DEBUG("New .symtab section: {} 0x{:x} (size: {:x})",
                 new_symtab->name(), new_symtab->file_offset(), new_symtab->size());
    }

    // Process note sections
    if (const Segment* segment_note = binary_->get(Segment::TYPE::NOTE)) {
      for (const Note& note : binary_->notes()) {
        auto section_res = Note::note_to_section(note);
        const auto& it_offset = notes_off_map_.find(&note);

        if (!section_res) {
          if (binary_->header().file_type() != Header::FILE_TYPE::CORE) {
            LIEF_ERR("Note type: {} ('{}') is not supported",
                     to_string(note.type()), note.name());
          }
          continue;
        }

        std::string sec_name = *section_res;

        // If the binary has the note type but does not have
        // the section (likly because the user added the note manually)
        // then, create the section
        if (const Section* nsec = binary_->get_section(*section_res);
            nsec == nullptr)
        {
          if (it_offset == std::end(notes_off_map_)) {
            LIEF_ERR("Can't find raw data for note: '{}'", to_string(note.type()));
            continue;
          }
          const size_t note_offset = it_offset->second;

          Section section{sec_name, Section::TYPE::NOTE};
          section += Section::FLAGS::ALLOC;

          Section* section_added = binary_->add(
            section, /*loaded=*/false, /*pos=*/Binary::SEC_INSERT_POS::POST_SECTION);
          if (section_added == nullptr) {
            LIEF_ERR("Can't add SHT_NOTE section");
            return make_error_code(lief_errors::build_error);
          }
          section_added->offset(segment_note->file_offset() + note_offset);
          section_added->size(note.size());
          section.virtual_address(segment_note->virtual_address() + note_offset);
          section_added->alignment(4);
        }
      }
    }
    return true;
  }

  const std::unordered_map<std::string, size_t>& dynstr_map() const {
    return offset_name_map_;
  }

  const std::unordered_map<const Note*, size_t>& note_off_map() const {
    return notes_off_map_;
  }

  uint32_t sysv_nchain() const {
    return nchain_;
  }

  ~ExeLayout() override = default;
  ExeLayout() = delete;
  private:

  std::unordered_map<std::string, size_t> offset_name_map_;
  std::unordered_map<const Note*, size_t> notes_off_map_;

  sym_verdef_info_t verdef_info_;

  std::vector<uint8_t> raw_notes_;
  bool relocate_notes_{false};

  std::vector<uint8_t> raw_dynstr_;
  bool relocate_dynstr_{false};

  bool relocate_shstrtab_{false};

  bool relocate_strtab_{false};

  std::vector<uint8_t> raw_gnu_hash_;
  bool relocate_gnu_hash_{false};

  std::vector<uint8_t> raw_relr_;
  bool relocate_relr_{false};

  std::vector<uint8_t> raw_android_rela_;
  bool relocate_android_rela_{false};

  uint64_t sysv_size_{0};

  uint64_t dynamic_size_{0};
  uint64_t dynsym_size_{0};

  uint64_t pltgot_reloc_size_{0};
  uint64_t dynamic_reloc_size_{0};

  uint64_t sver_size_{0};
  uint64_t sverd_size_{0};
  uint64_t sverr_size_{0};

  uint64_t preinit_size_{0};
  uint64_t init_size_{0};
  uint64_t fini_size_{0};

  uint64_t interp_size_{0};
  uint32_t nchain_{0};
  uint64_t symtab_size_{0};

  //uint64_t pltgot_reloc_size_{0};
  std::unordered_map<uint64_t, Relocation*> relocations_addresses_;
};
}
}

#endif
