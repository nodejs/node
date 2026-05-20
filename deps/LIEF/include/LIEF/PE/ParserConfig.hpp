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
#ifndef LIEF_PE_PARSER_CONFIG_H
#define LIEF_PE_PARSER_CONFIG_H
#include <string>
#include <ostream>
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

/// This structure is used to tweak the PE Parser (PE::Parser)
struct LIEF_API ParserConfig {
  static const ParserConfig& default_conf() {
    static const ParserConfig DEFAULT;
    return DEFAULT;
  }

  static ParserConfig all() {
    ParserConfig config;
    config.parse_exceptions = true;
    config.parse_arm64x_binary = true;
    return config;
  }

  /// Parse PE Authenticode signature
  bool parse_signature = true;

  /// Parse PE Exports Directory
  bool parse_exports = true;

  /// Parse PE Import Directory
  bool parse_imports = true;

  /// Parse PE resources tree
  bool parse_rsrc = true;

  /// Parse PE relocations
  bool parse_reloc = true;

  /// Whether it should parse in-depth exceptions metadata.
  ///
  /// This option is set to off by default since it can introduce a certain
  /// overhead.
  bool parse_exceptions = false;

  /// Whether it should parse nested ARM64X binary
  ///
  /// This option is set to off by default since it can introduce a certain
  /// overhead.
  bool parse_arm64x_binary = false;

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const ParserConfig& config)
  {
    os << config.to_string();
    return os;
  }
};

}
}
#endif
