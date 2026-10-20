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
#ifndef LIEF_RUNTIME_PROCESS_H
#define LIEF_RUNTIME_PROCESS_H
#include <cstdint>
#include <string>
#include <unordered_map>

#include "LIEF/visibility.h"
#include "LIEF/runtime/utils.hpp"
#include "LIEF/optional.hpp"

namespace LIEF {
namespace assembly {
class Engine;
}
}

namespace LIEF {
namespace runtime {

/// This structure represents the current process and provides
/// functions to query process-level information.
class LIEF_API Process {
  public:
  /// This structure wraps environment variables
  struct EnvVars {
    std::unordered_map<std::string, std::string> vars;

    bool empty() const {
      return vars.empty();
    }
  };

  /// Get the Process ID of the current process.
  static int32_t pid();

  /// Get the Thread ID of the current thread.
  static uint32_t tid();

  /// Return the target architecture of the current process.
  static constexpr ARCH arch() {
    return runtime::arch();
  }

  /// Return the target platform of the current process.
  static constexpr PLATFORMS platform() {
    return runtime::platform();
  }

  /// Return the number of bytes in a memory page.
  ///
  /// For instance:
  /// * `0x1000` (4096 bytes) for x86_64
  /// * `0x4000` (16384 bytes) for ARM64
  static uint32_t page_size();

  /// Return the environment variable associated with the given key.
  static optional<std::string> get_env(const std::string& key);

  /// Return the environment variables present in the current process
  static EnvVars get_envs();

  /// Return the assembler/disassembler for the current process.
  static assembly::Engine* default_engine();
};
}
}
#endif
