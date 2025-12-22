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
#ifndef LIEF_ELF_OBJECT_FILE_LAYOUT_H
#define LIEF_ELF_OBJECT_FILE_LAYOUT_H

#include <LIEF/types.hpp>
#include <LIEF/visibility.h>
#include <LIEF/ELF/Binary.hpp>
#include <LIEF/ELF/Section.hpp>
#include <LIEF/ELF/Symbol.hpp>
#include <LIEF/iostream.hpp>

#include "ELF/DataHandler/Handler.hpp"

#include "logging.hpp"
#include "Layout.hpp"
namespace LIEF {
namespace ELF {

/// Class used to compute the size and the offsets of the elements
/// needed to rebuild the ELF file.
class LIEF_LOCAL ObjectFileLayout : public Layout {
  public:
  using relocations_map_t    = std::unordered_map<Section*, std::vector<Relocation*>>; // Relocation associated with a section
  using sections_reloc_map_t = std::unordered_map<Section*, Section*>; // Map a section with its associated relocation section
  using rel_sections_size_t  = std::unordered_map<Section*, size_t>;   // Map relocation sections with needed size

  public:
  using Layout::Layout;
  ObjectFileLayout(const ObjectFileLayout&) = delete;
  ObjectFileLayout& operator=(const ObjectFileLayout&) = delete;

  ObjectFileLayout(ObjectFileLayout&&) = default;
  ObjectFileLayout& operator=(ObjectFileLayout&&) = default;

  /// The given section should be relocated if the "needed" size
  /// is greater than 0
  bool should_relocate(const Section& sec) const {
    const auto it = sec_reloc_info_.find(&sec);
    if (it == std::end(sec_reloc_info_)) {
      return false;
    }
    return it->second > 0;
  }

  ObjectFileLayout& relocate_section(const Section& section, size_t size) {
    sec_reloc_info_[&section] = size;
    return *this;
  }

  result<void> relocate() {
    uint64_t last_offset_sections = 0;

    for (std::unique_ptr<Section>& section : binary_->sections_) {
      if (section->type() == Section::TYPE::NOBITS) {
        continue;
      }
      last_offset_sections = std::max<uint64_t>(section->file_offset() + section->size(),
                                                last_offset_sections);
    }
    LIEF_DEBUG("Sections' last offset: 0x{:x}", last_offset_sections);
    LIEF_DEBUG("SHDR Table:            0x{:x}", binary_->header().section_headers_offset());

    Header& hdr = binary_->header();
    for (Section& sec : binary_->sections()) {
      if (!should_relocate(sec)) {
        continue;
      }

      const size_t needed_size = sec_reloc_info_[&sec];
      LIEF_DEBUG("Need to relocate: '{}' (0x{:x} bytes)", sec.name(), needed_size);

      DataHandler::Node new_node{last_offset_sections, needed_size,
                                 DataHandler::Node::SECTION};
      binary_->datahandler_->add(new_node);
      binary_->datahandler_->make_hole(last_offset_sections, needed_size);

      sec.offset(last_offset_sections);
      sec.size(needed_size);

      hdr.section_headers_offset(hdr.section_headers_offset() + needed_size);
      last_offset_sections += needed_size;
    }

    if (strtab_section_ != nullptr && !is_strtab_shared_shstrtab()) {
      strtab_section_->content(raw_strtab());
    }
    return {};
  }

  template<class ELF_T>
  size_t symtab_size() {
    using Elf_Sym = typename ELF_T::Elf_Sym;
    return binary_->symtab_symbols_.size() * sizeof(Elf_Sym);
  }

  inline relocations_map_t& relocation_map() {
    return relocation_map_;
  }

  inline sections_reloc_map_t& sections_reloc_map() {
    return sections_reloc_map_;
  }

  inline rel_sections_size_t& rel_sections_size() {
    return rel_sections_size_;
  }

  ~ObjectFileLayout() override = default;

  ObjectFileLayout() = delete;
  private:
  std::unordered_map<const Section*, size_t> sec_reloc_info_;

  relocations_map_t relocation_map_;
  sections_reloc_map_t sections_reloc_map_;
  rel_sections_size_t rel_sections_size_;

};
}
}

#endif
