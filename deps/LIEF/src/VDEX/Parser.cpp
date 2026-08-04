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

#include <memory>

#include "logging.hpp"

#include "LIEF/VDEX/Parser.hpp"
#include "LIEF/VDEX/File.hpp"
#include "LIEF/VDEX/utils.hpp"

#include "LIEF/BinaryStream/VectorStream.hpp"

#include "VDEX/Structures.hpp"

#include "Header.tcc"
#include "Parser.tcc"

namespace LIEF {
namespace VDEX {

Parser::~Parser() = default;
Parser::Parser()  = default;

std::unique_ptr<File> Parser::parse(const std::string& filename) {
  Parser parser{filename};
  return std::unique_ptr<File>{parser.file_};
}

std::unique_ptr<File> Parser::parse(const std::vector<uint8_t>& data, const std::string& name) {
  Parser parser{data, name};
  return std::unique_ptr<File>{parser.file_};
}


Parser::Parser(const std::vector<uint8_t>& data, const std::string& name) :
  file_{new File{}},
  stream_{std::make_unique<VectorStream>(data)}
{
  if (!is_vdex(data)) {
    LIEF_ERR("{} is not a VDEX file!", name);
    delete file_;
    file_ = nullptr;
    return;
  }

  vdex_version_t version = VDEX::version(data);
  init(name, version);
}

Parser::Parser(const std::string& file) :
  file_{new File{}}
{
  if (!is_vdex(file)) {
    LIEF_ERR("{} is not a VDEX file!", file);
    delete file_;
    file_ = nullptr;
    return;
  }

  if (auto s = VectorStream::from_file(file)) {
    stream_ = std::make_unique<VectorStream>(std::move(*s));
  }

  vdex_version_t version = VDEX::version(file);
  init(file, version);
}


void Parser::init(const std::string& /*name*/, vdex_version_t version) {
  LIEF_DEBUG("VDEX version: {:d}", version);

  if (version <= details::VDEX_6::vdex_version) {
    return parse_file<details::VDEX6>();
  }

  if (version <= details::VDEX_10::vdex_version) {
    return parse_file<details::VDEX10>();
  }

  if (version <= details::VDEX_11::vdex_version) {
    return parse_file<details::VDEX11>();
  }
}

} // namespace VDEX
} // namespace LIEF
