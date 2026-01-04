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
#include <iterator>
#include <spdlog/fmt/fmt.h>

#include "frozen.hpp"
#include "logging.hpp"
#include "PE/Structures.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/PE/Section.hpp"
#include "LIEF/COFF/String.hpp"

namespace LIEF {
namespace PE {

static constexpr std::array CHARACTERISTICS_LIST = {
  Section::CHARACTERISTICS::TYPE_NO_PAD,
  Section::CHARACTERISTICS::CNT_CODE,
  Section::CHARACTERISTICS::CNT_INITIALIZED_DATA,
  Section::CHARACTERISTICS::CNT_UNINITIALIZED_DATA,
  Section::CHARACTERISTICS::LNK_OTHER,
  Section::CHARACTERISTICS::LNK_INFO,
  Section::CHARACTERISTICS::LNK_REMOVE,
  Section::CHARACTERISTICS::LNK_COMDAT,
  Section::CHARACTERISTICS::GPREL,
  Section::CHARACTERISTICS::MEM_PURGEABLE,
  Section::CHARACTERISTICS::MEM_16BIT,
  Section::CHARACTERISTICS::MEM_LOCKED,
  Section::CHARACTERISTICS::MEM_PRELOAD,
  Section::CHARACTERISTICS::ALIGN_1BYTES,
  Section::CHARACTERISTICS::ALIGN_2BYTES,
  Section::CHARACTERISTICS::ALIGN_4BYTES,
  Section::CHARACTERISTICS::ALIGN_8BYTES,
  Section::CHARACTERISTICS::ALIGN_16BYTES,
  Section::CHARACTERISTICS::ALIGN_32BYTES,
  Section::CHARACTERISTICS::ALIGN_64BYTES,
  Section::CHARACTERISTICS::ALIGN_128BYTES,
  Section::CHARACTERISTICS::ALIGN_256BYTES,
  Section::CHARACTERISTICS::ALIGN_512BYTES,
  Section::CHARACTERISTICS::ALIGN_1024BYTES,
  Section::CHARACTERISTICS::ALIGN_2048BYTES,
  Section::CHARACTERISTICS::ALIGN_4096BYTES,
  Section::CHARACTERISTICS::ALIGN_8192BYTES,
  Section::CHARACTERISTICS::LNK_NRELOC_OVFL,
  Section::CHARACTERISTICS::MEM_DISCARDABLE,
  Section::CHARACTERISTICS::MEM_NOT_CACHED,
  Section::CHARACTERISTICS::MEM_NOT_PAGED,
  Section::CHARACTERISTICS::MEM_SHARED,
  Section::CHARACTERISTICS::MEM_EXECUTE,
  Section::CHARACTERISTICS::MEM_READ,
  Section::CHARACTERISTICS::MEM_WRITE,
};

Section::Section(const details::pe_section& header) :
  virtual_size_{header.VirtualSize},
  pointer_to_relocations_{header.PointerToRelocations},
  pointer_to_linenumbers_{header.PointerToLineNumbers},
  number_of_relocations_{header.NumberOfRelocations},
  number_of_linenumbers_{header.NumberOfLineNumbers},
  characteristics_{header.Characteristics}
{
  name_            = std::string(header.Name, sizeof(header.Name));
  virtual_address_ = header.VirtualAddress;
  size_            = header.SizeOfRawData;
  offset_          = header.PointerToRawData;
}

void Section::name(std::string name) {
  if (name.size() > MAX_SECTION_NAME) {
    LIEF_ERR("The max size of a section's name is {} vs {}", MAX_SECTION_NAME,
             name.size());
    return;
  }
  name_ = std::move(name);
}

uint32_t Section::sizeof_raw_data() const {
  return size();
}

uint32_t Section::pointerto_raw_data() const {
  return offset();
}

std::vector<Section::CHARACTERISTICS> Section::characteristics_to_list(uint32_t value) {
  std::vector<Section::CHARACTERISTICS> list;
  list.reserve(3);
  std::copy_if(CHARACTERISTICS_LIST.begin(), CHARACTERISTICS_LIST.end(),
               std::back_inserter(list),
               [value] (CHARACTERISTICS c) { return (value & (uint32_t)c) != 0; });

  return list;
}

void Section::content(const std::vector<uint8_t>& data) {
  content_ = data;
}

void Section::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::unique_ptr<SpanStream> Section::stream() const {
  return std::make_unique<SpanStream>(content());
}

void Section::clear(uint8_t c) {
  std::fill(std::begin(content_), std::end(content_), c);
}

std::ostream& operator<<(std::ostream& os, const Section& section) {
  using namespace fmt;
  static constexpr auto WIDTH = 24;
  const auto& list = section.characteristics_list();
  std::vector<std::string> list_str;
  list_str.reserve(list.size());
  std::transform(list.begin(), list.end(), std::back_inserter(list_str),
                 [] (const auto c) { return to_string(c); });

  std::vector<std::string> fullname_hex;
  fullname_hex.reserve(section.name().size());
  std::transform(section.fullname().begin(), section.fullname().end(),
                 std::back_inserter(fullname_hex),
                 [] (const char c) { return format("{:02x}", c); });

  if (const COFF::String* coff_str = section.coff_string()) {
    os << format("{:{}} {} ({}, {})\n", "Name:", WIDTH, section.name(),
                 join(fullname_hex, " "), coff_str->str());
  } else {
    os << format("{:{}} {} ({})\n", "Name:", WIDTH, section.name(),
                 join(fullname_hex, " "));
  }

  os << format("{:{}} 0x{:x}\n", "Virtual Size", WIDTH, section.virtual_size())
     << format("{:{}} 0x{:x}\n", "Virtual Address", WIDTH, section.virtual_address())
     << format("{:{}} [0x{:08x}, 0x{:08x}]\n", "Range", WIDTH,
               section.virtual_address(), section.virtual_address() + section.virtual_size())
     << format("{:{}} 0x{:x}\n", "Size of raw data", WIDTH, section.sizeof_raw_data())
     << format("{:{}} 0x{:x}\n", "Pointer to raw data", WIDTH, section.pointerto_raw_data())
     << format("{:{}} [0x{:08x}, 0x{:08x}]\n", "Range", WIDTH,
               section.pointerto_raw_data(), section.pointerto_raw_data() + section.sizeof_raw_data())
     << format("{:{}} 0x{:x}\n", "Pointer to relocations", WIDTH, section.pointerto_relocation())
     << format("{:{}} 0x{:x}\n", "Pointer to line numbers", WIDTH, section.pointerto_line_numbers())
     << format("{:{}} 0x{:x}\n", "Number of relocations", WIDTH, section.numberof_relocations())
     << format("{:{}} 0x{:x}\n", "Number of lines", WIDTH, section.numberof_line_numbers())
     << format("{:{}} {}", "Characteristics", WIDTH, join(list_str, ", "));
  return os;
}


const char* to_string(Section::CHARACTERISTICS e) {
  CONST_MAP(Section::CHARACTERISTICS, const char*, 35) enumStrings {
    { Section::CHARACTERISTICS::TYPE_NO_PAD,            "TYPE_NO_PAD" },
    { Section::CHARACTERISTICS::CNT_CODE,               "CNT_CODE" },
    { Section::CHARACTERISTICS::CNT_INITIALIZED_DATA,   "CNT_INITIALIZED_DATA" },
    { Section::CHARACTERISTICS::CNT_UNINITIALIZED_DATA, "CNT_UNINITIALIZED_DATA" },
    { Section::CHARACTERISTICS::LNK_OTHER,              "LNK_OTHER" },
    { Section::CHARACTERISTICS::LNK_INFO,               "LNK_INFO" },
    { Section::CHARACTERISTICS::LNK_REMOVE,             "LNK_REMOVE" },
    { Section::CHARACTERISTICS::LNK_COMDAT,             "LNK_COMDAT" },
    { Section::CHARACTERISTICS::GPREL,                  "GPREL" },
    { Section::CHARACTERISTICS::MEM_PURGEABLE,          "MEM_PURGEABLE" },
    { Section::CHARACTERISTICS::MEM_16BIT,              "MEM_16BIT" },
    { Section::CHARACTERISTICS::MEM_LOCKED,             "MEM_LOCKED" },
    { Section::CHARACTERISTICS::MEM_PRELOAD,            "MEM_PRELOAD" },
    { Section::CHARACTERISTICS::ALIGN_1BYTES,           "ALIGN_1BYTES" },
    { Section::CHARACTERISTICS::ALIGN_2BYTES,           "ALIGN_2BYTES" },
    { Section::CHARACTERISTICS::ALIGN_4BYTES,           "ALIGN_4BYTES" },
    { Section::CHARACTERISTICS::ALIGN_8BYTES,           "ALIGN_8BYTES" },
    { Section::CHARACTERISTICS::ALIGN_16BYTES,          "ALIGN_16BYTES" },
    { Section::CHARACTERISTICS::ALIGN_32BYTES,          "ALIGN_32BYTES" },
    { Section::CHARACTERISTICS::ALIGN_64BYTES,          "ALIGN_64BYTES" },
    { Section::CHARACTERISTICS::ALIGN_128BYTES,         "ALIGN_128BYTES" },
    { Section::CHARACTERISTICS::ALIGN_256BYTES,         "ALIGN_256BYTES" },
    { Section::CHARACTERISTICS::ALIGN_512BYTES,         "ALIGN_512BYTES" },
    { Section::CHARACTERISTICS::ALIGN_1024BYTES,        "ALIGN_1024BYTES" },
    { Section::CHARACTERISTICS::ALIGN_2048BYTES,        "ALIGN_2048BYTES" },
    { Section::CHARACTERISTICS::ALIGN_4096BYTES,        "ALIGN_4096BYTES" },
    { Section::CHARACTERISTICS::ALIGN_8192BYTES,        "ALIGN_8192BYTES" },
    { Section::CHARACTERISTICS::LNK_NRELOC_OVFL,        "LNK_NRELOC_OVFL" },
    { Section::CHARACTERISTICS::MEM_DISCARDABLE,        "MEM_DISCARDABLE" },
    { Section::CHARACTERISTICS::MEM_NOT_CACHED,         "MEM_NOT_CACHED" },
    { Section::CHARACTERISTICS::MEM_NOT_PAGED,          "MEM_NOT_PAGED" },
    { Section::CHARACTERISTICS::MEM_SHARED,             "MEM_SHARED" },
    { Section::CHARACTERISTICS::MEM_EXECUTE,            "MEM_EXECUTE" },
    { Section::CHARACTERISTICS::MEM_READ,               "MEM_READ" },
    { Section::CHARACTERISTICS::MEM_WRITE,              "MEM_WRITE" }
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}

}
}
