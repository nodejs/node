/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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

#include "logging.hpp"

#include "LIEF/BinaryStream/VectorStream.hpp"

#include "LIEF/OAT/Parser.hpp"
#include "LIEF/OAT/Binary.hpp"
#include "LIEF/OAT/utils.hpp"

#include "LIEF/VDEX/utils.hpp"
#include "LIEF/VDEX/Parser.hpp"

#include "OAT/Structures.hpp"

#include "Parser.tcc"

namespace LIEF {
namespace OAT {

Parser::~Parser() = default;
Parser::Parser()  = default;


std::unique_ptr<Binary> Parser::parse(const std::string& oat_file) {
  if (!is_oat(oat_file)) {
    LIEF_ERR("{} is not an OAT", oat_file);
    return nullptr;
  }

  Parser parser{oat_file};
  parser.init();

  std::unique_ptr<Binary> oat_binary{static_cast<Binary*>(parser.binary_.release())};
  return oat_binary;
}


std::unique_ptr<Binary> Parser::parse(const std::string& oat_file, const std::string& vdex_file) {
  if (!is_oat(oat_file)) {
    return nullptr;
  }

  if (!VDEX::is_vdex(vdex_file)) {
    return nullptr;
  }

  Parser parser{oat_file};
  if (std::unique_ptr<VDEX::File> vdex = VDEX::Parser::parse(vdex_file)) {
    parser.vdex_file_ = std::move(vdex);
  } else {
    LIEF_WARN("Can't parse the VDEX file '{}'", vdex_file);
  }
  parser.init();
  std::unique_ptr<Binary> oat_binary{static_cast<Binary*>(parser.binary_.release())};
  return oat_binary;

}

std::unique_ptr<Binary> Parser::parse(std::vector<uint8_t> data) {
  Parser parser{std::move(data)};
  parser.init();
  std::unique_ptr<Binary> oat_binary{static_cast<Binary*>(parser.binary_.release())};
  return oat_binary;
}


Parser::Parser(std::vector<uint8_t> data) {
  stream_    = std::make_unique<VectorStream>(std::move(data));
  binary_    = std::unique_ptr<Binary>(new Binary{});
  config_.count_mtd = ELF::ParserConfig::DYNSYM_COUNT::AUTO;
}

Parser::Parser(const std::string& file) {
  if (auto s = VectorStream::from_file(file)) {
    stream_ = std::make_unique<VectorStream>(std::move(*s));
  }
  binary_    = std::unique_ptr<Binary>(new Binary{});
  config_.count_mtd = ELF::ParserConfig::DYNSYM_COUNT::AUTO;
}


bool Parser::has_vdex() const {
  return vdex_file_ != nullptr;
}

void Parser::set_vdex(std::unique_ptr<VDEX::File> file) {
  vdex_file_ = std::move(file);
}


void Parser::init() {
  ELF::Parser::init();
  oat_version_t version = OAT::version(oat_binary());
  Binary& oat_bin = oat_binary();
  oat_bin.vdex_ = std::move(vdex_file_);

  if (!oat_bin.has_vdex() && version > details::OAT_088::oat_version) {
    LIEF_INFO("No VDEX provided with this OAT file. Parsing will be incomplete");
  }

  if (version <= details::OAT_064::oat_version) {
    return parse_binary<details::OAT64_t>();
  }

  if (version <= details::OAT_079::oat_version) {
    return parse_binary<details::OAT79_t>();
  }

  if (version <= details::OAT_088::oat_version) {
    return parse_binary<details::OAT88_t>();
  }

  if (version <= details::OAT_124::oat_version) {
    return parse_binary<details::OAT124_t>();
  }

  if (version <= details::OAT_131::oat_version) {
    return parse_binary<details::OAT131_t>();
  }

  if (version <= details::OAT_138::oat_version) {
    return parse_binary<details::OAT138_t>();
  }

}

} // namespace OAT
} // namespace LIEF
