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
#include <utility>
#include <algorithm>
#include <iterator>
#include <map>
#include <numeric>
#include <limits>

#include "logging.hpp"
#include "hash_stream.hpp"
#include "internal_utils.hpp"

#include "LIEF/utils.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/PE/hash.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Builder.hpp"
#include "LIEF/PE/DataDirectory.hpp"
#include "LIEF/PE/Debug.hpp"
#include "LIEF/PE/EnumToString.hpp"
#include "LIEF/PE/Export.hpp"
#include "LIEF/PE/ExportEntry.hpp"
#include "LIEF/PE/ImportEntry.hpp"
#include "LIEF/PE/LoadConfigurations/LoadConfiguration.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/ResourceData.hpp"
#include "LIEF/PE/ResourceDirectory.hpp"
#include "LIEF/PE/ResourcesManager.hpp"
#include "LIEF/PE/RichHeader.hpp"
#include "LIEF/PE/RichEntry.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/ExceptionInfo.hpp"
#include "LIEF/PE/LoadConfigurations/VolatileMetadata.hpp"
#include "LIEF/PE/exceptions_info/RuntimeFunctionAArch64.hpp"
#include "LIEF/PE/exceptions_info/RuntimeFunctionX64.hpp"
#include "LIEF/PE/TLS.hpp"
#include "LIEF/PE/utils.hpp"

#include "LIEF/COFF/Symbol.hpp"

#include "LIEF/PE/signature/SpcIndirectData.hpp"

#include "PE/Structures.hpp"
#include "PE/checksum.hpp"

#include "frozen.hpp"

#include "internal_utils.hpp"

namespace LIEF {
namespace PE {

Binary::~Binary() = default;
Binary::Binary() :
   LIEF::Binary(Binary::FORMATS::PE)
{}

inline bool has_hybrid_metadata_ptr(const Binary& pe) {
  const LoadConfiguration* LC = pe.load_configuration();
  if (LC == nullptr) {
    return false;
  }

  if (auto opt = LC->hybrid_metadata_pointer(); opt.value_or(0) > 0) {
    return true;
  }
  return false;
}

template<typename T>
inline std::unique_ptr<Builder>
  write_impl(Binary& binary, const Builder::config_t& config, T&& dest)
{
  auto builder = std::make_unique<Builder>(binary, config);
  builder->build();
  builder->write(dest);
  return builder;
}

std::unique_ptr<Builder>
  Binary::write(const std::string& filename, const Builder::config_t& config)
{
  return write_impl(*this, config, filename);
}

std::unique_ptr<Builder>
  Binary::write(std::ostream& os, const Builder::config_t& config)
{
  return write_impl(*this, config, os);
}

TLS& Binary::tls(const TLS& tls) {
  auto new_tls = std::make_unique<TLS>(tls);
  new_tls->directory_ = tls_dir();
  tls_ = std::move(new_tls);
  return *tls_;
}


void Binary::remove_tls() {
  if (tls_ == nullptr) {
    // No TLS so nothing to do
    return;
  }
  DataDirectory* tls_dir = this->tls_dir();
  const size_t ptr_size = optional_header().magic() == PE_TYPE::PE32 ?
                          sizeof(uint32_t) : sizeof(uint64_t);

  const uint64_t imagebase = optional_header().imagebase();

  uint32_t tls_hdr_start = tls_dir->RVA();
  uint32_t tls_hdr_end = tls_hdr_start + tls_dir->size();

  uint32_t tls_cbk_start = 0;
  uint32_t tls_cbk_end = 0;

  // Clear the TLS header with 0
  fill_address(tls_dir->RVA(), tls_dir->size(), 0);

  // Clear the callbacks
  if (uint64_t addr = tls_->addressof_callbacks(); addr > 0) {
    const size_t cbk_size = tls_->callbacks().size() * ptr_size;
    fill_address(addr, cbk_size, 0);
    tls_cbk_start = addr - imagebase;
    tls_cbk_end = tls_cbk_start + cbk_size;
  }

  // Clear the template data
  if (const auto& data = tls_->addressof_raw_data(); data.first > 0) {
    const size_t size = data.second - data.first;
    fill_address(data.first, size, 0);
  }

  // Remove relocations associated with the TLS structure
  for (Relocation& R : relocations()) {
    R.entries_.erase(
      std::remove_if(R.entries_.begin(), R.entries_.end(),
        [&] (const std::unique_ptr<RelocationEntry>& E) {
          const uint32_t addr = E->address();
          if (tls_cbk_start <= addr && addr < tls_cbk_end) {
            return true;
          }

          if (tls_hdr_start <= addr && addr < tls_hdr_end) {
            return true;
          }
          return false;
        }
      ), R.entries_.end());
  }
  // Reset the DataDirectory RVA/size
  tls_dir->RVA(0);
  tls_dir->size(0);

  // delete the TLS class
  tls_.reset(nullptr);
}

result<uint64_t> Binary::offset_to_virtual_address(uint64_t offset, uint64_t slide) const {
  const auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [offset] (const std::unique_ptr<Section>& section) {
        return (offset >= section->offset() &&
                offset < (section->offset() + section->sizeof_raw_data()));
      });

  if (it_section == std::end(sections_)) {
    if (slide > 0) {
      return slide + offset;
    }
    return offset;
  }
  const std::unique_ptr<Section>& section = *it_section;
  const uint64_t base_rva = section->virtual_address() - section->offset();
  if (slide > 0) {
    return slide + base_rva + offset;
  }

  return base_rva + offset;
}

uint64_t Binary::rva_to_offset(uint64_t RVA) const {
  const auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [RVA] (const std::unique_ptr<Section>& section) {
        const auto vsize_adj = std::max<uint64_t>(section->virtual_size(), section->sizeof_raw_data());
        return section->virtual_address() <= RVA &&
               RVA < (section->virtual_address() + vsize_adj);
      });

  if (it_section == std::end(sections_)) {
    // If not found within a section,
    // we assume that rva == offset
    return RVA;
  }
  const std::unique_ptr<Section>& section = *it_section;

  // rva - virtual_address + pointer_to_raw_data
  uint32_t section_alignment = optional_header().section_alignment();
  uint32_t file_alignment    = optional_header().file_alignment();
  if (section_alignment < 0x1000) {
    section_alignment = file_alignment;
  }

  uint64_t section_va     = section->virtual_address();
  uint64_t section_offset = section->pointerto_raw_data();

  section_va     = align(section_va, section_alignment);
  section_offset = align(section_offset, file_alignment);
  return ((RVA - section_va) + section_offset);
}

const Section* Binary::section_from_offset(uint64_t offset) const {
  const auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [&offset] (const std::unique_ptr<Section>& section) {
        return section->pointerto_raw_data() <= offset &&
               offset < (section->pointerto_raw_data() + section->sizeof_raw_data());
      });

  if (it_section == std::end(sections_)) {
    return nullptr;
  }

  return it_section->get();
}

const Section* Binary::section_from_rva(uint64_t virtual_address) const {
  const auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [virtual_address] (const std::unique_ptr<Section>& section) {
        return section->virtual_address() <= virtual_address &&
               virtual_address < (section->virtual_address() + section->virtual_size());
      });

  if (it_section == std::end(sections_)) {
    return nullptr;
  }

  return it_section->get();
}

const DataDirectory* Binary::data_directory(DataDirectory::TYPES index) const {
  if (static_cast<size_t>(index) < data_directories_.size() &&
      data_directories_[static_cast<size_t>(index)] != nullptr) {
    return data_directories_[static_cast<size_t>(index)].get();
  }
  return nullptr;
}

bool Binary::is_reproducible_build() const {
  const auto it = std::find_if(debug_.begin(), debug_.end(),
      [] (const std::unique_ptr<Debug>& dbg) {
        return Repro::classof(dbg.get());
      });
  return it != debug_.end();
}

Export& Binary::set_export(const Export& export_table) {
  export_ = std::make_unique<Export>(export_table);
  return *export_;
}

LIEF::Binary::symbols_t Binary::get_abstract_symbols() {
  LIEF::Binary::symbols_t lief_symbols;
  for (COFF::Symbol& s : symbols()) {
    lief_symbols.push_back(&s);
  }

  if (Export* exp = get_export()) {
    for (ExportEntry& entry : exp->entries()) {
      lief_symbols.push_back(&entry);
    }
  }

  for (std::unique_ptr<Import>& imp : imports_) {
    for (ImportEntry& entry : imp->entries()) {
      lief_symbols.push_back(&entry);
    }
  }

  for (std::unique_ptr<DelayImport>& imp : delay_imports_) {
    for (DelayImportEntry& entry : imp->entries()) {
      lief_symbols.push_back(&entry);
    }
  }
  return lief_symbols;
}



LIEF::Binary::sections_t Binary::get_abstract_sections() {
  LIEF::Binary::sections_t secs;
  secs.reserve(sections_.size());
  std::transform(std::begin(sections_), std::end(sections_),
                 std::back_inserter(secs),
                 [] (const std::unique_ptr<Section>& s) {
                  return s.get();
                 });
  return secs;
}


const Section* Binary::get_section(const std::string& name) const {
  const auto section_it = std::find_if(std::begin(sections_), std::end(sections_),
      [&name] (const std::unique_ptr<Section>& section) {
        return section->name() == name;
      });

  if (section_it == std::end(sections_)) {
    return nullptr;
  }
  return section_it->get();
}


const Section* Binary::import_section() const {
  if (!has_imports()) {
    return nullptr;
  }
  if (const DataDirectory* import_directory = import_dir()) {
    return import_directory->section();
  }
  return nullptr;
}

uint64_t Binary::virtual_size() const {
  uint64_t size = 0;
  size += dos_header().addressof_new_exeheader();
  size += sizeof(details::pe_header);
  size += (type_ == PE_TYPE::PE32) ? sizeof(details::pe32_optional_header) :
                                     sizeof(details::pe64_optional_header);
  for (const std::unique_ptr<Section>& section : sections_) {
    size = std::max(size, section->virtual_address() + section->virtual_size());
  }
  size = LIEF::align(size, optional_header().section_alignment());
  return size;
}


uint32_t Binary::sizeof_headers() const {
  uint32_t size = 0;
  size += dos_header().addressof_new_exeheader();
  size += sizeof(details::pe_header);
  size += (type_ == PE_TYPE::PE32) ? sizeof(details::pe32_optional_header) :
                                     sizeof(details::pe64_optional_header);

  size += sizeof(details::pe_data_directory) * data_directories_.size();
  size += sizeof(details::pe_section) * sections_.size();
  size = static_cast<uint32_t>(LIEF::align(size, optional_header().file_alignment()));

  return size;
}

void Binary::remove_section(const std::string& name, bool clear) {
  Section* sec = get_section(name);

  if (sec == nullptr) {
    LIEF_ERR("Unable to find the section: '{}'", name);
    return;
  }

  return remove(*sec, clear);
}

void Binary::remove(const Section& section, bool clear) {
  const auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [&section] (const std::unique_ptr<Section>& s) {
        return *s == section;
      });

  if (it_section == std::end(sections_)) {
    LIEF_ERR("Unable to find section: '{}'", section.name());
    return;
  }

  std::unique_ptr<Section>& to_remove = *it_section;
  const size_t section_index = std::distance(std::begin(sections_), it_section);

  if (section_index < (sections_.size() - 1) && section_index > 0) {
    std::unique_ptr<Section>& previous = sections_[section_index - 1];
    const size_t raw_size_gap = (to_remove->offset() + to_remove->size()) - (previous->offset() + previous->size());
    previous->size(previous->size() + raw_size_gap);

    const size_t vsize_size_gap = (to_remove->virtual_address() + to_remove->virtual_size()) -
                                  (previous->virtual_address() + previous->virtual_size());
    previous->virtual_size(previous->virtual_size() + vsize_size_gap);
  }


  if (clear) {
    to_remove->clear(0);
  }

  sections_.erase(it_section);

  header().numberof_sections(header().numberof_sections() - 1);

  optional_header().sizeof_headers(sizeof_headers());
  optional_header().sizeof_image(static_cast<uint32_t>(virtual_size()));
}

result<uint64_t> Binary::make_space_for_new_section() {
  const uint32_t shift_value = align(sizeof(details::pe_section), optional_header().file_alignment());

  const uint64_t section_table_offset =
    dos_header().addressof_new_exeheader() +
    sizeof(details::pe_header) +
    header().sizeof_optional_header() +
    sizeof(details::pe_data_directory) * data_directories_.size() +
    sizeof(details::pe_section) * sections_.size();

  shift(section_table_offset, shift_value);

  available_sections_space_++;
  return available_sections_space_;
}

void Binary::shift(uint64_t /*from*/, uint64_t by) {
  for (std::unique_ptr<Section>& section : sections_) {
    section->pointerto_raw_data(section->pointerto_raw_data() + by);
  }
}

uint64_t Binary::last_section_offset() const {
  uint64_t offset = std::accumulate(
      std::begin(sections_), std::end(sections_), static_cast<uint64_t>(sizeof_headers()),
      [] (uint64_t offset, const std::unique_ptr<Section>& s) {
        return std::max<uint64_t>(s->pointerto_raw_data() + s->sizeof_raw_data(), offset);
      });
  return offset;
}

Section* Binary::add_section(const Section& section) {
  if (available_sections_space_ < 0) {
    make_space_for_new_section();
    return add_section(section);
  }

  auto new_section = std::make_unique<Section>(section);
  std::vector<uint8_t> content    = as_vector(new_section->content());
  const auto section_size         = static_cast<uint32_t>(content.size());
  const auto section_size_aligned = static_cast<uint32_t>(align(section_size, optional_header().file_alignment()));
  const uint32_t virtual_size     = section_size;

  content.insert(content.end(), section_size_aligned - section_size, 0);
  new_section->content(content);

  // Compute new section offset
  uint64_t new_section_offset = align(
      last_section_offset(), optional_header().file_alignment());

  LIEF_DEBUG("New section offset: 0x{:010x}", new_section_offset);

  // Compute new section Virtual address
  const auto section_align = static_cast<uint64_t>(optional_header().section_alignment());
  const uint64_t new_section_va = align(std::accumulate(
      std::begin(sections_), std::end(sections_), section_align,
      [] (uint64_t va, const std::unique_ptr<Section>& s) {
        return std::max<uint64_t>(s->virtual_address() + s->virtual_size(), va);
      }), section_align);

  LIEF_DEBUG("New section VA: 0x{:010x}", new_section_va);

  if (new_section->pointerto_raw_data() == 0) {
    new_section->pointerto_raw_data(new_section_offset);
  }

  if (new_section->sizeof_raw_data() == 0) {
    new_section->sizeof_raw_data(section_size_aligned);
  }

  if (new_section->virtual_address() == 0) {
    new_section->virtual_address(new_section_va);
  }

  if (new_section->virtual_size() == 0) {
    new_section->virtual_size(virtual_size);
  }

  if (sections_.size() >= std::numeric_limits<uint16_t>::max()) {
    LIEF_INFO("Binary reachs its maximum number of sections");
    return nullptr;
  }

  available_sections_space_--;

  // Update headers
  header().numberof_sections(static_cast<uint16_t>(sections_.size()));

  optional_header().sizeof_image(this->virtual_size());
  optional_header().sizeof_headers(sizeof_headers());

  sections_.push_back(std::move(new_section));
  return sections_.back().get();
}


Relocation& Binary::add_relocation(const Relocation& relocation) {
  auto newone = std::make_unique<Relocation>(relocation);
  for (RelocationEntry& entry : newone->entries()) {
    entry.parent(*newone);
  }
  relocations_.push_back(std::move(newone));
  return *relocations_.back();
}

void Binary::remove_all_relocations() {
  relocations_.clear();
}

LIEF::Binary::relocations_t Binary::get_abstract_relocations() {
  LIEF::Binary::relocations_t abstract_relocs;
  for (Relocation& relocation : relocations()) {
    for (RelocationEntry& entry : relocation.entries()) {
      abstract_relocs.push_back(&entry);
    }
  }
  return abstract_relocs;
}

bool Binary::remove_import(const std::string& name) {
  auto it = std::find_if(imports_.begin(), imports_.end(),
    [&name] (const std::unique_ptr<Import>& imp) { return imp->name() == name; }
  );

  if (it == imports_.end()) {
    return false;
  }

  imports_.erase(it);
  return true;
}

const Import* Binary::get_import(const std::string& import_name) const {
  const auto it_import = std::find_if(std::begin(imports_), std::end(imports_),
      [&import_name] (const std::unique_ptr<Import>& import) {
        return import->name() == import_name;
      });

  if (it_import == std::end(imports_)) {
    return nullptr;
  }

  return &**it_import;
}

ResourceNode* Binary::set_resources(const ResourceNode& resource) {
  return set_resources(resource.clone());
}

ResourceNode* Binary::set_resources(std::unique_ptr<ResourceNode> root) {
  resources_ = std::move(root);
  return resources_.get();
}

uint32_t Binary::compute_checksum() const {
  const size_t sizeof_ptr = type_ == PE_TYPE::PE32 ? sizeof(uint32_t) :
                                                     sizeof(uint64_t);

  ChecksumStream cs(optional_header_.checksum());
  cs // Hash dos header
    .write(dos_header_.magic())
    .write(dos_header_.used_bytes_in_last_page())
    .write(dos_header_.file_size_in_pages())
    .write(dos_header_.numberof_relocation())
    .write(dos_header_.header_size_in_paragraphs())
    .write(dos_header_.minimum_extra_paragraphs())
    .write(dos_header_.maximum_extra_paragraphs())
    .write(dos_header_.initial_relative_ss())
    .write(dos_header_.initial_sp())
    .write(dos_header_.checksum())
    .write(dos_header_.initial_ip())
    .write(dos_header_.initial_relative_cs())
    .write(dos_header_.addressof_relocation_table())
    .write(dos_header_.overlay_number())
    .write(dos_header_.reserved())
    .write(dos_header_.oem_id())
    .write(dos_header_.oem_info())
    .write(dos_header_.reserved2())
    .write(dos_header_.addressof_new_exeheader())
    .write(dos_stub_);

  cs // Hash PE Header
    .write(header_.signature())
    .write(static_cast<uint16_t>(header_.machine()))
    .write(header_.numberof_sections())
    .write(header_.time_date_stamp())
    .write(header_.pointerto_symbol_table())
    .write(header_.numberof_symbols())
    .write(header_.sizeof_optional_header())
    .write(static_cast<uint16_t>(header_.characteristics()));

  cs // Hash OptionalHeader
    .write(static_cast<uint16_t>(optional_header_.magic()))
    .write(optional_header_.major_linker_version())
    .write(optional_header_.minor_linker_version())
    .write(optional_header_.sizeof_code())
    .write(optional_header_.sizeof_initialized_data())
    .write(optional_header_.sizeof_uninitialized_data())
    .write(optional_header_.addressof_entrypoint())
    .write(optional_header_.baseof_code());

  if (type_ == PE_TYPE::PE32) {
    cs.write(optional_header_.baseof_data());
  }
  cs // Continuation of optional header
    .write_sized_int(optional_header_.imagebase(), sizeof_ptr)
    .write(optional_header_.section_alignment())
    .write(optional_header_.file_alignment())
    .write(optional_header_.major_operating_system_version())
    .write(optional_header_.minor_operating_system_version())
    .write(optional_header_.major_image_version())
    .write(optional_header_.minor_image_version())
    .write(optional_header_.major_subsystem_version())
    .write(optional_header_.minor_subsystem_version())
    .write(optional_header_.win32_version_value())
    .write(optional_header_.sizeof_image())
    .write(optional_header_.sizeof_headers())
    .write(optional_header_.checksum())
    .write(static_cast<uint16_t>(optional_header_.subsystem()))
    .write(static_cast<uint16_t>(optional_header_.dll_characteristics()))
    .write_sized_int(optional_header_.sizeof_stack_reserve(), sizeof_ptr)
    .write_sized_int(optional_header_.sizeof_stack_commit(), sizeof_ptr)
    .write_sized_int(optional_header_.sizeof_heap_reserve(), sizeof_ptr)
    .write_sized_int(optional_header_.sizeof_heap_commit(), sizeof_ptr)
    .write(optional_header_.loader_flags())
    .write(optional_header_.numberof_rva_and_size());

  for (const std::unique_ptr<DataDirectory>& dir : data_directories_) {
    cs
      .write(dir->RVA())
      .write(dir->size());
  }
  // Section headers
  for (const std::unique_ptr<Section>& sec : sections_) {
    std::array<char, 8> name = {0};
    const std::string& sec_name = sec->fullname();
    uint32_t name_length = std::min<uint32_t>(sec_name.size() + 1, sizeof(name));
    std::copy(sec_name.c_str(), sec_name.c_str() + name_length, std::begin(name));
    cs
      .write(name)
      .write(sec->virtual_size())
      .write<uint32_t>(sec->virtual_address())
      .write(sec->sizeof_raw_data())
      .write(sec->pointerto_raw_data())
      .write(sec->pointerto_relocation())
      .write(sec->pointerto_line_numbers())
      .write(sec->numberof_relocations())
      .write(sec->numberof_line_numbers())
      .write(static_cast<uint32_t>(sec->characteristics()));
  }

  cs.write(section_offset_padding_);

  std::vector<Section*> sections;
  sections.reserve(sections_.size());
  std::transform(
    std::begin(sections_), std::end(sections_), std::back_inserter(sections),
    [] (const std::unique_ptr<Section>& s) { return s.get(); });

  // Sort by file offset
  std::sort(std::begin(sections), std::end(sections),
    [] (const Section* lhs, const Section* rhs) {
      return  lhs->pointerto_raw_data() < rhs->pointerto_raw_data();
    }
  );

  uint64_t position = 0;
  for (const Section* sec : sections) {
    if (sec->sizeof_raw_data() == 0) {
      continue;
    }
    span<const uint8_t> pad = sec->padding();
    span<const uint8_t> content = sec->content();
    if (/* overlapping */ sec->offset() < position) {
      // Trunc the beginning of the overlap
      if (position <= sec->offset() + content.size()) {
        const uint64_t start_p = position - sec->offset();
        const uint64_t size = content.size() - start_p;
        cs
          .write(content.data() + start_p, size)
          .write(pad);
      } else {
        LIEF_WARN("Overlapping in the padding area");
      }
    } else {
      cs
        .write(content.data(), content.size())
        .write(pad);
    }
    position = sec->offset() + content.size() + pad.size();
  }
  if (!overlay_.empty()) {
    cs.write(overlay());
  }
  return cs.finalize();
}

std::vector<uint8_t> Binary::authentihash(ALGORITHMS algo) const {
  CONST_MAP_ALT HMAP = {
    std::pair(ALGORITHMS::MD5,     hashstream::HASH::MD5),
    std::pair(ALGORITHMS::SHA_1,   hashstream::HASH::SHA1),
    std::pair(ALGORITHMS::SHA_256, hashstream::HASH::SHA256),
    std::pair(ALGORITHMS::SHA_384, hashstream::HASH::SHA384),
    std::pair(ALGORITHMS::SHA_512, hashstream::HASH::SHA512),
  };
  auto it_hash = HMAP.find(algo);
  if (it_hash == std::end(HMAP)) {
    LIEF_WARN("Unsupported hash algorithm: {}", to_string(algo));
    return {};
  }
  const size_t sizeof_ptr = type_ == PE_TYPE::PE32 ? sizeof(uint32_t) : sizeof(uint64_t);
  const hashstream::HASH hash_type = it_hash->second;
  hashstream ios(hash_type);
  //vector_iostream ios;
  ios // Hash dos header
    .write(dos_header_.magic())
    .write(dos_header_.used_bytes_in_last_page())
    .write(dos_header_.file_size_in_pages())
    .write(dos_header_.numberof_relocation())
    .write(dos_header_.header_size_in_paragraphs())
    .write(dos_header_.minimum_extra_paragraphs())
    .write(dos_header_.maximum_extra_paragraphs())
    .write(dos_header_.initial_relative_ss())
    .write(dos_header_.initial_sp())
    .write(dos_header_.checksum())
    .write(dos_header_.initial_ip())
    .write(dos_header_.initial_relative_cs())
    .write(dos_header_.addressof_relocation_table())
    .write(dos_header_.overlay_number())
    .write(dos_header_.reserved())
    .write(dos_header_.oem_id())
    .write(dos_header_.oem_info())
    .write(dos_header_.reserved2())
    .write(dos_header_.addressof_new_exeheader())
    .write(dos_stub_);

  ios // Hash PE Header
    .write(header_.signature())
    .write(static_cast<uint16_t>(header_.machine()))
    .write(header_.numberof_sections())
    .write(header_.time_date_stamp())
    .write(header_.pointerto_symbol_table())
    .write(header_.numberof_symbols())
    .write(header_.sizeof_optional_header())
    .write(static_cast<uint16_t>(header_.characteristics()));

  ios // Hash OptionalHeader
    .write(static_cast<uint16_t>(optional_header_.magic()))
    .write(optional_header_.major_linker_version())
    .write(optional_header_.minor_linker_version())
    .write(optional_header_.sizeof_code())
    .write(optional_header_.sizeof_initialized_data())
    .write(optional_header_.sizeof_uninitialized_data())
    .write(optional_header_.addressof_entrypoint())
    .write(optional_header_.baseof_code());

  if (type_ == PE_TYPE::PE32) {
    ios.write(optional_header_.baseof_data());
  }
  ios // Continuation of optional header
    .write_sized_int(optional_header_.imagebase(), sizeof_ptr)
    .write(optional_header_.section_alignment())
    .write(optional_header_.file_alignment())
    .write(optional_header_.major_operating_system_version())
    .write(optional_header_.minor_operating_system_version())
    .write(optional_header_.major_image_version())
    .write(optional_header_.minor_image_version())
    .write(optional_header_.major_subsystem_version())
    .write(optional_header_.minor_subsystem_version())
    .write(optional_header_.win32_version_value())
    .write(optional_header_.sizeof_image())
    .write(optional_header_.sizeof_headers())
    // optional_header_.checksum()) is not a part of the hash
    .write(static_cast<uint16_t>(optional_header_.subsystem()))
    .write(static_cast<uint16_t>(optional_header_.dll_characteristics()))
    .write_sized_int(optional_header_.sizeof_stack_reserve(), sizeof_ptr)
    .write_sized_int(optional_header_.sizeof_stack_commit(), sizeof_ptr)
    .write_sized_int(optional_header_.sizeof_heap_reserve(), sizeof_ptr)
    .write_sized_int(optional_header_.sizeof_heap_commit(), sizeof_ptr)
    .write(optional_header_.loader_flags())
    .write(optional_header_.numberof_rva_and_size());

  for (const std::unique_ptr<DataDirectory>& dir : data_directories_) {
    if (dir->type() == DataDirectory::TYPES::CERTIFICATE_TABLE) {
      continue;
    }
    ios
      .write(dir->RVA())
      .write(dir->size());
  }

  for (const std::unique_ptr<Section>& sec : sections_) {
    std::array<char, 8> name = {0};
    const std::string& sec_name = sec->fullname();
    uint32_t name_length = std::min<uint32_t>(sec_name.size() + 1, sizeof(name));
    std::copy(sec_name.c_str(), sec_name.c_str() + name_length, std::begin(name));
    ios
      .write(name)
      .write(sec->virtual_size())
      .write<uint32_t>(sec->virtual_address())
      .write(sec->sizeof_raw_data())
      .write(sec->pointerto_raw_data())
      .write(sec->pointerto_relocation())
      .write(sec->pointerto_line_numbers())
      .write(sec->numberof_relocations())
      .write(sec->numberof_line_numbers())
      .write(static_cast<uint32_t>(sec->characteristics()));
  }
  //LIEF_DEBUG("Section padding at 0x{:x}", ios.tellp());
  ios.write(section_offset_padding_);

  std::vector<Section*> sections;
  sections.reserve(sections_.size());
  std::transform(std::begin(sections_), std::end(sections_),
                 std::back_inserter(sections),
                 [] (const std::unique_ptr<Section>& s) {
                   return s.get();
                 });

  // Sort by file offset
  std::sort(std::begin(sections), std::end(sections),
            [] (const Section* lhs, const Section* rhs) {
              return  lhs->pointerto_raw_data() < rhs->pointerto_raw_data();
            });

  uint64_t position = 0;
  for (const Section* sec : sections) {
    if (sec->sizeof_raw_data() == 0) {
      continue;
    }
    span<const uint8_t> pad = sec->padding();
    span<const uint8_t> content = sec->content();
    LIEF_DEBUG("Authentihash:  Append section {:<8}: [0x{:04x}, 0x{:04x}] + "
               "[0x{:04x}] = [0x{:04x}, 0x{:04x}]",
               sec->name(),
               sec->offset(), sec->offset() + content.size(), pad.size(),
               sec->offset(), sec->offset() + content.size() + pad.size());
    if (/* overlapping */ sec->offset() < position) {
      // Trunc the beginning of the overlap
      if (position <= sec->offset() + content.size()) {
        const uint64_t start_p = position - sec->offset();
        const uint64_t size = content.size() - start_p;
        ios
          .write(content.data() + start_p, size)
          .write(pad);
      } else {
        LIEF_WARN("Overlapping in the padding area");
      }
    } else {
      ios
        .write(content.data(), content.size())
        .write(pad);
    }
    position = sec->offset() + content.size() + pad.size();
  }
  if (!overlay_.empty()) {
    const DataDirectory* cert_dir = this->cert_dir();
    if (cert_dir == nullptr) {
      LIEF_ERR("Can't find the data directory for CERTIFICATE_TABLE");
      return {};
    }
    LIEF_DEBUG("Add overlay and omit 0x{:08x} - 0x{:08x}",
               cert_dir->RVA(), cert_dir->RVA() + cert_dir->size());
    if (cert_dir->RVA() > 0 && cert_dir->size() > 0 && cert_dir->RVA() >= overlay_offset_) {
      const uint64_t start_cert_offset = cert_dir->RVA() - overlay_offset_;
      const uint64_t end_cert_offset   = start_cert_offset + cert_dir->size();
      if (end_cert_offset <= overlay_.size()) {
        LIEF_DEBUG("Add [0x{:x}, 0x{:x}]", overlay_offset_, overlay_offset_ + start_cert_offset);
        LIEF_DEBUG("Add [0x{:x}, 0x{:x}]",
                   overlay_offset_ + end_cert_offset,
                   overlay_offset_ + end_cert_offset + overlay_.size() - end_cert_offset);
        ios
          .write(overlay_.data(), start_cert_offset)
          .write(overlay_.data() + end_cert_offset, overlay_.size() - end_cert_offset);
      } else {
        ios.write(overlay());
      }
    } else {
      ios.write(overlay());
    }
  }
  // When something gets wrong with the hash:
  // std::vector<uint8_t> out = ios.raw();
  // std::ofstream output_file{"/tmp/hash.blob", std::ios::out | std::ios::binary | std::ios::trunc};
  // if (output_file) {
  //   std::copy(
  //       std::begin(out),
  //       std::end(out),
  //       std::ostreambuf_iterator<char>(output_file));
  // }
  // std::vector<uint8_t> hash = hashstream(hash_type).write(out).raw();

  std::vector<uint8_t> hash = ios.raw();
  LIEF_DEBUG("{}", hex_dump(hash));
  return hash;
}

Signature::VERIFICATION_FLAGS Binary::verify_signature(Signature::VERIFICATION_CHECKS checks) const {
  if (!has_signatures()) {
    return Signature::VERIFICATION_FLAGS::NO_SIGNATURE;
  }

  Signature::VERIFICATION_FLAGS flags = Signature::VERIFICATION_FLAGS::OK;

  for (size_t i = 0; i < signatures_.size(); ++i) {
    const Signature& sig = signatures_[i];
    flags |= verify_signature(sig, checks);
    if (flags != Signature::VERIFICATION_FLAGS::OK) {
      LIEF_INFO("Verification failed for signature #{:d} (0b{:b})", i, static_cast<uintptr_t>(flags));
      break;
    }
  }
  return flags;
}

Signature::VERIFICATION_FLAGS Binary::verify_signature(const Signature& sig, Signature::VERIFICATION_CHECKS checks) const {
  Signature::VERIFICATION_FLAGS flags = Signature::VERIFICATION_FLAGS::OK;
  if (!is_true(checks & Signature::VERIFICATION_CHECKS::HASH_ONLY)) {
    const Signature::VERIFICATION_FLAGS value = sig.check(checks);
    if (value != Signature::VERIFICATION_FLAGS::OK) {
      LIEF_INFO("Bad signature (0b{:b})", static_cast<uintptr_t>(value));
      flags |= value;
    }
  }

  const ContentInfo::Content& content = sig.content_info().value();

  if (!SpcIndirectData::classof(&content)) {
    LIEF_INFO("Expecting SpcIndirectData");
    flags |= Signature::VERIFICATION_FLAGS::CORRUPTED_CONTENT_INFO;
    return flags;
  }
  const auto& spc_indirect_data = static_cast<const SpcIndirectData&>(content);

  // Check that the authentihash matches Content Info's digest
  const std::vector<uint8_t>& authhash = authentihash(sig.digest_algorithm());
  const span<const uint8_t> chash = spc_indirect_data.digest();
  if (authhash != std::vector<uint8_t>(chash.begin(), chash.end())) {
    LIEF_INFO("Authentihash and Content info's digest does not match:\n  {}\n  {}",
              hex_dump(authhash), hex_dump(chash));
    flags |= Signature::VERIFICATION_FLAGS::BAD_DIGEST;
  }
  if (flags != Signature::VERIFICATION_FLAGS::OK) {
    flags |= Signature::VERIFICATION_FLAGS::BAD_SIGNATURE;
  }

  return flags;
}

LIEF::Binary::functions_t Binary::get_abstract_exported_functions() const {
  LIEF::Binary::functions_t result;
  if (const Export* exp = get_export()) {
    for (const ExportEntry& entry : exp->entries()) {
      const std::string& name = entry.name();
      if(!name.empty()) {
        result.emplace_back(name, entry.address(), Function::FLAGS::EXPORTED);
      }
    }
  }
  return result;
}

LIEF::Binary::functions_t Binary::get_abstract_imported_functions() const {
  LIEF::Binary::functions_t result;
  for (const Import& import : imports()) {
    Import resolved = import;
    if (auto resolution = resolve_ordinals(import)) {
      resolved = std::move(*resolution);
    }
    for (const ImportEntry& entry : resolved.entries()) {
      const std::string& name = entry.name();
      if(!name.empty()) {
        result.emplace_back(name, entry.iat_address(), Function::FLAGS::IMPORTED);
      }
    }
  }


  for (const DelayImport& import : delay_imports()) {
    for (const DelayImportEntry& entry : import.entries()) {
      if (entry.is_ordinal()) {
        continue;
      }
      const std::string& name = entry.name();
      if(!name.empty()) {
        result.emplace_back(name, entry.value(), Function::FLAGS::IMPORTED);
      }
    }
  }
  return result;
}


std::vector<std::string> Binary::get_abstract_imported_libraries() const {
  std::vector<std::string> result;
  for (const Import& import : imports()) {
    result.push_back(import.name());
  }
  for (const DelayImport& import : delay_imports()) {
    result.push_back(import.name());
  }
  return result;
}

void Binary::fill_address(uint64_t address, size_t size, uint8_t value,
                          VA_TYPES addr_type)
{
  uint64_t rva = address;

  if (addr_type == VA_TYPES::VA || addr_type == VA_TYPES::AUTO) {
    const int64_t delta = address - optional_header().imagebase();

    if (delta > 0 || addr_type == LIEF::Binary::VA_TYPES::VA) {
      rva -= optional_header().imagebase();
    }
  }

  Section* section_topatch = section_from_rva(rva);
  if (section_topatch == nullptr) {
    LIEF_ERR("Can't find section with the rva: 0x{:x}", rva);
    return;
  }
  const uint64_t offset = rva - section_topatch->virtual_address();
  span<uint8_t> content = section_topatch->writable_content();
  if (offset + size > content.size()) {
    LIEF_ERR("Can't write {} bytes at 0x{:x} (limit: 0x{:x})",
             size, offset, content.size());
    return;
  }
  std::fill_n(content.begin() + offset, size, value);
}


void Binary::patch_address(uint64_t address, const std::vector<uint8_t>& patch_value,
                           VA_TYPES addr_type) {
  uint64_t rva = address;

  if (addr_type == VA_TYPES::VA || addr_type == VA_TYPES::AUTO) {
    const int64_t delta = address - optional_header().imagebase();

    if (delta > 0 || addr_type == LIEF::Binary::VA_TYPES::VA) {
      rva -= optional_header().imagebase();
    }
  }

  // Find the section associated with the virtual address
  Section* section_topatch = section_from_rva(rva);
  if (section_topatch == nullptr) {
    LIEF_ERR("Can't find section with the rva: 0x{:x}", rva);
    return;
  }
  const uint64_t offset = rva - section_topatch->virtual_address();

  span<uint8_t> content = section_topatch->writable_content();
  if (offset + patch_value.size() > content.size()) {
    LIEF_ERR("The patch value ({} bytes @0x{:x}) is out of bounds of the section (limit: 0x{:x})",
             patch_value.size(), offset, content.size());
    return;
  }
  std::copy(std::begin(patch_value), std::end(patch_value),
            content.data() + offset);

}

void Binary::patch_address(uint64_t address, uint64_t patch_value,
                           size_t size, LIEF::Binary::VA_TYPES addr_type) {
  if (size > sizeof(patch_value)) {
    LIEF_ERR("Invalid size (0x{:x})", size);
    return;
  }

  uint64_t rva = address;

  if (addr_type == LIEF::Binary::VA_TYPES::VA || addr_type == LIEF::Binary::VA_TYPES::AUTO) {
    const int64_t delta = address - optional_header().imagebase();

    if (delta > 0 || addr_type == LIEF::Binary::VA_TYPES::VA) {
      rva -= optional_header().imagebase();
    }
  }

  Section* section_topatch = section_from_rva(rva);
  if (section_topatch == nullptr) {
    LIEF_ERR("Can't find section with the rva: 0x{:x}", rva);
    return;
  }
  const uint64_t offset = rva - section_topatch->virtual_address();
  span<uint8_t> content = section_topatch->writable_content();
  if (offset > content.size() || (offset + size) > content.size()) {
    LIEF_ERR("The patch value ({} bytes @0x{:x}) is out of bounds of the section (limit: 0x{:x})",
             size, offset, content.size());
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

span<const uint8_t> Binary::get_content_from_virtual_address(uint64_t virtual_address,
    uint64_t size, LIEF::Binary::VA_TYPES addr_type) const {

  uint64_t rva = virtual_address;

  if (addr_type == LIEF::Binary::VA_TYPES::VA || addr_type == LIEF::Binary::VA_TYPES::AUTO) {
    const int64_t delta = virtual_address - optional_header().imagebase();

    if (delta > 0 || addr_type == LIEF::Binary::VA_TYPES::VA) {
      rva -= optional_header().imagebase();
    }
  }
  const Section* section = section_from_rva(rva);
  if (section == nullptr) {
    LIEF_ERR("Can't find the section with the rva 0x{:x}", rva);
    return {};
  }
  span<const uint8_t> content = section->content();
  const uint64_t offset = rva - section->virtual_address();
  uint64_t checked_size = size;
  if ((offset + checked_size) > content.size()) {
    uint64_t delta_off = offset + checked_size - content.size();
    if (checked_size < delta_off) {
        LIEF_ERR("Can't access section data due to a section end overflow.");
        return {};
    }
    checked_size = checked_size - delta_off;
  }

  return {content.data() + offset, static_cast<size_t>(checked_size)};

}

void Binary::rich_header(const RichHeader& rich_header) {
  rich_header_ = std::make_unique<RichHeader>(rich_header);
}

// Resource manager
// ===============

result<ResourcesManager> Binary::resources_manager() const {
  if (resources_ == nullptr) {
    return make_error_code(lief_errors::not_found);
  }
  return *resources_;
}

LIEF::Binary::functions_t Binary::ctor_functions() const {
  LIEF::Binary::functions_t functions;

  if (const TLS* tls_obj = tls()) {
    const std::vector<uint64_t>& clbs = tls_obj->callbacks();
    for (size_t i = 0; i < clbs.size(); ++i) {
      functions.emplace_back("tls_" + std::to_string(i), clbs[i],
                             Function::FLAGS::CONSTRUCTOR);

    }
  }
  return functions;
}


LIEF::Binary::functions_t Binary::functions() const {

  static const auto func_cmd = [] (const Function& lhs, const Function& rhs) {
    return lhs.address() < rhs.address();
  };
  std::set<Function, decltype(func_cmd)> functions_set(func_cmd);

  LIEF::Binary::functions_t exception_functions = this->exception_functions();
  LIEF::Binary::functions_t exported            = get_abstract_exported_functions();
  LIEF::Binary::functions_t ctors               = ctor_functions();

  std::move(std::begin(exception_functions), std::end(exception_functions),
            std::inserter(functions_set, std::end(functions_set)));

  std::move(std::begin(exported), std::end(exported),
            std::inserter(functions_set, std::end(functions_set)));

  std::move(std::begin(ctors), std::end(ctors),
            std::inserter(functions_set, std::end(functions_set)));

  return {std::begin(functions_set), std::end(functions_set)};
}

LIEF::Binary::functions_t Binary::exception_functions() const {
  functions_t functions;
  functions.reserve(exceptions_.size());

  for (const std::unique_ptr<ExceptionInfo>& info : exceptions_) {
    if (const auto* x64 = info->as<RuntimeFunctionX64>()) {
      Function F(x64->rva_start());
      F.size(x64->size());

      functions.push_back(std::move(F));
    }
  }

  return functions;
}

const DelayImport* Binary::get_delay_import(const std::string& import_name) const {
  const auto it_import = std::find_if(std::begin(delay_imports_), std::end(delay_imports_),
      [&import_name] (const std::unique_ptr<DelayImport>& import) {
        return import->name() == import_name;
      });

  if (it_import == std::end(delay_imports_)) {
    return nullptr;
  }

  return &**it_import;
}

const CodeViewPDB* Binary::codeview_pdb() const {
  if (debug_.empty()) {
    return nullptr;
  }

  const auto it = std::find_if(debug_.begin(), debug_.end(),
    [] (const std::unique_ptr<Debug>& debug) {
      return CodeViewPDB::classof(debug.get());
    }
  );
  if (it == debug_.end()) {
    return nullptr;
  }

  return static_cast<const CodeViewPDB*>(it->get());
}


Debug* Binary::add_debug_info(const Debug& entry) {
  debug_.push_back(entry.clone());
  return debug_.back().get();
}

bool Binary::remove_debug(const Debug& entry) {
  auto it = std::find_if(debug_.begin(), debug_.end(),
      [&entry] (const std::unique_ptr<Debug>& dbg) {
        return dbg.get() == &entry;
      });

  if (it == debug_.end()) {
    return false;
  }

  Debug& target = **it;
  if (!target.payload().empty()) {
    span<uint8_t> payload = target.payload();
    std::memset(payload.data(), 0, payload.size());
  }

  debug_.erase(it);
  return true;
}

bool Binary::clear_debug() {
  DataDirectory* dbg_dir = this->debug_dir();
  for (std::unique_ptr<Debug>& dbg : debug_) {
    span<uint8_t> payload = dbg->payload();
    if (payload.empty()) {
      continue;
    }
    std::memset(payload.data(), 0, payload.size());
  }

  debug_.clear();
  this->fill_address(dbg_dir->RVA(), dbg_dir->size());
  dbg_dir->RVA(0);
  dbg_dir->size(0);
  return true;
}


ExceptionInfo* Binary::find_exception_at(uint32_t rva) {
  auto it = std::find_if(exceptions_.begin(), exceptions_.end(),
    [rva] (const std::unique_ptr<ExceptionInfo>& info) {
      return info->rva_start() == rva;
    }
  );

  if (it == exceptions_.end()) {
    return nullptr;
  }

  return it->get();
}

result<uint64_t> Binary::get_function_address(const std::string& name) const {
  if (const Export* exp = get_export()) {
    if (const ExportEntry* entry = exp->find_entry(name)) {
      uint32_t rva = entry->address();
      if (rva > 0) {
        return rva;
      }
    }
  }

  const std::string alt_name = '_' + name;

  for (const COFF::Symbol& sym : symbols()) {
    if (sym.complex_type() != COFF::Symbol::COMPLEX_TYPE::TY_FUNCTION) {
      continue;
    }
    if (sym.value() == 0) {
      continue;
    }

    if (sym.name() == name || sym.name() == alt_name) {
      return sym.value();
    }
  }

  return LIEF::Binary::get_function_address(name);
}

bool Binary::is_arm64ec() const {
  return has_hybrid_metadata_ptr(*this) &&
         header().machine() == Header::MACHINE_TYPES::AMD64;
}

bool Binary::is_arm64x() const {
  return has_hybrid_metadata_ptr(*this) &&
         header().machine() == Header::MACHINE_TYPES::ARM64;
}

void Binary::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& Binary::print(std::ostream& os) const {
  using namespace fmt;
  os << "DOS Header {\n"
     << indent(LIEF::to_string(dos_header()), 2)
     << "}\n";

  if (auto stub = dos_stub(); !stub.empty()) {
    os << "DOS Sub {\n"
       << indent(LIEF::dump(stub), 2)
       << "}\n";
  }

  if (const RichHeader* rich = rich_header()) {
    os << "Rich Header {\n"
       << indent(LIEF::to_string(*rich), 2)
       << "}\n";
  }

  os << "Header {\n"
     << indent(LIEF::to_string(header()), 2)
     << "}\n";

  os << "Optional Header {\n"
     << indent(LIEF::to_string(optional_header()), 2)
     << "}\n";

  os << "Data Directories:\n";
  for (const DataDirectory& dir : data_directories()) {
    os << "  " << dir << '\n';
  }

  {
    const auto secs = sections();
    for (size_t i = 0; i < secs.size(); ++i) {
      os << fmt::format("Section #{:02} {{\n", i)
         << indent(LIEF::to_string(secs[i]), 2)
         << "}\n";
    }
  }

  if (const TLS* tls_obj = tls()) {
    os << "TLS: {\n"
       << indent(LIEF::to_string(*tls_obj), 2)
       << "}\n";
  }

  if (const LoadConfiguration* lconf = load_configuration()) {
    os << "Load Configuration: {\n"
       << indent(LIEF::to_string(*lconf), 2)
       << "}\n";
  }

  if (auto entries = debug(); !entries.empty()) {
    os << fmt::format("Debug Entries (#{}):\n", entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      os << fmt::format("  Entry[{:02}] {{\n", i)
         << indent(LIEF::to_string(entries[i]), 4)
         << "  }\n";
    }
    os << "}\n";
  }

  if (auto imps = imports(); !imps.empty()) {
    os << fmt::format("Imports (#{}):\n", imps.size());
    for (const Import& imp : imps) {
      os << indent(LIEF::to_string(imp), 2);
    }
  }

  if (auto imps = delay_imports(); !imps.empty()) {
    os << fmt::format("Delay Load Imports (#{}):\n", imps.size());
    for (const DelayImport& imp : imps) {
      os << indent(LIEF::to_string(imp), 2);
    }
  }

  if (auto relocs = relocations(); !relocs.empty()) {
    os << fmt::format("Base Relocations (#{}):\n", relocs.size());
    for (const Relocation& R : relocs) {
      os << indent(LIEF::to_string(R), 2);
    }
  }

  if (const Export* exp = get_export()) {
    os << *exp << '\n';
  }

  if (auto sigs = signatures(); !sigs.empty()) {
    os << fmt::format("Signatures (#{})\n", sigs.size());
    for (const Signature& S : sigs) {
      os << indent(LIEF::to_string(S), 2);
    }
  }

  if (const ResourceNode* root = resources()) {
    os << "Resources:\n"
       << indent(LIEF::to_string(*root), 2);
  }

  if (auto syms = symbols(); !syms.empty()) {
    os << fmt::format("COFF Symbols (#{})\n", syms.size());
    for (size_t i = 0; i < syms.size(); ++i) {
      os << fmt::format("Symbol[{:02d}] {{\n", i)
         << indent(LIEF::to_string(syms[i]), 2)
         << "}\n";
    }
  }

  if (auto unwind = this->exceptions(); !unwind.empty()) {
    os << fmt::format("Unwind Info (#{})\n", unwind.size());
    for (const ExceptionInfo& info : unwind) {
      os << indent(LIEF::to_string(info), 2);
    }
  }

  if (const Binary* nested = nested_pe_binary()) {
    os << "Nested PE Binary {\n"
       << indent(LIEF::to_string(*nested), 2)
       << "}";
  }

  return os;
}

} // namesapce PE
} // namespace LIEF
