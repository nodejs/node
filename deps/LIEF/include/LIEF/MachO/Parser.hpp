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
#ifndef LIEF_MACHO_PARSER_H
#define LIEF_MACHO_PARSER_H
#include <string>
#include <vector>
#include <memory>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"

#include "LIEF/Abstract/Parser.hpp"

#include "LIEF/MachO/ParserConfig.hpp"

namespace LIEF {
class BinaryStream;

namespace MachO {
class Binary;
class FatBinary;

/// The main interface to parse a Mach-O binary.
///
/// This class is used to parse both Fat & non-Fat binary.
/// Non-fat binaries are considerated as a **fat** with
/// only one architecture. This is why MachO::Parser::parse outputs
/// a FatBinary object.
class LIEF_API Parser : public LIEF::Parser {
  public:
  Parser& operator=(const Parser& copy) = delete;
  Parser(const Parser& copy)            = delete;

  ~Parser() override;

  /// Parse a Mach-O file from the path provided by the ``filename``
  /// parameter
  ///
  /// The @p conf parameter can be used to tweak the configuration
  /// of the parser
  ///
  /// @param[in] filename   Path to the Mach-O file
  /// @param[in] conf       Parser configuration (Defaut: ParserConfig::deep)
  static std::unique_ptr<FatBinary> parse(const std::string& filename,
                                          const ParserConfig& conf = ParserConfig::deep());

  /// Parse a Mach-O file from the raw content provided by the ``data``
  /// parameter
  ///
  /// The @p conf parameter can be used to tweak the configuration
  /// of the parser
  ///
  /// @param[in] data       Mach-O file as a vector of bytes
  /// @param[in] conf       Parser configuration (Defaut: ParserConfig::deep)
  static std::unique_ptr<FatBinary> parse(const std::vector<uint8_t>& data,
                                          const ParserConfig& conf = ParserConfig::deep());


  /// Parser a Mach-O binary from the provided BinaryStream.
  static std::unique_ptr<FatBinary> parse(std::unique_ptr<BinaryStream> stream,
                                          const ParserConfig& conf = ParserConfig::deep());

  /// Parse the Mach-O binary from the address given in the first parameter
  static std::unique_ptr<FatBinary> parse_from_memory(uintptr_t address,
                                                      const ParserConfig& conf = ParserConfig::deep());

  /// Parse the Mach-O binary from the address given in the first parameter
  /// and the size given in the second parameter
  static std::unique_ptr<FatBinary> parse_from_memory(uintptr_t address, size_t size,
                                                      const ParserConfig& conf = ParserConfig::deep());

  private:
  LIEF_LOCAL Parser(const std::string& file, const ParserConfig& conf);
  LIEF_LOCAL Parser(std::vector<uint8_t> data, const ParserConfig& conf);
  LIEF_LOCAL Parser();

  LIEF_LOCAL ok_error_t build();
  LIEF_LOCAL ok_error_t build_fat();

  LIEF_LOCAL ok_error_t undo_reloc_bindings(uintptr_t base_address);

  std::unique_ptr<BinaryStream> stream_;
  std::vector<std::unique_ptr<Binary>> binaries_;
  ParserConfig config_;
};
}
}
#endif
