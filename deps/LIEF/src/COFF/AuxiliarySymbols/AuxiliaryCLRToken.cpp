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

#include "LIEF/COFF/AuxiliarySymbols/AuxiliaryCLRToken.hpp"
#include "LIEF/COFF/Symbol.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

#include "LIEF/utils.hpp"

namespace LIEF::COFF {

// typedef struct IMAGE_AUX_SYMBOL_TOKEN_DEF {
//     BYTE  bAuxType;                  // IMAGE_AUX_SYMBOL_TYPE
//     BYTE  bReserved;                 // Must be 0
//     DWORD SymbolTableIndex;
//     BYTE  rgbReserved[12];           // Must be 0
// } IMAGE_AUX_SYMBOL_TOKEN_DEF
std::unique_ptr<AuxiliaryCLRToken>
  AuxiliaryCLRToken::parse(const std::vector<uint8_t>& payload)
{
  SpanStream stream(payload);

  auto bAuxType = stream.read<uint8_t>();
  if (!bAuxType) {
    LIEF_WARN("AuxiliaryCLRToken error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryCLRToken>();
  }

  auto bReserved = stream.read<uint8_t>();
  if (!bReserved) {
    LIEF_WARN("AuxiliaryCLRToken error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryCLRToken>();
  }

  auto SymbolTableIndex = stream.read<uint32_t>();
  if (!SymbolTableIndex) {
    LIEF_WARN("AuxiliaryCLRToken error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryCLRToken>();
  }

  std::vector<uint8_t> rgbReserved;
  if (!stream.read_data(rgbReserved, 12)) {
    LIEF_WARN("AuxiliaryCLRToken error (line: {})", __LINE__);
    return std::make_unique<AuxiliaryCLRToken>();
  }
  return std::make_unique<AuxiliaryCLRToken>(
      *bAuxType, *bReserved, *SymbolTableIndex, std::move(rgbReserved));
}

std::string AuxiliaryCLRToken::to_string() const {
  std::ostringstream oss;
  oss << "AuxiliaryCLRToken {\n";
  oss << fmt::format("  Aux Type: {}\n", aux_type());
  oss << fmt::format("  Reserved: {}\n", reserved());
  oss << fmt::format("  Symbol index: {}\n", symbol_idx());
  if (const Symbol* sym = symbol()) {
    oss << fmt::format("  Symbol: {}\n", sym->name());
  }
  oss << fmt::format("  Rgb reserved:\n{}", indent(dump(rgb_reserved()), 4));
  oss << "}\n";
  return oss.str();
}
}
