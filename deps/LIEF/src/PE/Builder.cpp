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
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iterator>

#include "logging.hpp"

#include "LIEF/PE/Builder.hpp"
#include "LIEF/PE/ResourceData.hpp"
#include "LIEF/PE/utils.hpp"
#include "LIEF/PE/ImportEntry.hpp"
#include "LIEF/PE/Import.hpp"
#include "LIEF/PE/Debug.hpp"
#include "LIEF/PE/debug/PDBChecksum.hpp"
#include "LIEF/PE/debug/ExDllCharacteristics.hpp"
#include "LIEF/PE/debug/FPO.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/ResourceDirectory.hpp"
#include "LIEF/PE/DataDirectory.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/Export.hpp"
#include "LIEF/PE/ExportEntry.hpp"
#include "LIEF/utils.hpp"

#include "PE/Structures.hpp"

#include "Builder.tcc"
#include "internal_utils.hpp"

namespace LIEF {
namespace PE {

Builder::~Builder() = default;

void Builder::write(const std::string& filename) const {
  static constexpr auto DEFAULT_MODE = std::ios::out |
                                       std::ios::binary |
                                       std::ios::trunc;
  std::ofstream output_file(filename, DEFAULT_MODE);
  if (!output_file) {
    LIEF_ERR("Can't write in {}", filename);
    return;
  }
  write(output_file);
}

void Builder::write(std::ostream& os) const {
  std::vector<uint8_t> content;
  ios_.move(content);
  os.write(reinterpret_cast<const char*>(content.data()), content.size());
}

ok_error_t Builder::build() {
  LIEF_DEBUG("Build process started");
  const bool is_64bit = binary_->type() == PE_TYPE::PE32_PLUS;

  if (config_.tls) {
    // TLS reconstruction could change relocations. Thus, it needs to be done
    // BEFORE build_relocations. Moreover, because of some relocation
    // optimization, it should be done as early as possible.
    auto res = is_64bit ? build_tls<details::PE64>() :
                          build_tls<details::PE32>();
    if (!res) {
      LIEF_WARN("Something went wrong while re-building TLS: {}",
                to_string(res.error()));
    }
  }

  if (config_.resources) {
    auto is_ok = build_resources();
    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building resource tree: {}",
                to_string(is_ok.error()));
    }
  }

  if (config_.relocations) {
    auto is_ok = build_relocations();
    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building relocations: {}",
                to_string(is_ok.error()));
    }
  }

  if (config_.imports) {
    auto is_ok = is_64bit ? build_imports<details::PE64>() :
                            build_imports<details::PE32>();

    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building imports: {}",
                to_string(is_ok.error()));
    }
  }

  if (config_.exports) {
    auto is_ok = build_exports();
    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building exports: {}",
                to_string(is_ok.error()));
    }
  }

  if (config_.load_configuration) {
    auto is_ok = is_64bit ? build_load_config<details::PE64>() :
                            build_load_config<details::PE32>();

    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building load config: {}",
                to_string(is_ok.error()));
    }
  }

  if (config_.debug) {
    auto is_ok = build_debug_info();
    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building debug entries: {}",
                to_string(is_ok.error()));
    }
  }

  auto is_ok = build(binary_->dos_header());
  if (!is_ok) {
    LIEF_WARN("Something went wrong while re-building DOS header: {}",
              to_string(is_ok.error()));
  }
  is_ok = build(binary_->header());

  if (!is_ok) {
    LIEF_WARN("Something went wrong while re-building regular header: {}",
              to_string(is_ok.error()));
  }

  is_ok = build(binary_->optional_header());

  if (!is_ok) {
    LIEF_WARN("Something went wrong while re-building optional header: {}",
              to_string(is_ok.error()));
  }

  for (const DataDirectory& dir : binary_->data_directories()) {
    auto is_ok = build(dir);
    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building data directory '{}': {}",
                to_string(dir.type()), to_string(is_ok.error()));
    }
  }

  for (const Section& sec : binary_->sections()) {
    auto is_ok = build(sec);
    if (!is_ok) {
      LIEF_WARN("Something went wrong while re-building section '{}': {}",
                sec.name(), to_string(is_ok.error()));
    }
  }

  if (!binary_->overlay().empty() && config_.overlay) {
    build_overlay();
  }

  return ok();
}


ok_error_t Builder::build(const DosHeader& dos_header) {

  vector_iostream dos_hdr_strm;
  dos_hdr_strm.reserve(sizeof(details::pe_dos_header));

  dos_hdr_strm
    .write<uint16_t>(dos_header.magic())
    .write<uint16_t>(dos_header.used_bytes_in_last_page())
    .write<uint16_t>(dos_header.file_size_in_pages())
    .write<uint16_t>(dos_header.numberof_relocation())
    .write<uint16_t>(dos_header.header_size_in_paragraphs())
    .write<uint16_t>(dos_header.minimum_extra_paragraphs())
    .write<uint16_t>(dos_header.maximum_extra_paragraphs())
    .write<uint16_t>(dos_header.initial_relative_ss())
    .write<uint16_t>(dos_header.initial_sp())
    .write<uint16_t>(dos_header.checksum())
    .write<uint16_t>(dos_header.initial_ip())
    .write<uint16_t>(dos_header.initial_relative_cs())
    .write<uint16_t>(dos_header.addressof_relocation_table())
    .write<uint16_t>(dos_header.overlay_number())
    .write<uint16_t>(dos_header.reserved())
    .write<uint16_t>(dos_header.oem_id())
    .write<uint16_t>(dos_header.oem_info())
    .write<uint16_t>(dos_header.reserved2())
    .write<uint32_t>(dos_header.addressof_new_exeheader())
  ;

  assert(dos_hdr_strm.size() == sizeof(details::pe_dos_header));

  ios_.seekp(0);
  ios_.write(dos_hdr_strm);

  if (!binary_->dos_stub().empty() && config_.dos_stub) {
    const uint64_t e_lfanew = sizeof(details::pe_dos_header) +
                              binary_->dos_stub().size();
    if (e_lfanew > dos_header.addressof_new_exeheader()) {
      LIEF_WARN("Inconsistent 'addressof_new_exeheader': 0x{:x}",
                dos_header.addressof_new_exeheader());
    } else {
      ios_.write(binary_->dos_stub());
    }
  }

  return ok();
}


ok_error_t Builder::build(const Header& header) {
  vector_iostream pe_hdr_strm;
  pe_hdr_strm.reserve(sizeof(details::pe_header));

  pe_hdr_strm
    .write(header.signature())
    .write<uint16_t>((int)header.machine())
    .write<uint16_t>(binary_->sections_.size())
    .write<uint32_t>(header.time_date_stamp())
    .write<uint32_t>(header.pointerto_symbol_table())
    .write<uint32_t>(header.numberof_symbols())
    .write<uint16_t>(header.sizeof_optional_header())
    .write<uint16_t>(header.characteristics())
  ;

  assert(pe_hdr_strm.size() == sizeof(details::pe_header));

  ios_
    .seekp(binary_->dos_header().addressof_new_exeheader())
    .write(pe_hdr_strm);
  return ok();
}


ok_error_t Builder::build(const OptionalHeader& optional_header) {
  return binary_->type() == PE_TYPE::PE32 ?
         build_optional_header<details::PE32>(optional_header) :
         build_optional_header<details::PE64>(optional_header);
}


ok_error_t Builder::build(const DataDirectory& data_directory) {
  ios_
    .write<uint32_t>(data_directory.RVA())
    .write<uint32_t>(data_directory.size());

  return ok();
}

ok_error_t Builder::build(const Section& section) {
  std::array<char, 8> section_name;
  std::memset(section_name.data(), 0, section_name.size());

  const std::string& sec_name = section.fullname();
  uint32_t name_length = std::min<uint32_t>(sec_name.size() + 1, section_name.size());
  std::copy(sec_name.c_str(), sec_name.c_str() + name_length,
            std::begin(section_name));
  ios_
    .increase_capacity(sizeof(details::pe_section))
    .write(section_name)
    .write<uint32_t>(section.virtual_size())
    .write<uint32_t>(section.virtual_address())
    .write<uint32_t>(section.size())
    .write<uint32_t>(section.pointerto_raw_data())
    .write<uint32_t>(section.pointerto_relocation())
    .write<uint32_t>(section.pointerto_line_numbers())
    .write<uint16_t>(section.numberof_relocations())
    .write<uint16_t>(section.numberof_line_numbers())
    .write<uint32_t>(section.characteristics())
  ;

  size_t pad_length = 0;
  if (section.content().size() > section.size()) {
    LIEF_WARN("{} content size is bigger than section's header size",
              section.name());
  }
  else {
    pad_length = section.size() - section.content().size();
  }

  {
    ScopeOStream scoped(ios_, section.offset());
    (*scoped)
      .write(section.content())
      .pad(pad_length);
  }

  return ok();
}


ok_error_t Builder::build_overlay() {
  const uint64_t last_section_offset = binary_->last_section_offset();

  LIEF_DEBUG("Overlay offset: 0x{:x}", last_section_offset);
  LIEF_DEBUG("Overlay size: 0x{:x}", binary_->overlay().size());

  ScopeOStream scoped(ios_, last_section_offset);
  (*scoped)
    .write(binary_->overlay());

  return ok();
}

ok_error_t Builder::build_relocations() {
  vector_iostream ios;
  DataDirectory* reloc_dir = binary_->relocation_dir();

  const uint32_t original_size = reloc_dir->size();
  ios.reserve(original_size);

  for (const Relocation& reloc : binary_->relocations()) {
    if (reloc.entries_.empty()) {
      continue;
    }

    const uint32_t block_size =
      static_cast<uint32_t>((reloc.entries().size()) * sizeof(uint16_t) +
      sizeof(details::pe_base_relocation_block));

    ios
      .write<uint32_t>(reloc.virtual_address())
      .write<uint32_t>(align(block_size, sizeof(uint32_t)))
    ;
    for (const RelocationEntry& entry : reloc.entries()) {
      ios.write(entry.data());
    }
  }
  ios
    .align(sizeof(uint32_t))
    .move(reloc_data_);

  if (reloc_data_.size() > original_size || config_.force_relocating) {
    LIEF_DEBUG("Need to relocated BASE_RELOCATION_TABLE (0x{:06x} new bytes)",
               reloc_data_.size() - original_size);

    Section reloc_section(config_.reloc_section);
    const size_t relocated_size =
      align(reloc_data_.size(), binary_->optional_header().file_alignment());

    reloc_section
      .reserve(relocated_size)
      .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
      .add_characteristic(Section::CHARACTERISTICS::MEM_READ)
      .add_characteristic(Section::CHARACTERISTICS::MEM_DISCARDABLE);

    Section* new_reloc_section = binary_->add_section(reloc_section);
    new_reloc_section->edit().write(reloc_data_);
    reloc_dir->RVA(new_reloc_section->virtual_address());
    reloc_dir->size(reloc_data_.size());
  } else {
    LIEF_DEBUG("BASE_RELOCATION_TABLE -0x{:06x}",
               original_size - reloc_data_.size());

    binary_->patch_address(reloc_dir->RVA(), reloc_data_);
  }

  return ok();
}

ok_error_t Builder::build_resources() {
  DataDirectory* rsrc_dir = binary_->rsrc_dir();
  ResourceNode* node = binary_->resources();

  if (node == nullptr) {
    if (rsrc_dir->RVA() == 0 && rsrc_dir->size() == 0) {
      return ok();
    }

    binary_->fill_address(rsrc_dir->RVA(), rsrc_dir->size());
    rsrc_dir->RVA(0);
    rsrc_dir->size(0);
    return ok();
  }

  rsrc_sizing_info_t sizes_info;
  if (!compute_resources_size(*node, sizes_info)) {
    return make_error_code(lief_errors::build_error);
  }

  vector_iostream rsrc_stream;
  rsrc_build_context_t ctx;
  ctx.offset_header = 0;
  ctx.offset_name = ctx.offset_header + sizes_info.header_size;
  ctx.offset_data = align(ctx.offset_name + sizes_info.name_size, 4);

  LIEF_DEBUG(".rsrc[header].offset: 0x{:04x}", ctx.offset_header);
  LIEF_DEBUG(".rsrc[names].offset:  0x{:04x}", ctx.offset_name);
  LIEF_DEBUG(".rsrc[data].offset:   0x{:04x}", ctx.offset_data);

  construct_resource(rsrc_stream, *node, ctx);
  rsrc_stream.align(sizeof(uint32_t));

  const uint32_t original_size = rsrc_dir->size();
  const uint32_t new_size = rsrc_stream.size();
  const uint32_t expected_size = align(sizes_info.data_size + sizes_info.header_size + sizes_info.name_size, 4);
  LIEF_DEBUG("New size: new_size=0x{:016x} vs original_size=0x{:016x} vs expected_size=0x{:016x}", new_size, original_size, expected_size);

  if (new_size > original_size || config_.force_relocating) {
    const uint64_t delta = new_size - original_size;
    LIEF_DEBUG("Need to relocate RESOURCE_TABLE (0x{:06x} new bytes)", delta);
    Section* section = rsrc_dir->section();
    if (section == nullptr) {
      LIEF_ERR("Can't find section associated with the RESOURCE_TABLE");
      return make_error_code(lief_errors::build_error);
    }

    // Note(romain): We could/should leverage this optimization:
    //
    // If the the `.rsrc` section has some kind of padding that is large
    // enough, then we can use this area to avoid relocating the `.rsrc` location
    //
    // const bool padding_is_large = section->padding().size() >= delta;
    // if (padding_is_large) {
    //   LIEF_DEBUG("large padding for new rsrc");
    //   // Now check that the rsrc content is at the "end" of the section
    //   const uint64_t rsrc_offset = binary_->rva_to_offset(rsrc_dir->RVA());
    //   const uint64_t section_size = section->virtual_size() > 0 ?
    //                                 std::min(section->virtual_size(), section->sizeof_raw_data()) :
    //                                 section->sizeof_raw_data();
    //   if ((section->offset() + section_size) - (rsrc_offset + original_size) == 0) {
    //     LIEF_ERR("ok");
    //   }
    // }
    rsrc_stream.align(binary_->optional_header().file_alignment());

    Section rsrc_section(config_.rsrc_section);
    rsrc_section.reserve(rsrc_stream.size());
    rsrc_section
      .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
      .add_characteristic(Section::CHARACTERISTICS::MEM_READ);

    // TODO(romain): Make sure we add this section before the FIRST
    // MEM_DISCARDABLE section.
    Section* new_rsrc_section = binary_->add_section(rsrc_section);
    if (new_rsrc_section == nullptr) {
      LIEF_ERR("Can't add a new rsrc section");
      return make_error_code(lief_errors::build_error);
    }

    rsrc_dir->RVA(new_rsrc_section->virtual_address());
    rsrc_dir->size(new_rsrc_section->size());

    const uint64_t rsrc_rva = rsrc_dir->RVA();
    for (uint64_t fixup : ctx.rva_fixups) {
      rsrc_stream.reloc<uint32_t>(fixup, rsrc_rva, vector_iostream::RELOC_OP::ADD);
    }
    rsrc_stream.move(rsrc_data_);
    new_rsrc_section->content(rsrc_data_);

    return ok();
  }

  LIEF_DEBUG("RESOURCE_TABLE: -0x{:06x}", original_size - new_size);
  // Fill the original content with 0
  const uint64_t rva = rsrc_dir->RVA();
  binary_->patch_address(
    rva, std::vector<uint8_t>(original_size, 0), Binary::VA_TYPES::RVA);

  const uint64_t rsrc_rva = rsrc_dir->RVA();
  for (uint64_t fixup : ctx.rva_fixups) {
    rsrc_stream.reloc<uint32_t>(fixup, rsrc_rva, vector_iostream::RELOC_OP::ADD);
  }

  rsrc_stream.move(rsrc_data_);
  binary_->patch_address(rsrc_dir->RVA(), rsrc_data_, Binary::VA_TYPES::RVA);
  rsrc_dir->size(rsrc_data_.size());

  return ok();
}

ok_error_t Builder::compute_resources_size(
    const ResourceNode& node, rsrc_sizing_info_t& info)
{
  if (!node.name().empty() || node.has_name()) {
    LIEF_DEBUG("{}", u16tou8(node.name()));
    info.name_size += sizeof(uint16_t) + node.name().size() * sizeof(char16_t);
  }


  if (const auto* _ = node.cast<ResourceDirectory>()) {
    info.header_size += sizeof(details::pe_resource_directory_table) +
                        sizeof(details::pe_resource_directory_entries) * node.childs().size();
    for (const ResourceNode& child : node.childs()) {
      compute_resources_size(child, info);
    }
    return ok();
  }

  if (const auto* data = node.cast<ResourceData>()) {
    info.header_size += sizeof(details::pe_resource_data_entry);
    info.data_size   += align(data->content().size(), sizeof(uint32_t));
    return ok();
  }

  return ok();
}
ok_error_t Builder::construct_resource(
    vector_iostream& ios, ResourceDirectory& dir, rsrc_build_context_t& ctx)
{
  LIEF_DEBUG("[rsrc] writing directory header at 0x{:04x} (depth={}, id={}, #children={})",
             ctx.offset_header, dir.depth(), dir.id(), dir.childs().size());
  ios
    .seekp(ctx.offset_header)
    .increase_capacity(sizeof(details::pe_resource_directory_table))
    .write<uint32_t>(dir.characteristics())
    .write<uint32_t>(dir.time_date_stamp())
    .write<uint16_t>(dir.major_version())
    .write<uint16_t>(dir.minor_version())
    .write<uint16_t>(dir.numberof_name_entries())
    .write<uint16_t>(dir.numberof_id_entries())
  ;
  ctx.offset_header = ios.tellp();

  uint32_t current_offset = ctx.offset_header;
  ctx.offset_header += dir.childs().size() * sizeof(details::pe_resource_directory_entries);

  for (ResourceNode& child : dir.childs()) {
    if (child.has_name() || !child.name().empty()) {
      const std::u16string& name = child.name();
      child.id(0x80000000 | ctx.offset_name);
      LIEF_DEBUG("[rsrc] writing name '{}' at 0x{:04x} (depth={}, id={})",
                 u16tou8(child.name()), ctx.offset_name,
                 dir.depth(), dir.id());
      ios
        .seekp(ctx.offset_name)
        .write<uint16_t>(name.size())
        .write(name, /*with_null_char=*/false);
      ctx.offset_name = ios.tellp();
    }

    const uint32_t mask = child.is_directory() ? 0x80000000 : 0;

    ios
      .seekp(current_offset)
      .write<uint32_t>(child.id())
      .write<uint32_t>(mask | ctx.offset_header)
    ;
    current_offset = ios.tellp();

    ++ctx.depth;
    construct_resource(ios, child, ctx);
  }
  return ok();
}


ok_error_t Builder::construct_resource(
    vector_iostream& ios, ResourceData& data, rsrc_build_context_t& ctx)
{
  span<const uint8_t> content = data.content();

  ctx.rva_fixups.push_back(ctx.offset_header);
  LIEF_DEBUG("[rsrc] writing data header at 0x{:04x} (depth={}, id={})",
             ctx.offset_header, data.depth(), data.id());

  ios
    .seekp(ctx.offset_header)
    .increase_capacity(sizeof(details::pe_resource_data_entry))
    .write<uint32_t>(ctx.offset_data)
    .write<uint32_t>(content.size())
    .write<uint32_t>(data.code_page())
    .write<uint32_t>(data.reserved())
  ;
  ctx.offset_header = ios.tellp();

  LIEF_DEBUG("[rsrc] writing data content at 0x{:04x} (depth={}, id={}, size=0x{:04x})",
             ctx.offset_data, data.depth(), data.id(), content.size());
  ios
    .seekp(ctx.offset_data)
    .write(content.data(), content.size())
    .align(sizeof(uint32_t));

  ctx.offset_data = ios.tellp();
  return ok();
}


ok_error_t Builder::construct_resource(
    vector_iostream& ios, ResourceNode& node, rsrc_build_context_t& ctx)
{
  if (auto* dir = node.cast<ResourceDirectory>()) {
    return construct_resource(ios, *dir, ctx);
  }


  if (auto* data = node.cast<ResourceData>()) {
    return construct_resource(ios, *data, ctx);
  }

  return make_error_code(lief_errors::conversion_error);
}

ok_error_t build_codeview(const CodeView& CV, vector_iostream& ios) {
  const auto* cv_pdb = CV.as<CodeViewPDB>();
  if (cv_pdb == nullptr) {
    return make_error_code(lief_errors::not_supported);
  }
  ios
    .write<uint32_t>((uint32_t)CV.signature())
    .write(cv_pdb->signature())
    .write<uint32_t>(cv_pdb->age())
    .write(cv_pdb->filename())
  ;
  return ok();
}

ok_error_t build_pogo(const Pogo& pgo, vector_iostream& ios) {
  switch (pgo.signature()) {
    default:
      return make_error_code(lief_errors::not_supported);
    case Pogo::SIGNATURES::ZERO:
    case Pogo::SIGNATURES::LCTG:
      {
        ios.write<uint32_t>((uint32_t)pgo.signature());
        for (const PogoEntry& entry : pgo.entries()) {
          ios
            .write<uint32_t>(entry.start_rva())
            .write<uint32_t>(entry.size())
            .write(entry.name())
            .align(4)
          ;
        }
        return ok();
      }
  }
  return ok();
}

ok_error_t build_repro(const Repro& repro, vector_iostream& ios) {
  span<const uint8_t> hash = repro.hash();
  ios
    .write<uint32_t>(hash.size())
    .write(hash)
  ;
  return ok();
}


ok_error_t build_pdbchecksum(const PDBChecksum& checksum, vector_iostream& ios) {
  if (checksum.algorithm() == PDBChecksum::HASH_ALGO::UNKNOWN) {
    return make_error_code(lief_errors::not_supported);
  }
  const std::string& algo_name = to_string(checksum.algorithm());
  assert(algo_name != "UNKNOWN");

  ios
    .write(algo_name)
    .write(checksum.hash())
  ;
  return ok();
}

ok_error_t build_vcfeatures(const VCFeature& vcfeature, vector_iostream& ios) {
  const size_t original_size = vcfeature.sizeof_data();
  if ((size_t)ios.tellp() + sizeof(uint32_t) > original_size) {
    return ok();
  }
  ios.write<uint32_t>(vcfeature.pre_vcpp());

  if ((size_t)ios.tellp() + sizeof(uint32_t) > original_size) {
    return ok();
  }
  ios.write<uint32_t>(vcfeature.c_cpp());

  if ((size_t)ios.tellp() + sizeof(uint32_t) > original_size) {
    return ok();
  }
  ios.write<uint32_t>(vcfeature.gs());

  if ((size_t)ios.tellp() + sizeof(uint32_t) > original_size) {
    return ok();
  }
  ios.write<uint32_t>(vcfeature.sdl());

  if ((size_t)ios.tellp() + sizeof(uint32_t) > original_size) {
    return ok();
  }
  ios.write<uint32_t>(vcfeature.guards());
  return ok();
}

ok_error_t build_exdllcharacteristics(const ExDllCharacteristics& characteristics,
                                      vector_iostream& ios)
{
  if (characteristics.sizeof_data() != sizeof(uint32_t)) {
    return make_error_code(lief_errors::not_supported);
  }

  ios.write<uint32_t>((uint32_t)characteristics.characteristics());
  return ok();
}

ok_error_t Builder::build_debug_info() {
  LIEF_DEBUG("Writing back debug info");
  static constexpr auto FX_BASE_RVA = 0;
  static constexpr auto FX_BASE_OFFSET = 1;
  DataDirectory* debug_dir = binary_->debug_dir();

  if (binary_->debug_.empty()) {
    if (debug_dir->RVA() > 0 && debug_dir->size() > 0) {
      binary_->fill_address(debug_dir->RVA(), debug_dir->size());
    }
    debug_dir->RVA(0);
    debug_dir->size(0);
    return ok();
  }

  // Check if we need to relocate the headers
  const size_t hdr_size = binary_->debug_.size() * sizeof(details::pe_debug);

  vector_iostream payload_stream;
  vector_iostream header_stream;

  header_stream.reserve(debug_dir->size());
  header_stream.init_fixups(2);

  const uint32_t hdr_start = 0;
  const uint32_t payload_start = hdr_start + hdr_size;

  uint32_t payload_offset = payload_start;

  for (size_t i = 0; i < binary_->debug_.size(); ++i) {
    const std::unique_ptr<Debug>& dbg = binary_->debug_[i];
    LIEF_DEBUG("debug[{:02}]: {}", i, to_string(dbg->type()));

    uint32_t hdr_pos = hdr_start + i * sizeof(details::pe_debug);
    span<uint8_t> payload = dbg->payload();

    header_stream
      .write<uint32_t>(dbg->characteristics())
      .write<uint32_t>(dbg->timestamp())
      .write<uint16_t>(dbg->major_version())
      .write<uint16_t>(dbg->minor_version())
      .write<uint32_t>((uint32_t)dbg->type())
    ;
    LIEF_DEBUG("  - Header pos:  0x{:06x}", hdr_pos);
    LIEF_DEBUG("  - Payload pos: 0x{:06x}", payload_offset);

    const bool has_offset_rva = dbg->addressof_rawdata() != 0 ||
                                dbg->pointerto_rawdata() != 0;

    if (payload.empty() && has_offset_rva) {
      LIEF_INFO("Empty payload. Can't rebuild the content of {}",
                to_string(dbg->type()));
      header_stream
        .write<uint32_t>(dbg->sizeof_data())
        .write<uint32_t>(dbg->addressof_rawdata())
        .write<uint32_t>(dbg->pointerto_rawdata())
      ;
      continue;
    }

    if (const auto* codeview = dbg->as<CodeView>()) {
      vector_iostream ios_cv;
      ios_cv.reserve(codeview->sizeof_data());
      if (auto is_ok = build_codeview(*codeview, ios_cv); !is_ok) {
        ios_cv.clear().write(payload);
      }
      header_stream.write<uint32_t>(ios_cv.size());

      LIEF_DEBUG("codeview - original size 0x{:06x}", dbg->sizeof_data());
      LIEF_DEBUG("codeview - new size      0x{:06x}", ios_cv.size());

      const int32_t delta = (int32_t)ios_cv.size() - dbg->sizeof_data();

      if (delta <= 0) {
        std::memset(payload.data(), 0, payload.size());
        std::copy(ios_cv.begin(), ios_cv.end(), payload.begin());
        header_stream
          .write<uint32_t>(dbg->addressof_rawdata())
          .write<uint32_t>(dbg->pointerto_rawdata())
        ;
      } else {
        LIEF_DEBUG("Need to relocate entry #{} +{} bytes", i, delta);
        const size_t payload_off = payload_stream.tellp();
        payload_stream.write(ios_cv);
        header_stream
          .record_fixup(FX_BASE_RVA)
          .write<uint32_t>(payload_off)
          .record_fixup(FX_BASE_OFFSET)
          .write<uint32_t>(payload_off)
        ;
      }
      continue;
    }

    if (const auto* pogo = dbg->as<Pogo>()) {
      vector_iostream ios_pogo;
      ios_pogo.reserve(pogo->sizeof_data());
      if (auto is_ok = build_pogo(*pogo, ios_pogo); !is_ok) {
        ios_pogo.clear().write(payload);
      }
      header_stream.write<uint32_t>(ios_pogo.size());

      LIEF_DEBUG("pogo - original size 0x{:06x}", dbg->sizeof_data());
      LIEF_DEBUG("pogo - new size      0x{:06x}", ios_pogo.size());

      const int32_t delta = (int32_t)ios_pogo.size() - dbg->sizeof_data();

      if (delta <= 0) {
        std::memset(payload.data(), 0, payload.size());
        std::copy(ios_pogo.begin(), ios_pogo.end(), payload.begin());
        header_stream
          .write<uint32_t>(dbg->addressof_rawdata())
          .write<uint32_t>(dbg->pointerto_rawdata())
        ;
      } else {
        LIEF_DEBUG("Need to relocate entry #{} +{} bytes", i, delta);
        const size_t payload_off = payload_stream.tellp();
        payload_stream.write(ios_pogo);
        header_stream
          .record_fixup(FX_BASE_RVA)
          .write<uint32_t>(payload_off)
          .record_fixup(FX_BASE_OFFSET)
          .write<uint32_t>(payload_off)
        ;
      }
      continue;
    }

    if (const auto* repro = dbg->as<Repro>()) {
      vector_iostream ios_repro;
      ios_repro.reserve(repro->sizeof_data());
      if (auto is_ok = build_repro(*repro, ios_repro); !is_ok) {
        ios_repro.clear().write(payload);
      }
      header_stream.write<uint32_t>(ios_repro.size());

      LIEF_DEBUG("repro - original size 0x{:06x}", dbg->sizeof_data());
      LIEF_DEBUG("repro - new size      0x{:06x}", ios_repro.size());

      const int32_t delta = (int32_t)ios_repro.size() - dbg->sizeof_data();

      if (delta <= 0) {
        std::memset(payload.data(), 0, payload.size());
        std::copy(ios_repro.begin(), ios_repro.end(), payload.begin());
        header_stream
          .write<uint32_t>(dbg->addressof_rawdata())
          .write<uint32_t>(dbg->pointerto_rawdata())
        ;
      } else {
        LIEF_DEBUG("Need to relocate entry #{} +{} bytes", i, delta);
        const size_t payload_off = payload_stream.tellp();
        payload_stream.write(ios_repro);
        header_stream
          .record_fixup(FX_BASE_RVA)
          .write<uint32_t>(payload_off)
          .record_fixup(FX_BASE_OFFSET)
          .write<uint32_t>(payload_off)
        ;
      }
      continue;
    }

    if (const auto* repro = dbg->as<PDBChecksum>()) {
      vector_iostream ios_repro;
      ios_repro.reserve(repro->sizeof_data());
      if (auto is_ok = build_pdbchecksum(*repro, ios_repro); !is_ok) {
        ios_repro.clear().write(payload);
      }
      header_stream.write<uint32_t>(ios_repro.size());

      LIEF_DEBUG("pdbchecksum - original size 0x{:06x}", dbg->sizeof_data());
      LIEF_DEBUG("pdbchecksum - new size      0x{:06x}", ios_repro.size());

      const int32_t delta = (int32_t)ios_repro.size() - dbg->sizeof_data();

      if (delta <= 0) {
        std::memset(payload.data(), 0, payload.size());
        std::copy(ios_repro.begin(), ios_repro.end(), payload.begin());
        header_stream
          .write<uint32_t>(dbg->addressof_rawdata())
          .write<uint32_t>(dbg->pointerto_rawdata())
        ;
      } else {
        LIEF_DEBUG("Need to relocate entry #{} +{} bytes", i, delta);
        const size_t payload_off = payload_stream.tellp();
        payload_stream.write(ios_repro);
        header_stream
          .record_fixup(FX_BASE_RVA)
          .write<uint32_t>(payload_off)
          .record_fixup(FX_BASE_OFFSET)
          .write<uint32_t>(payload_off)
        ;
      }
      continue;
    }

    if (const auto* vcfeat = dbg->as<VCFeature>()) {
      vector_iostream ios_vcfeat;
      ios_vcfeat.reserve(vcfeat->sizeof_data());
      if (auto is_ok = build_vcfeatures(*vcfeat, ios_vcfeat); !is_ok) {
        ios_vcfeat.clear().write(payload);
      }
      header_stream.write<uint32_t>(ios_vcfeat.size());

      LIEF_DEBUG("vcfeature - original size 0x{:06x}", dbg->sizeof_data());
      LIEF_DEBUG("vcfeature - new size      0x{:06x}", ios_vcfeat.size());

      const int32_t delta = (int32_t)ios_vcfeat.size() - dbg->sizeof_data();

      if (delta <= 0) {
        std::memset(payload.data(), 0, payload.size());
        std::copy(ios_vcfeat.begin(), ios_vcfeat.end(), payload.begin());
        header_stream
          .write<uint32_t>(dbg->addressof_rawdata())
          .write<uint32_t>(dbg->pointerto_rawdata())
        ;
      } else {
        LIEF_DEBUG("Need to relocate entry #{} +{} bytes", i, delta);
        const size_t payload_off = payload_stream.tellp();
        payload_stream.write(ios_vcfeat);
        header_stream
          .record_fixup(FX_BASE_RVA)
          .write<uint32_t>(payload_off)
          .record_fixup(FX_BASE_OFFSET)
          .write<uint32_t>(payload_off)
        ;
      }
      continue;
    }

    if (const auto* exdllchar = dbg->as<ExDllCharacteristics>()) {
      vector_iostream ios_exdll;
      ios_exdll.reserve(exdllchar->sizeof_data());
      if (auto is_ok = build_exdllcharacteristics(*exdllchar, ios_exdll); !is_ok) {
        ios_exdll.clear().write(payload);
      }
      header_stream.write<uint32_t>(ios_exdll.size());

      LIEF_DEBUG("ExDllCharacteristics - original size 0x{:06x}", dbg->sizeof_data());
      LIEF_DEBUG("ExDllCharacteristics - new size      0x{:06x}", ios_exdll.size());

      const int32_t delta = (int32_t)ios_exdll.size() - dbg->sizeof_data();

      if (delta <= 0) {
        std::memset(payload.data(), 0, payload.size());
        std::copy(ios_exdll.begin(), ios_exdll.end(), payload.begin());
        header_stream
          .write<uint32_t>(dbg->addressof_rawdata())
          .write<uint32_t>(dbg->pointerto_rawdata())
        ;
      } else {
        LIEF_DEBUG("Need to relocate entry #{} +{} bytes", i, delta);
        const size_t payload_off = payload_stream.tellp();
        payload_stream.write(ios_exdll);
        header_stream
          .record_fixup(FX_BASE_RVA)
          .write<uint32_t>(payload_off)
          .record_fixup(FX_BASE_OFFSET)
          .write<uint32_t>(payload_off)
        ;
      }
      continue;
    }

    LIEF_DEBUG("original size 0x{:06x}", dbg->sizeof_data());
    LIEF_DEBUG("new size      0x{:06x}", payload.size());
    header_stream.write<uint32_t>(payload.size());
    header_stream
      .write<uint32_t>(dbg->addressof_rawdata())
      .write<uint32_t>(dbg->pointerto_rawdata())
    ;
    continue;
  }

  payload_stream.align(sizeof(uint32_t));

  size_t relocated_size = payload_stream.size();
  bool relocate_hdr = false;

  {
    // Clear the original headers since it will be either: relocated or
    // overwritten
    span<uint8_t> raw = debug_dir->content();
    std::memset(raw.data(), 0, raw.size());
  }

  if (hdr_size > debug_dir->size()) {
    LIEF_DEBUG("Need to relocated debug directory (for headers)");
    relocated_size += header_stream.size();
    relocate_hdr = true;
  }

  if (relocated_size > 0) {
    relocated_size = align(relocated_size,
                           binary_->optional_header().file_alignment());

    Section dbg_section(config_.debug_section);
    dbg_section.reserve(relocated_size);
    dbg_section
      .add_characteristic(Section::CHARACTERISTICS::MEM_DISCARDABLE)
      .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
      .add_characteristic(Section::CHARACTERISTICS::MEM_READ);

    Section* new_dbg_section = binary_->add_section(dbg_section);
    if (new_dbg_section == nullptr) {
      LIEF_ERR("Can't add a new dbg section");
      return make_error_code(lief_errors::build_error);
    }

    LIEF_DEBUG("{:10}: VA:  0x{:08x}", new_dbg_section->name(),
               new_dbg_section->virtual_address());
    LIEF_DEBUG("{:10}: OFF: 0x{:08x}", new_dbg_section->name(),
               new_dbg_section->offset());

    header_stream.apply_fixup<uint32_t>(FX_BASE_RVA, new_dbg_section->virtual_address());
    header_stream.apply_fixup<uint32_t>(FX_BASE_OFFSET, new_dbg_section->offset());

    vector_iostream sec_strm = new_dbg_section->edit();
    sec_strm.write(payload_stream);

    if (relocate_hdr) {
      sec_strm.write(header_stream);
      debug_dir->RVA(new_dbg_section->virtual_address() + payload_stream.size());
    } else {
      span<uint8_t> raw = debug_dir->content();
      assert(header_stream.size() <= raw.size());
      std::copy(header_stream.begin(), header_stream.end(), raw.begin());
    }
  } else {
    span<uint8_t> raw = debug_dir->content();
    assert(header_stream.size() <= raw.size());
    std::copy(header_stream.begin(), header_stream.end(), raw.begin());
  }

  debug_dir->size(header_stream.size());
  return ok();
}


ok_error_t Builder::build_exports() {
  // +-----------------------------+ <-------+ DataDirectory RVA
  // | Header                      |         |                |
  // +-----------------------------+         |                |
  // +-----------------------------+         |                |
  // | Addr Table                  |         |                |
  // +-----------------------------+         |                |
  // +-----------------------------+         |                |
  // | Names Table                 |         |                |
  // +-----------------------------+         |                |
  // +-----------------------------+         |                |
  // | Ord Table                   |         |                |
  // +-----------------------------+         |                |
  // +-----------------------------+         |                |
  // | Strings                     |         |                |
  // |                             |         |                |
  // |                             |         |                v
  // +-----------------------------+ <-------+              + Size

  DataDirectory* exp_dir = binary_->export_dir();
  Export* exp = binary_->get_export();

  if (exp == nullptr) {
    if (exp_dir->RVA() > 0 && exp_dir->size() > 0) {
      binary_->fill_address(exp_dir->RVA(), exp_dir->size());
    }
    exp_dir->RVA(0);
    exp_dir->size(0);
    return ok();
  }

  std::unordered_map<std::string, size_t> strmap_offset;
  std::vector<std::string> strings;
  size_t name_table_cnt = 0;

  uint32_t base_ord = 1 << 16;
  uint32_t max_ord = 0;

  strings.push_back(exp->name());
  std::vector<ExportEntry*> entries;
  entries.reserve(exp->entries_.size());
  std::transform(exp->entries_.begin(), exp->entries_.end(),
    std::back_inserter(entries), [] (std::unique_ptr<ExportEntry>& E) { return E.get(); });

  std::stable_sort(entries.begin(), entries.end(),
    [] (const ExportEntry* lhs, const ExportEntry* rhs) {
      return lhs->name() < rhs->name();
    }
  );

  for (const ExportEntry* entry : entries) {
    LIEF_DEBUG("{}: 0x{:06x} (ord={})", entry->name(), entry->function_rva(), entry->ordinal());
    base_ord = std::min<uint32_t>(entry->ordinal(), base_ord);
    max_ord = std::max<uint32_t>(entry->ordinal(), max_ord);
    bool has_name = false;
    if (ExportEntry::forward_information_t fwd = entry->forward_information()) {
      strings.push_back(fwd.key());
      strings.push_back(entry->name());
      has_name = true;
    } else {
      strings.push_back(entry->name());
      has_name = true;
    }
    if (has_name) {
      ++name_table_cnt;
    }
  }

  size_t addr_table_cnt = max_ord;

  size_t offset_counter = 0;
  std::vector<std::string> strings_opt =
    optimize(strings, [] (const std::string& s) { return s; }, offset_counter,
             &strmap_offset);

  uint32_t head_off = 0;
  uint32_t addr_table_off = head_off + sizeof(details::pe_export_directory_table);
  uint32_t names_table_off = addr_table_off + addr_table_cnt * sizeof(uint32_t);
  uint32_t ord_table_off = names_table_off + name_table_cnt * sizeof(uint32_t);
  uint32_t str_table_off = ord_table_off + name_table_cnt * sizeof(uint16_t);
  const uint32_t tail_off = align(str_table_off + offset_counter, sizeof(uint32_t));

  LIEF_DEBUG("Address table count: #{:03d}", addr_table_cnt);
  LIEF_DEBUG("Name table count:    #{:03d}", name_table_cnt);
  LIEF_DEBUG("Base ordinal:        #{:03d}", base_ord);
  LIEF_DEBUG("Max ordinal:         #{:03d}", max_ord);
  LIEF_DEBUG("String table size:   0x{:06x}", offset_counter);
  LIEF_DEBUG("Original size:       0x{:06x}", exp_dir->size());
  LIEF_DEBUG("New size:            0x{:06x}", tail_off);
  LIEF_DEBUG("Delta size:          {:d}", (int32_t)tail_off - (int32_t)exp_dir->size());
  LIEF_DEBUG("Offsets");
  LIEF_DEBUG("  - Header:        [0x{:04x}, 0x{:08x}]", head_off, addr_table_off);
  LIEF_DEBUG("  - Address Table: [0x{:04x}, 0x{:08x}]", addr_table_off, names_table_off);
  LIEF_DEBUG("  - Names Table:   [0x{:04x}, 0x{:08x}]", names_table_off, ord_table_off);
  LIEF_DEBUG("  - Ord Table:     [0x{:04x}, 0x{:08x}]", ord_table_off, str_table_off);
  LIEF_DEBUG("  - Str Table:     [0x{:04x}, 0x{:08x}]", str_table_off, tail_off);

  vector_iostream ios;
  static constexpr auto FX_BASE_RVA = 0;

  uint32_t name_off = 0;
  if (auto it = strmap_offset.find(exp->name()); it != strmap_offset.end()) {
    name_off = str_table_off + it->second;
  } else {
    LIEF_ERR("Can't find '{}' in the optimized string table");
    return make_error_code(lief_errors::build_error);
  }

  ios
    .reserve(tail_off).init_fixups(1)
    .write<uint32_t>(exp->export_flags())
    .write<uint32_t>(exp->timestamp())
    .write<uint16_t>(exp->major_version())
    .write<uint16_t>(exp->minor_version())
    .record_fixup(FX_BASE_RVA)
    .write<uint32_t>(name_off)
    .write<uint32_t>(base_ord)
    .write<uint32_t>(addr_table_cnt)
    .write<uint32_t>(name_table_cnt)
    .record_fixup(FX_BASE_RVA)
    .write<uint32_t>(addr_table_off)
    .record_fixup(FX_BASE_RVA)
    .write<uint32_t>(names_table_off)
    .record_fixup(FX_BASE_RVA)
    .write<uint32_t>(ord_table_off)
  ;
  size_t name_idx = 0;
  for (const ExportEntry* E : entries) {
    int32_t idx = E->ordinal() - base_ord;
    assert(idx >= 0);
    uint32_t rva = E->function_rva();
    if (rva == 0) {
      rva = E->address();
    }

    if (auto fwd = E->forward_information()) {
      const std::string& fwd_key = fwd.key();
      LIEF_DEBUG("Writing back fwd: {}", fwd_key);
      auto it = strmap_offset.find(fwd_key);
      if (it == strmap_offset.end()) {
        LIEF_ERR("Can't find forwarded key '{}' in the optimized string table",
                 fwd_key);
        return make_error_code(lief_errors::build_error);
      }

      auto it_name = strmap_offset.find(E->name());
      if (it_name == strmap_offset.end()) {
        LIEF_ERR("Can't find name '{}' in the optimized string table", E->name());
        return make_error_code(lief_errors::build_error);
      }
      ios
        // Write addr table
        .seekp(addr_table_off + idx * sizeof(uint32_t))
        .record_fixup(FX_BASE_RVA)
        .write<uint32_t>(str_table_off + it->second)
        // Write ordinal value
        .seekp(ord_table_off + name_idx * sizeof(uint16_t))
        .write<uint16_t>(E->ordinal() - base_ord)
        // Write name table
        .seekp(names_table_off + name_idx * sizeof(uint32_t))
        .record_fixup(FX_BASE_RVA)
        .write<uint32_t>(str_table_off + it_name->second);
      ++name_idx;
    } else if (const std::string& name = E->name(); !name.empty()){
      auto it = strmap_offset.find(name);

      if (it == strmap_offset.end()) {
        LIEF_ERR("Can't find '{}' in the optimized string table", name);
        return make_error_code(lief_errors::build_error);
      }

      ios
        .seekp(addr_table_off + idx * sizeof(uint32_t))
        .write<uint32_t>(rva)
        .seekp(ord_table_off + name_idx * sizeof(uint16_t))
        .write<uint16_t>(E->ordinal() - base_ord)
        .seekp(names_table_off + name_idx * sizeof(uint32_t))
        .record_fixup(FX_BASE_RVA)
        .write<uint32_t>(str_table_off + it->second);
      ;
      ++name_idx;
    }
  }

  // Write string
  ios.seekp(str_table_off);
  for (const std::string& str : strings_opt) {
    ios.write(str);
  }

  ios.align(sizeof(uint32_t));

  const size_t relocated_size = align(tail_off,
                                      binary_->optional_header().file_alignment());

  Section edata_section(config_.export_section);
  edata_section.reserve(relocated_size);
  edata_section
    .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
    .add_characteristic(Section::CHARACTERISTICS::MEM_READ);


  Section* new_edata_section = binary_->add_section(edata_section);
  if (new_edata_section == nullptr) {
    LIEF_ERR("Can't add a new edata section");
    return make_error_code(lief_errors::build_error);
  }
  exp_dir->RVA(new_edata_section->virtual_address());
  exp_dir->size(tail_off);

  ios.apply_fixup<uint32_t>(FX_BASE_RVA, exp_dir->RVA());

  new_edata_section->content(ios.raw());
  return ok();
}

}
}
