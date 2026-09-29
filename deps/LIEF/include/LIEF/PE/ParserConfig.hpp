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
#ifndef LIEF_PE_PARSER_CONFIG_H
#define LIEF_PE_PARSER_CONFIG_H
#include <string>
#include <ostream>
#include "LIEF/visibility.h"
#include "LIEF/optional.hpp"

namespace LIEF {
namespace PE {

/// This structure is used to configure the behavior of the PE Parser (PE::Parser).
struct LIEF_API ParserConfig {
  /// Returns the default configuration for the PE Parser.
  static const ParserConfig& default_conf() {
    static const ParserConfig DEFAULT;
    return DEFAULT;
  }

  /// Returns a configuration that enables all optional parsing features.
  static ParserConfig all() {
    ParserConfig config;
    config.parse_exceptions = true;
    config.parse_arm64x_binary = true;
    return config;
  }

  /// Whether to parse the PE Authenticode signature.
  bool parse_signature = true;

  /// Whether to parse the PE Export Directory.
  bool parse_exports = true;

  /// Whether to parse the PE Import Directory.
  bool parse_imports = true;

  /// Whether to parse the PE resources tree.
  bool parse_rsrc = true;

  /// Whether to parse PE relocations.
  bool parse_reloc = true;

  /// Whether to parse in-depth exception metadata.
  ///
  /// This option is disabled by default because it can introduce significant
  /// parsing overhead.
  bool parse_exceptions = false;

  /// Whether to parse nested ARM64X binaries.
  ///
  /// This option is disabled by default because it can introduce significant
  /// parsing overhead.
  bool parse_arm64x_binary = false;

  /// If set, this value holds the original image base from which the binary
  /// should be rebased. This is used to *undo* relocations and IAT bindings
  /// when parsing a PE loaded in memory.
  optional<uint64_t> rebase;

  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                           const ParserConfig& config) {
    os << config.to_string();
    return os;
  }
};

}
}
#endif
