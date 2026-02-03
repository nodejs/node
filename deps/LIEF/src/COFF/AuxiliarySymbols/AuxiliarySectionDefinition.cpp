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
#include "LIEF/COFF/AuxiliarySymbols/AuxiliarySectionDefinition.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"

#include "COFF/structures.hpp"

namespace LIEF::COFF {

std::unique_ptr<AuxiliarySectionDefinition>
  AuxiliarySectionDefinition::parse(const std::vector<uint8_t>& payload)
{
  const bool isbigobj = payload.size() == sizeof(details::symbol32);

  SpanStream stream(payload);
  auto Length = stream.read<uint32_t>();
  if (!Length) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  auto NumberOfRelocations = stream.read<uint16_t>();
  if (!NumberOfRelocations) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  auto NumberOfLinenumbers = stream.read<uint16_t>();
  if (!NumberOfLinenumbers) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  auto CheckSum = stream.read<uint32_t>();
  if (!CheckSum) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  auto Number = stream.read<uint16_t>();
  if (!Number) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  auto Selection = stream.read<uint8_t>();
  if (!Selection) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  if (!isbigobj) {
    return std::make_unique<AuxiliarySectionDefinition>(
      *Length, *NumberOfRelocations, *NumberOfLinenumbers, *CheckSum, *Number,
      *Selection, /*reserved=*/0
    );
  }


  auto bReserved = stream.read<uint8_t>();
  if (!bReserved) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  auto HighNumber = stream.read<uint16_t>();
  if (!HighNumber) {
    LIEF_WARN("AuxiliarySectionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliarySectionDefinition>();
  }

  const uint32_t nb_sections = *HighNumber << 16 | *Number;

  return std::make_unique<AuxiliarySectionDefinition>(
    *Length, *NumberOfRelocations, *NumberOfLinenumbers, *CheckSum, nb_sections,
    *Selection, *bReserved
  );
}

std::string AuxiliarySectionDefinition::to_string() const {
  std::ostringstream oss;
  oss << "AuxiliarySectionDefinition {\n";
  oss << fmt::format("  Length: 0x{:06x}\n", length());
  oss << fmt::format("  Number of relocations: {}\n", nb_relocs());
  oss << fmt::format("  Number of line numbers: {}\n", nb_line_numbers());
  oss << fmt::format("  Checksum: 0x{:08x}\n", checksum());
  oss << fmt::format("  Section index: {}\n", section_idx());
  oss << fmt::format("  Selection: {}\n", COFF::to_string(selection()));
  oss << fmt::format("  Reserved: {}\n", reserved());
  oss << "}\n";
  return oss.str();
}


const char* to_string(AuxiliarySectionDefinition::COMDAT_SELECTION e) {
  using COMDAT_SELECTION = AuxiliarySectionDefinition::COMDAT_SELECTION;
  switch (e) {
    case COMDAT_SELECTION::NONE: return "NONE";
    case COMDAT_SELECTION::NODUPLICATES: return "NODUPLICATES";
    case COMDAT_SELECTION::ANY: return "ANY";
    case COMDAT_SELECTION::SAME_SIZE: return "SAME_SIZE";
    case COMDAT_SELECTION::EXACT_MATCH: return "EXACT_MATCH";
    case COMDAT_SELECTION::ASSOCIATIVE: return "ASSOCIATIVE";
    case COMDAT_SELECTION::LARGEST: return "LARGEST";
  }
  return "NONE";
}
}
