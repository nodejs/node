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
#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryWeakExternal.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "logging.hpp"

#include <sstream>
namespace LIEF::COFF {

std::unique_ptr<AuxiliaryWeakExternal>
  AuxiliaryWeakExternal::parse(const std::vector<uint8_t>& payload)
{
  SpanStream stream(payload);

  auto TagIndex = stream.read<uint32_t>();
  if (!TagIndex) {
    LIEF_WARN("AuxiliaryWeakExternal error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryWeakExternal>();
  }

  auto Characteristics = stream.read<uint32_t>();
  if (!Characteristics) {
    LIEF_WARN("AuxiliaryWeakExternal error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryWeakExternal>();
  }

  std::vector<uint8_t> padding;
  if (!stream.read_data(padding, 10)) {
    LIEF_WARN("AuxiliaryWeakExternal error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryWeakExternal>();
  }

  return std::make_unique<AuxiliaryWeakExternal>(
    *TagIndex, *Characteristics, std::move(padding)
  );
}

const char* to_string(AuxiliaryWeakExternal::CHARACTERISTICS e) {
  switch (e) {
    default:
      return "UNKNOWN";
    case AuxiliaryWeakExternal::CHARACTERISTICS::SEARCH_NOLIBRARY:
      return "SEARCH_NOLIBRARY";
    case AuxiliaryWeakExternal::CHARACTERISTICS::SEARCH_LIBRARY:
      return "SEARCH_LIBRARY";
    case AuxiliaryWeakExternal::CHARACTERISTICS::SEARCH_ALIAS:
      return "SEARCH_ALIAS";
    case AuxiliaryWeakExternal::CHARACTERISTICS::ANTI_DEPENDENCY:
      return "ANTI_DEPENDENCY";
  }
  return "UNKNOWN";
}

std::string AuxiliaryWeakExternal::to_string() const {
  std::ostringstream oss;
  oss << "AuxiliaryWeakExternal {\n";
  oss << fmt::format("  Symbol index: {}\n", sym_idx());
  oss << fmt::format("  Characteristics: {} ({})\n", LIEF::COFF::to_string(characteristics()),
                     (int)characteristics());
  oss << "}\n";
  return oss.str();
}

}
