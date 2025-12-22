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
#include "LIEF/PE/Factory.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/LoadConfigurations.hpp"
#include "LIEF/PE/TLS.hpp"
#include "LIEF/PE/RichHeader.hpp"
#include "LIEF/PE/ResourceNode.hpp"
#include "LIEF/PE/Export.hpp"
#include "LIEF/PE/Debug.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/Section.hpp"

#include "PE/Structures.hpp"

#include <sstream>

namespace LIEF::PE {

Factory::Factory(Factory&&) = default;
Factory& Factory::operator=(Factory&&) = default;

Factory::~Factory() = default;

std::unique_ptr<Factory> Factory::create(PE_TYPE type) {
  auto factory = std::unique_ptr<Factory>(new Factory());
  Binary& pe = *factory->pe_;
  pe.type_ = type;
  pe.dos_header_ = DosHeader::create(type);
  pe.header_ = Header::create(type);
  pe.optional_header_ = OptionalHeader::create(type);

  pe.dos_stub_ = {
    0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd, 0x21, 0xb8, 0x01, 0x4c,
    0xcd, 0x21, 0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72,
    0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f, 0x74, 0x20, 0x62, 0x65,
    0x20, 0x72, 0x75, 0x6e, 0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20,
    0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x24, 0x00, 0x00
  };

  pe.data_directories_.reserve(DataDirectory::DEFAULT_NB);
  for (size_t i = 0; i < DataDirectory::DEFAULT_NB; ++i) {
    pe.data_directories_.push_back(std::make_unique<DataDirectory>(DataDirectory::TYPES(i)));
  }

  return factory;
}

std::unique_ptr<Binary> Factory::process() {
  update_headers();
  assign_locations();
  move_sections();
  check_overlapping();

  Builder::config_t config;
  std::ostringstream oss;
  pe_->write(oss, config);

  std::string buffer = oss.str();
  return Parser::parse((const uint8_t*)buffer.data(), buffer.size());
}

ok_error_t Factory::assign_locations() {
  uint64_t last_offset = sizeof_headers();
  uint64_t last_rva = section_align();
  for (std::unique_ptr<Section>& section : sections_) {
    last_offset = std::max<uint64_t>(section->offset() + section->size(), last_offset);
    last_rva = std::max<uint64_t>(section->virtual_address() + section->virtual_size(), last_rva);
  }

  last_offset = align(last_offset, file_align());
  last_rva = align(last_offset, section_align());
  for (std::unique_ptr<Section>& section : sections_) {
    if (section->virtual_size() == 0) {
      section->virtual_size(section->sizeof_raw_data());
    }

    if (section->offset() == 0) {
      section->offset(last_offset);
      last_offset = align(last_offset + section->size(), file_align());
    }

    if (section->virtual_address() == 0) {
      section->virtual_address(last_rva);
      last_rva = align(last_rva + section->virtual_size(), section_align());
    }
  }
  return ok();
}

uint32_t Factory::sizeof_headers() const {
  return align(
    pe_->dos_header().addressof_new_exeheader() +
    sizeof(details::pe_header) +
    (is_32bit() ? sizeof(details::pe32_optional_header) :
                  sizeof(details::pe64_optional_header)) +
    sizeof(details::pe_data_directory) * pe_->data_directories_.size() +
    sizeof(details::pe_section) * sections_.size(),
    file_align());
}


ok_error_t Factory::update_headers() {
  pe_->header().numberof_sections(sections_.size());
  return ok();
}


ok_error_t Factory::move_sections() {
  pe_->sections_ = std::move(sections_);
  return ok();
}

ok_error_t Factory::check_overlapping() const {
  // check that no section are overlapping
  return ok();
}
}
