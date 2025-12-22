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

#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryFunctionDefinition.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "logging.hpp"

namespace LIEF::COFF {

std::unique_ptr<AuxiliaryFunctionDefinition>
  AuxiliaryFunctionDefinition::parse(const std::vector<uint8_t>& payload)
{
  SpanStream stream(payload);
  auto TagIndex = stream.read<uint32_t>();
  if (!TagIndex) {
    LIEF_WARN("AuxiliaryFunctionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryFunctionDefinition>();
  }

  auto TotalSize = stream.read<uint32_t>();
  if (!TotalSize) {
    LIEF_WARN("AuxiliaryFunctionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryFunctionDefinition>();
  }

  auto PointerToLinenumber = stream.read<uint32_t>();
  if (!PointerToLinenumber) {
    LIEF_WARN("AuxiliaryFunctionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryFunctionDefinition>();
  }

  auto PointerToNextFunction = stream.read<uint32_t>();
  if (!PointerToNextFunction) {
    LIEF_WARN("AuxiliaryFunctionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryFunctionDefinition>();
  }

  auto Unused = stream.read<uint16_t>();
  if (!Unused) {
    LIEF_WARN("AuxiliaryFunctionDefinition error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryFunctionDefinition>();
  }

  return std::make_unique<AuxiliaryFunctionDefinition>(
    *TagIndex, *TotalSize, *PointerToLinenumber, *PointerToNextFunction, *Unused
  );
}

std::string AuxiliaryFunctionDefinition::to_string() const {
  std::ostringstream oss;
  oss << "AuxiliaryFunctionDefinition {\n";
  oss << fmt::format("  Tag index: 0x{:06x}\n", tag_index());
  oss << fmt::format("  Total size: 0x{:06x}\n", total_size());
  oss << fmt::format("  Pointer to line number: 0x{:06x}\n", ptr_to_line_number());
  oss << fmt::format("  Pointer to next function: {}\n", ptr_to_next_func());
  oss << "}\n";
  return oss.str();
}
}
