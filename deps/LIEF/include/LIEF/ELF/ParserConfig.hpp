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
#ifndef LIEF_ELF_PARSER_CONFIG_H
#define LIEF_ELF_PARSER_CONFIG_H
#include "LIEF/visibility.h"

#include <cstdint>

namespace LIEF {
namespace ELF {

/// This structure is used to tweak the ELF Parser (ELF::Parser)
struct LIEF_API ParserConfig {
  /// Methods that can be used by the LIEF::ELF::Parser to count the number
  /// of dynamic symbols
  enum class DYNSYM_COUNT {
    /// Automatic detection
    AUTO = 0,

    /// Count based on sections (not very reliable)
    SECTION,

    /// Count based on hash table (reliable)
    HASH,

    /// Count based on PLT/GOT relocations (very reliable but not accurate)
    RELOCATIONS,
  };

  /// This returns a ParserConfig object configured to process all the ELF
  /// elements.
  static ParserConfig all() {
    static const ParserConfig DEFAULT;
    return DEFAULT;
  }

  /// Whether relocations (including plt-like relocations) should be parsed.
  bool parse_relocations = true;

  /// Whether dynamic symbols (those from `.dynsym`) should be parsed
  bool parse_dyn_symbols = true;

  /// Whether debug symbols (those from `.symtab`) should be parsed
  bool parse_symtab_symbols = true;

  /// Whether versioning symbols should be parsed
  bool parse_symbol_versions = true;

  /// Whether ELF notes  information should be parsed
  bool parse_notes = true;

  /// Whether the overlay data should be parsed
  bool parse_overlay = true;

  /// The method used to count the number of dynamic symbols
  DYNSYM_COUNT count_mtd = DYNSYM_COUNT::AUTO;

  /// Memory page size if the binary uses a non-standard value.
  ///
  /// For instance, SPARCV9 binary can use page size from 0x2000 to 0x100000.
  uint64_t page_size = 0;
};

}
}
#endif
