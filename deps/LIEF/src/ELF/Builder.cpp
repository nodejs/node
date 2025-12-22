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
#include <set>
#include <fstream>
#include <iterator>

#include "LIEF/ELF/Builder.hpp"

#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/Note.hpp"


#include "Builder.tcc"

#include "ExeLayout.hpp"
#include "ObjectFileLayout.hpp"

namespace LIEF {
namespace ELF {

Builder::~Builder() = default;

Builder::Builder(Binary& binary, const config_t& config) :
  config_{config},
  binary_{&binary}
{
  const Header::FILE_TYPE type = binary.header().file_type();
  switch (type) {
    case Header::FILE_TYPE::CORE:
    case Header::FILE_TYPE::DYN:
    case Header::FILE_TYPE::EXEC:
      {
        layout_ = std::make_unique<ExeLayout>(binary, should_swap(), config_);
        break;
      }

    case Header::FILE_TYPE::REL:
      {
        layout_ = std::make_unique<ObjectFileLayout>(binary, should_swap(), config_);
        break;
      }

    default:
      {
        LIEF_ERR("ELF {} are not supported", to_string(type));
        std::abort();
      }
  }
  ios_.reserve(binary.original_size());
  ios_.set_endian_swap(should_swap());
}


bool Builder::should_swap() const {
  switch (binary_->get_abstract_header().endianness()) {
#ifdef __BYTE_ORDER__
#if  defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    case LIEF::Header::ENDIANNESS::BIG:
#elif defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    case LIEF::Header::ENDIANNESS::LITTLE:
#endif
      return true;
#endif // __BYTE_ORDER__
    default:
      // we're good (or don't know what to do), consider bytes are in the expected order
      return false;
  }
}


void Builder::build() {
  const Header::CLASS elf_class = binary_->type();
  if (elf_class != Header::CLASS::ELF32 && elf_class != Header::CLASS::ELF64) {
    LIEF_ERR("Invalid ELF class");
    return;
  }

  auto res = binary_->type() == Header::CLASS::ELF32 ?
             build<details::ELF32>() : build<details::ELF64>();
  if (!res) {
    LIEF_ERR("Builder failed");
  }
}

const std::vector<uint8_t>& Builder::get_build() {
  return ios_.raw();
}

void Builder::write(const std::string& filename) const {
  std::ofstream output_file{filename, std::ios::out | std::ios::binary | std::ios::trunc};
  if (!output_file) {
    LIEF_ERR("Can't open {}!", filename);
    return;
  }
  write(output_file);
}

void Builder::write(std::ostream& os) const {
  std::vector<uint8_t> content;
  ios_.move(content);
  os.write(reinterpret_cast<const char*>(content.data()), content.size());
}

uint32_t Builder::sort_dynamic_symbols() {
  const auto it_begin = std::begin(binary_->dynamic_symbols_);
  const auto it_end = std::end(binary_->dynamic_symbols_);

  const auto it_first_non_local_symbol =
      std::stable_partition(it_begin, it_end,
      [] (const std::unique_ptr<Symbol>& sym) {
        return sym->is_local();
      });

  const uint32_t first_non_local_symbol_index = std::distance(it_begin, it_first_non_local_symbol);

  if (Section* section = binary_->get_section(".dynsym")) {
    if (section->information() != first_non_local_symbol_index) {
      // TODO: Erase null entries of dynamic symbol table and symbol version
      // table if information of .dynsym section is smaller than null entries
      // num.
      LIEF_DEBUG("information of {} section changes from {:d} to {:d}",
                 section->name(), section->information(), first_non_local_symbol_index);

      section->information(first_non_local_symbol_index);
    }
  }

  const auto it_first_exported_symbol = std::stable_partition(
      it_first_non_local_symbol, it_end, [] (const std::unique_ptr<Symbol>& sym) {
        return sym->shndx() == Symbol::SECTION_INDEX::UNDEF;
      });

  const uint32_t first_exported_symbol_index = std::distance(it_begin, it_first_exported_symbol);
  return first_exported_symbol_index;
}


ok_error_t Builder::build_empty_symbol_gnuhash() {
  LIEF_DEBUG("Build empty GNU Hash");
  Section* gnu_hash_section = binary_->get(Section::TYPE::GNU_HASH);

  if (gnu_hash_section == nullptr) {
    LIEF_ERR("Can't find section with type SHT_GNU_HASH");
    return make_error_code(lief_errors::not_found);
  }

  vector_iostream content(should_swap());
  const uint32_t nb_buckets = 1;
  const uint32_t shift2     = 0;
  const uint32_t maskwords  = 1;
  const uint32_t symndx     = 1; // 0 is reserved

  // nb_buckets
  content.write<uint32_t>(nb_buckets);

  // symndx
  content.write<uint32_t>(symndx);

  // maskwords
  content.write<uint32_t>(maskwords);

  // shift2
  content.write<uint32_t>(shift2);

  // fill with 0
  content.align(gnu_hash_section->size(), 0);
  gnu_hash_section->content(content.raw());
  return ok();
}

ok_error_t Builder::update_note_section(const Note& note,
                                        std::set<const Note*>& notes)
{
  Segment* segment_note = binary_->get(Segment::TYPE::NOTE);
  if (segment_note == nullptr) {
    LIEF_ERR("Can't find the PT_NOTE segment");
    return make_error_code(lief_errors::not_found);
  }

  auto res_secname = Note::type_to_section(note.type());
  if (!res_secname) {
    LIEF_ERR("LIEF doesn't know the section name for note: '{}'",
             to_string(note.type()));
    return make_error_code(lief_errors::not_supported);
  }

  Section* note_sec = binary_->get_section(*res_secname);
  if (!note_sec) {
    LIEF_ERR("Section {} not present", *res_secname);
    return make_error_code(lief_errors::not_found);
  }

  const auto& offset_map = static_cast<ExeLayout*>(layout_.get())->note_off_map();
  auto it_offset = offset_map.find(&note);
  if (it_offset == offset_map.end()) {
    LIEF_ERR("Can't find offset for note '{}'", to_string(note.type()));
    return make_error_code(lief_errors::not_found);
  }

  if (!notes.insert(&note).second) {
    LIEF_DEBUG("Note '{}' has already been processed", to_string(note.type()));
    note_sec->virtual_address(0);
    note_sec->size(note_sec->size() + note.size());
    return ok_t();
  }

  const uint64_t note_offset = it_offset->second;
  note_sec->offset(segment_note->file_offset() + note_offset);
  note_sec->size(note.size());
  note_sec->virtual_address(segment_note->virtual_address() + note_offset);

  if (note.type() == Note::TYPE::GNU_PROPERTY_TYPE_0) {
    if (Segment* seg = binary_->get(Segment::TYPE::GNU_PROPERTY)) {
      seg->file_offset(note_sec->offset());
      seg->physical_size(note_sec->size());
      seg->virtual_address(note_sec->virtual_address());
      seg->physical_address(note_sec->virtual_address());
      seg->virtual_size(note_sec->size());
    }
  }
  return ok();
}

}
}
