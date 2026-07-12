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
#ifndef LIEF_ELF_PARSER_CONFIG_H
#define LIEF_ELF_PARSER_CONFIG_H
#include "LIEF/visibility.h"
#include "LIEF/ELF/enums.hpp"

namespace LIEF {
namespace ELF {

/// This structure is used to tweak the ELF Parser (ELF::Parser)
struct LIEF_API ParserConfig {
  /** Methods that can be used by the LIEF::ELF::Parser
      to count the number of dynamic symbols */
  enum class DYNSYM_COUNT {
    AUTO        = 0, /**< Automatic detection */
    SECTION     = 1, /**< Count based on sections (not very reliable) */
    HASH        = 2, /**< Count based on hash table (reliable) */
    RELOCATIONS = 3, /**< Count based on PLT/GOT relocations (very reliable but not accurate) */
  };

  /// This returns a ParserConfig object configured to process all the ELF
  /// elements.
  static ParserConfig all() {
    static const ParserConfig DEFAULT;
    return DEFAULT;
  }

  bool parse_relocations     = true; ///< Whether relocations (including plt-like relocations) should be parsed.
  bool parse_dyn_symbols     = true; ///< Whether dynamic symbols (those from `.dynsym`) should be parsed
  bool parse_symtab_symbols  = true; ///< Whether debug symbols (those from `.symtab`) should be parsed
  bool parse_symbol_versions = true; ///< Whether versioning symbols should be parsed
  bool parse_notes           = true; ///< Whether ELF notes  information should be parsed
  bool parse_overlay         = true; ///< Whether the overlay data should be parsed

  /** The method used to count the number of dynamic symbols */
  DYNSYM_COUNT count_mtd = DYNSYM_COUNT::AUTO;

  /// Memory page size if the binary uses a non-standard value.
  ///
  /// For instance, SPARCV9 binary can use page size from 0x2000 to 0x100000.
  uint64_t page_size = 0;
};

}
}
#endif
