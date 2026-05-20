/* Copyright 2022 - 2025 R. Thomas
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
#ifndef LIEF_ASM_ASSEMBLER_CONFIG_H
#define LIEF_ASM_ASSEMBLER_CONFIG_H

#include "LIEF/visibility.h"
#include "LIEF/optional.hpp"

#include <string>

namespace LIEF {

namespace assembly {

/// This class exposes the different elements that can be configured to assemble
/// code.
class LIEF_API AssemblerConfig {
  public:
  AssemblerConfig() = default;

  AssemblerConfig(const AssemblerConfig &) = default;
  AssemblerConfig &operator=(const AssemblerConfig &) = default;

  AssemblerConfig(AssemblerConfig &&) = default;
  AssemblerConfig &operator=(AssemblerConfig &&) = default;

  /// The different supported dialects
  enum class DIALECT {
    DEFAULT_DIALECT = 0,
    /// Intel syntax
    X86_INTEL,

    /// AT&T syntax
    X86_ATT,
  };

  /// Default configuration
  static AssemblerConfig& default_config() {
    static AssemblerConfig AC;
    return AC;
  }

  /// The dialect of the input assembly code
  DIALECT dialect = DIALECT::DEFAULT_DIALECT;

  /// This function aims to be overloaded in order to resolve symbols used
  /// in the assembly listing.
  ///
  /// For instance, given this assembly code:
  ///
  /// ```text
  /// 0x1000: mov rdi, rbx
  /// 0x1003: call _my_function
  /// ```
  ///
  /// The function `_my_function` will remain undefined unless we return its address
  /// in `resolve_symbol()`:
  ///
  /// ```cpp
  /// class MyConfig : public AssemblerConfig {
  ///   public:
  ///   optional<uint64_t> resolve_symbol(const std::string& name) {
  ///     if (name == "_my_function") {
  ///       return 0x4000;
  ///     }
  ///     return nullopt(); // or AssemblerConfig::resolve_symbol(name)
  ///   }
  /// };
  /// ```
  virtual optional<uint64_t> resolve_symbol(const std::string& /*name*/) {
    return nullopt();
  }

  virtual ~AssemblerConfig() = default;
};
}
}

#endif
