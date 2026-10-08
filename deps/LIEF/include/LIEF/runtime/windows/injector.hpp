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
#ifndef LIEF_RUNTIME_WIN_INJECTOR_H
#define LIEF_RUNTIME_WIN_INJECTOR_H

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

#include <ostream>
#include <string>
#include <unordered_map>

namespace LIEF {
namespace runtime {
namespace windows {

/// Describes how to spawn a new process and inject a library into it.
struct LIEF_API injection_context_t {
  /// Absolute path to the target executable to spawn.
  std::string target_path;

  /// Command-line arguments passed to the spawned process.
  std::string args;

  /// Absolute path to the library (DLL) that should be injected.
  std::string library;

  /// Environment variables to set in the spawned process. If left empty,
  /// the current process environment is inherited.
  std::unordered_map<std::string, std::string> env;

  /// Check whether the context is consistent (required paths filled-in and
  /// readable).
  bool validate() const;

  operator bool() const {
    return validate();
  }

  std::string to_string() const;

  friend LIEF_API std::ostream& operator<<(std::ostream& os,
                                           const injection_context_t& ctx) {
    os << ctx.to_string();
    return os;
  }
};

/// Spawn the target described by the given injection context and inject the
/// associated library before the main thread starts executing.
///
/// This is the Windows equivalent of a "create suspended + remote LoadLibrary"
/// approach.
LIEF_API ok_error_t inject_spawn(const injection_context_t& ctx);

}
}
}
#endif
