/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
/// Non-fat binaries are considered as a **fat** with
/// only one architecture. This is why MachO::Parser::parse outputs
/// a FatBinary object.
class LIEF_API Parser : public LIEF::Parser {
  public:
  Parser& operator=(const Parser& copy) = delete;
  Parser(const Parser& copy) = delete;

  ~Parser() override;

  /// Parse a Mach-O file from the path provided by the ``filename``
  /// parameter
  ///
  /// The @p conf parameter can be used to tweak the configuration
  /// of the parser
  ///
  /// @param[in] filename   Path to the Mach-O file
  /// @param[in] conf       Parser configuration (Default: ParserConfig::deep)
  static std::unique_ptr<FatBinary>
      parse(const std::string& filename,
            const ParserConfig& conf = ParserConfig::deep());

  /// Parse a Mach-O file from the raw content provided by the ``data``
  /// parameter
  ///
  /// The @p conf parameter can be used to tweak the configuration
  /// of the parser
  ///
  /// @param[in] data       Mach-O file as a vector of bytes
  /// @param[in] conf       Parser configuration (Default: ParserConfig::deep)
  static std::unique_ptr<FatBinary>
      parse(const std::vector<uint8_t>& data,
            const ParserConfig& conf = ParserConfig::deep());


  /// Parse a Mach-O binary from the provided BinaryStream.
  static std::unique_ptr<FatBinary>
      parse(std::unique_ptr<BinaryStream> stream,
            const ParserConfig& conf = ParserConfig::deep());

  /// Parse the Mach-O binary from the address given in the first parameter
  static std::unique_ptr<FatBinary>
      parse_from_memory(uintptr_t address,
                        const ParserConfig& conf = ParserConfig::deep());

  /// Parse the Mach-O binary from the address given in the first parameter
  /// and the size given in the second parameter
  static std::unique_ptr<FatBinary>
      parse_from_memory(uintptr_t address, size_t size,
                        const ParserConfig& conf = ParserConfig::deep());

  /// Parse a Mach-O binary from a memory dump located on disk.
  ///
  /// A dump is a raw capture of the process memory that was mapped starting at
  /// the virtual address `addr`. This is typically used to parse a Mach-O image
  /// that has been dumped from memory (e.g. from a debugger or a runtime hook).
  ///
  /// @param[in] filepath Path to the file that contains the memory dump
  /// @param[in] addr     Virtual address at which the dump was mapped
  /// @param[in] conf     Optional configuration for the parser
  static std::unique_ptr<FatBinary>
      parse_from_dump(const std::string& filepath, uint64_t addr,
                      const ParserConfig& conf = ParserConfig::deep());

  /// Same as parse_from_dump(const std::string&, uint64_t, const ParserConfig&)
  /// but the dump is wrapped in the given **non-owned** stream.
  static std::unique_ptr<FatBinary>
      parse_from_dump(BinaryStream& stream, uint64_t addr,
                      const ParserConfig& conf = ParserConfig::deep());

  /// Same as parse_from_dump(const std::string&, uint64_t, const ParserConfig&)
  /// but the dump is wrapped in the given **owned** stream.
  static std::unique_ptr<FatBinary>
      parse_from_dump(std::unique_ptr<BinaryStream> stream, uint64_t addr,
                      const ParserConfig& conf = ParserConfig::deep());

  private:
  LIEF_LOCAL Parser(const std::string& file, const ParserConfig& conf);
  LIEF_LOCAL Parser(std::vector<uint8_t> data, const ParserConfig& conf);
  LIEF_LOCAL Parser();

  LIEF_LOCAL ok_error_t parse();
  LIEF_LOCAL ok_error_t parse_fat();

  LIEF_LOCAL ok_error_t undo_reloc_bindings(uintptr_t base_address);
  LIEF_LOCAL ok_error_t unpack_tlv();

  std::unique_ptr<BinaryStream> stream_;
  std::vector<std::unique_ptr<Binary>> binaries_;
  ParserConfig config_;
};
}
}
#endif
