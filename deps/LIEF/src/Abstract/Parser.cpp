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

#include <fstream>

#include "logging.hpp"
#include "LIEF/Abstract/Parser.hpp"
#include "LIEF/Abstract/Binary.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"


#if defined(LIEF_OAT_SUPPORT)
#include "LIEF/OAT/Binary.hpp"
#include "LIEF/OAT/Parser.hpp"
#include "LIEF/OAT/utils.hpp"
#endif

#if defined(LIEF_ELF_SUPPORT)
#include "LIEF/ELF/utils.hpp"
#include "LIEF/ELF/Parser.hpp"
#include "LIEF/ELF/Binary.hpp"
#endif

#if defined(LIEF_PE_SUPPORT)
#include "LIEF/PE/utils.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#endif

#if defined(LIEF_MACHO_SUPPORT)
#include "LIEF/MachO/utils.hpp"
#include "LIEF/MachO/Parser.hpp"
#include "LIEF/MachO/FatBinary.hpp"
#include "LIEF/MachO/Binary.hpp"
#endif



namespace LIEF {
Parser::~Parser() = default;
Parser::Parser() = default;

std::unique_ptr<Binary> Parser::parse(const std::string& filename) {

#if defined(LIEF_OAT_SUPPORT)
  if (OAT::is_oat(filename)) {
    return OAT::Parser::parse(filename);
  }
#endif

#if defined(LIEF_ELF_SUPPORT)
  if (ELF::is_elf(filename)) {
    return ELF::Parser::parse(filename);
  }
#endif


#if defined(LIEF_PE_SUPPORT)
  if (PE::is_pe(filename)) {
     return PE::Parser::parse(filename);
  }
#endif

#if defined(LIEF_MACHO_SUPPORT)
  if (MachO::is_macho(filename)) {
    // For fat binary we take the last one...
    std::unique_ptr<MachO::FatBinary> fat = MachO::Parser::parse(filename);
    if (fat != nullptr) {
      return fat->pop_back();
    }
    return nullptr;
  }
#endif

  LIEF_ERR("Unknown format");
  return nullptr;
}

std::unique_ptr<Binary> Parser::parse(const std::vector<uint8_t>& raw) {

#if defined(LIEF_OAT_SUPPORT)
  if (OAT::is_oat(raw)) {
    return OAT::Parser::parse(raw);
  }
#endif

#if defined(LIEF_ELF_SUPPORT)
  if (ELF::is_elf(raw)) {
    return ELF::Parser::parse(raw);
  }
#endif


#if defined(LIEF_PE_SUPPORT)
  if (PE::is_pe(raw)) {
     return PE::Parser::parse(raw);
  }
#endif

#if defined(LIEF_MACHO_SUPPORT)
  if (MachO::is_macho(raw)) {
    // For fat binary we take the last one...
    std::unique_ptr<MachO::FatBinary> fat = MachO::Parser::parse(raw);
    if (fat != nullptr) {
      return fat->pop_back();
    }
    return nullptr;
  }
#endif

  LIEF_ERR("Unknown format");
  return nullptr;

}

std::unique_ptr<Binary> Parser::parse(std::unique_ptr<BinaryStream> stream) {

#if defined(LIEF_ELF_SUPPORT)
  if (ELF::is_elf(*stream)) {
    return ELF::Parser::parse(std::move(stream));
  }
#endif


#if defined(LIEF_PE_SUPPORT)
  if (PE::is_pe(*stream)) {
     return PE::Parser::parse(std::move(stream));
  }
#endif

#if defined(LIEF_MACHO_SUPPORT)
  if (MachO::is_macho(*stream)) {
    // For fat binary we take the last one...
    std::unique_ptr<MachO::FatBinary> fat = MachO::Parser::parse(std::move(stream));
    if (fat != nullptr) {
      return fat->pop_back();
    }
    return nullptr;
  }
#endif

  LIEF_ERR("Unknown format");
  return nullptr;
}

Parser::Parser(const std::string& filename) {
  std::ifstream file(filename, std::ios::in | std::ios::binary);

  if (!file) {
    LIEF_ERR("Can't open '{}'", filename);
    return;
  }
  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  binary_size_ = static_cast<uint64_t>(file.tellg());
  file.seekg(0, std::ios::beg);
}

}
