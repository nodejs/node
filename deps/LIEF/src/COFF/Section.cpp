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
#include <sstream>
#include <spdlog/fmt/fmt.h>
#include "LIEF/COFF/Section.hpp"
#include "LIEF/COFF/Symbol.hpp"
#include "LIEF/COFF/AuxiliarySymbol.hpp"
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarySectionDefinition.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "PE/Structures.hpp"

#include "logging.hpp"

namespace LIEF::COFF {
std::unique_ptr<Section> Section::parse(BinaryStream& stream) {
  auto raw_sec = stream.read<LIEF::PE::details::pe_section>();
  if (!raw_sec) {
    return nullptr;
  }
  auto sec = std::unique_ptr<Section>(new Section());

  sec->virtual_size_ = raw_sec->VirtualSize;
  sec->pointer_to_relocations_ = raw_sec->PointerToRelocations;
  sec->pointer_to_linenumbers_ = raw_sec->PointerToLineNumbers;
  sec->number_of_relocations_ = raw_sec->NumberOfRelocations;
  sec->number_of_linenumbers_ = raw_sec->NumberOfLineNumbers;
  sec->characteristics_ = raw_sec->Characteristics;
  sec->name_ = std::string(raw_sec->Name, sizeof(raw_sec->Name));
  sec->virtual_address_ = raw_sec->VirtualAddress;
  sec->size_ = raw_sec->SizeOfRawData;
  sec->offset_ = raw_sec->PointerToRawData;

  stream.peek_data(sec->content_, sec->offset(), sec->size(),
                   sec->virtual_address());

  return sec;
}

void Section::name(std::string name) {
  if (name.size() > LIEF::PE::Section::MAX_SECTION_NAME) {
    LIEF_ERR("The max size of a section's name is {} vs {}",
             LIEF::PE::Section::MAX_SECTION_NAME, name.size());
    return;
  }
  name_ = std::move(name);
}

optional<Section::ComdatInfo> Section::comdat_info() const {
  // Implementation following MS documentation:
  // https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#comdat-sections-object-only
  if (!has_characteristic(Section::CHARACTERISTICS::LNK_COMDAT)) {
    return nullopt();
  }

  // [...] The first symbol that has the section value of the COMDAT section
  // must be the section symbol
  if (symbols_.size() < 2) {
    return nullopt();
  }

  ComdatInfo info;
  {
    const Symbol* sym = symbols_[0];
    if (sym->value() != 0) {
      return nullopt();
    }

    if (sym->base_type() != Symbol::BASE_TYPE::TY_NULL) {
      return nullopt();
    }

    if (sym->storage_class() != Symbol::STORAGE_CLASS::STATIC) {
      return nullopt();
    }

    if (sym->auxiliary_symbols().empty()) {
      return nullopt();
    }

    const AuxiliarySymbol& aux = sym->auxiliary_symbols()[0];
    const auto* aux_secdef = aux.as<AuxiliarySectionDefinition>();
    if (aux_secdef == nullptr) {
      return nullopt();
    }
    info.kind = aux_secdef->selection();
  }

  info.symbol = symbols_[1];
  return info;
}

std::string Section::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 24;
  std::ostringstream os;

  const auto& list = characteristics_list();
  std::vector<std::string> list_str;
  list_str.reserve(list.size());
  std::transform(list.begin(), list.end(), std::back_inserter(list_str),
                 [] (const auto c) { return COFF::to_string(c); });

  std::vector<std::string> fullname_hex;
  fullname_hex.reserve(name().size());
  std::transform(fullname().begin(), fullname().end(),
                 std::back_inserter(fullname_hex),
                 [] (const char c) { return format("{:02x}", c); });

  os << format("{:{}} {} ({})\n", "Name:", WIDTH, name(),
               join(fullname_hex, " "));

  os << format("{:{}} 0x{:x}\n", "Virtual Size", WIDTH, virtual_size())
     << format("{:{}} 0x{:x}\n", "Virtual Address", WIDTH, virtual_address())
     << format("{:{}} 0x{:x}\n", "Size of raw data", WIDTH, sizeof_raw_data())
     << format("{:{}} 0x{:x}\n", "Pointer to raw data", WIDTH, pointerto_raw_data())
     << format("{:{}} [0x{:08x}, 0x{:08x}]\n", "Range", WIDTH,
               pointerto_raw_data(), pointerto_raw_data() + sizeof_raw_data())
     << format("{:{}} 0x{:x}\n", "Pointer to relocations", WIDTH, pointerto_relocation())
     << format("{:{}} 0x{:x}\n", "Pointer to line numbers", WIDTH, pointerto_line_numbers())
     << format("{:{}} 0x{:x}\n", "Number of relocations", WIDTH, numberof_relocations())
     << format("{:{}} 0x{:x}\n", "Number of lines", WIDTH, numberof_line_numbers())
     << format("{:{}} {}", "Characteristics", WIDTH, join(list_str, ", "));

  return os.str();

}

}
