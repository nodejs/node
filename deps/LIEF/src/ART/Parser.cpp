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

#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/ART/Parser.hpp"
#include "LIEF/ART/utils.hpp"
#include "LIEF/ART/File.hpp"

#include "ART/Structures.hpp"

#include "Header.tcc"
#include "Parser.tcc"

namespace LIEF {
namespace ART {

Parser::~Parser() = default;
Parser::Parser()  = default;

std::unique_ptr<File> Parser::parse(const std::string& filename) {
  if (!is_art(filename)) {
    LIEF_ERR("'{}' is not an ART file", filename);
    return nullptr;
  }

  const art_version_t version = ART::version(filename);
  Parser parser{filename};
  parser.init(filename, version);
  return std::move(parser.file_);
}

std::unique_ptr<File> Parser::parse(std::vector<uint8_t> data, const std::string& name) {
  if (!is_art(data)) {
    LIEF_ERR("'{}' is not an ART file", name);
    return nullptr;
  }

  art_version_t version = ART::version(data);
  Parser parser{std::move(data)};
  parser.init(name, version);
  return std::move(parser.file_);
}


Parser::Parser(std::vector<uint8_t> data) :
  file_{new File{}},
  stream_{std::make_unique<VectorStream>(std::move(data))}
{
}

Parser::Parser(const std::string& file) :
  file_{new File{}}
{
  auto stream = VectorStream::from_file(file);
  if (!stream) {
    LIEF_ERR("Can't create the stream");
    return;
  }
  stream_ = std::make_unique<VectorStream>(std::move(*stream));
}


void Parser::init(const std::string& /*name*/, art_version_t version) {

  if (version <= details::ART_17::art_version) {
    return parse_file<details::ART17>();
  }

  if (version <= details::ART_29::art_version) {
    return parse_file<details::ART29>();
  }

  if (version <= details::ART_30::art_version) {
    return parse_file<details::ART30>();
  }

  if (version <= details::ART_44::art_version) {
    return parse_file<details::ART44>();
  }

  if (version <= details::ART_46::art_version) {
    return parse_file<details::ART46>();
  }

  if (version <= details::ART_56::art_version) {
    return parse_file<details::ART56>();
  }
}

} // namespace ART
} // namespace LIEF
