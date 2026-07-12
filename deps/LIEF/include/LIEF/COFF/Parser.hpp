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
#ifndef LIEF_COFF_PARSER_H
#define LIEF_COFF_PARSER_H
#include <map>
#include "LIEF/visibility.h"

#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/COFF/ParserConfig.hpp"
#include "LIEF/COFF/Header.hpp"

namespace LIEF {
namespace COFF {
class Binary;
class Section;
class String;
class Symbol;

class Parser {
  public:
  /// Parse the COFF binary referenced by the `stream` argument with the
  /// given config
  static LIEF_API
    std::unique_ptr<Binary> parse(std::unique_ptr<BinaryStream> stream,
                                  const ParserConfig& config = ParserConfig::default_conf());

  /// Parse the COFF binary pointed by the `file` argument with the given config
  static std::unique_ptr<Binary> parse(const std::string& file,
                                       const ParserConfig& config = ParserConfig::default_conf())
  {
    if (auto strm = VectorStream::from_file(file)) {
      return parse(std::unique_ptr<VectorStream>(new VectorStream(std::move(*strm))), config);
    }
    return nullptr;
  }

  /// \private
  struct SymSec {
    size_t sec_idx = 0;
    Symbol* symbol = nullptr;

    friend bool operator<(const SymSec& lhs, const SymSec& rhs) {
      return lhs.sec_idx < rhs.sec_idx;
    }

    friend bool operator==(const SymSec& lhs, const SymSec& rhs) {
      return lhs.sec_idx == rhs.sec_idx && lhs.symbol == rhs.symbol;
    }

    friend bool operator!=(const SymSec& lhs, const SymSec& rhs) {
      return !(lhs == rhs);
    }
  };

  /// <=> std::unordered_multimap<section index, Symbol*>
  using SymSecMap = std::vector<SymSec>;

  /// \private
  LIEF_LOCAL void memoize(String str);

  /// \private
  LIEF_LOCAL String* find_coff_string(uint32_t offset) const;

  ~Parser();

  private:
  Parser(std::unique_ptr<BinaryStream> stream, const ParserConfig& config,
         Header::KIND kind) :
    stream_(std::move(stream)),
    kind_(kind),
    config_(config)
  {}

  ok_error_t process();
  ok_error_t parse_header();
  ok_error_t parse_optional_header();
  ok_error_t parse_sections();
  ok_error_t parse_relocations(Section& section);
  ok_error_t parse_symbols();
  ok_error_t parse_string_table();

  std::unique_ptr<BinaryStream> stream_;
  std::unique_ptr<Binary> bin_;
  Header::KIND kind_ = Header::KIND::UNKNOWN;

  std::map<uint32_t, size_t> memoize_coff_str_;
  std::map<size_t, Symbol*> symbol_idx_;
  SymSecMap symsec_;

  ParserConfig config_;
};
}
}
#endif
