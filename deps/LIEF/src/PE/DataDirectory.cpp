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
#include <ostream>

#include "LIEF/Visitor.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/DataDirectory.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "PE/Structures.hpp"

#include "frozen.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

DataDirectory::DataDirectory(const details::pe_data_directory& header,
                             DataDirectory::TYPES type) :
  rva_{header.RelativeVirtualAddress},
  size_{header.Size},
  type_{type}
{}

void DataDirectory::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::unique_ptr<SpanStream> DataDirectory::stream(bool sized) const {
  if (sized) {
    return std::make_unique<SpanStream>(content());
  }

  if (section_ == nullptr) {
    return nullptr;
  }

  span<const uint8_t> section_content = section_->content();
  if (section_content.empty()) {
    return std::make_unique<SpanStream>(section_content);
  }

  int64_t rel_offset = RVA() - section_->virtual_address();

  if (rel_offset < 0 || (uint64_t)rel_offset >= section_content.size() ||
      ((uint64_t)rel_offset + size()) > section_content.size())
  {
    return std::make_unique<SpanStream>(nullptr, 0);
  }

  span<uint8_t> buffer = section_->writable_content().subspan(rel_offset);
  return std::make_unique<SpanStream>(buffer);
}

span<uint8_t> DataDirectory::content() {
  if (section_ == nullptr) {
    return {};
  }

  span<const uint8_t> section_content = section_->content();
  if (section_content.empty()) {
    return {};
  }

  int64_t rel_offset = RVA() - section_->virtual_address();

  if (rel_offset < 0 || (uint64_t)rel_offset >= section_content.size() ||
      ((uint64_t)rel_offset + size()) > section_content.size())
  {
    return {};
  }

  return section_->writable_content().subspan(rel_offset, size());
}

std::ostream& operator<<(std::ostream& os, const DataDirectory& entry) {
  os << fmt::format("[{:>24}] [0x{:08x}, 0x{:08x}] ({} bytes)",
                    to_string(entry.type()), entry.RVA(), entry.RVA() + entry.size(),
                    entry.size());
  if (const Section* section = entry.section()) {
    os << fmt::format(" section: '{}'", section->name());
  }
  return os;
}

const char* to_string(DataDirectory::TYPES e) {
  #define ENTRY(X) std::pair(DataDirectory::TYPES::X, #X)
  STRING_MAP enums2str {
    ENTRY(EXPORT_TABLE),
    ENTRY(IMPORT_TABLE),
    ENTRY(RESOURCE_TABLE),
    ENTRY(EXCEPTION_TABLE),
    ENTRY(CERTIFICATE_TABLE),
    ENTRY(BASE_RELOCATION_TABLE),
    ENTRY(DEBUG_DIR),
    ENTRY(ARCHITECTURE),
    ENTRY(GLOBAL_PTR),
    ENTRY(TLS_TABLE),
    ENTRY(LOAD_CONFIG_TABLE),
    ENTRY(BOUND_IMPORT),
    ENTRY(IAT),
    ENTRY(DELAY_IMPORT_DESCRIPTOR),
    ENTRY(CLR_RUNTIME_HEADER),
    ENTRY(RESERVED),
    ENTRY(UNKNOWN),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
