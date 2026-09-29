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
#ifndef LIEF_RUNTIME_LINUX_MODULE_H
#define LIEF_RUNTIME_LINUX_MODULE_H

#include "LIEF/runtime/Module.hpp"
#include "LIEF/runtime/utils.hpp"

namespace LIEF {

namespace ELF {
class Binary;
struct ParserConfig;
}

namespace runtime {
namespace Linux {

/// This class exposes a Linux-specific API for a module
class LIEF_API Module : public runtime::Module {
  public:
  using runtime::Module::Module;

  /// Instantiate a Module from the given `dlopen` handle.
  static std::unique_ptr<Module> from_handle(void* H);

  /// Return the dlopen handle for this library.
  ///
  /// Return a nullptr if the function fails or if the handler can't be found
  void* handle() const;

  /// Resolve the symbol with the given name for the current module
  void* dlsym(const std::string& name) const;

  /// Parse the ELF module from its path on the filesystem
  std::unique_ptr<ELF::Binary> parse_from_path() const;

  /// Parse the ELF module from its path on the filesystem and given the parser
  /// configuration
  std::unique_ptr<ELF::Binary>
      parse_from_path(const ELF::ParserConfig& config) const;

  /// Parse the ELF module from memory with the given configuration
  std::unique_ptr<ELF::Binary>
      parse_from_memory(const ELF::ParserConfig& config) const;

  /// Parse the ELF module from memory
  std::unique_ptr<ELF::Binary> parse_from_memory() const;

  ~Module() override = default;

  static constexpr bool classof(const runtime::Module*) {
    return platform() == PLATFORMS::LINUX;
  }
};

/// Load the library with the given path/name
LIEF_API std::unique_ptr<Module> dlopen(const std::string& name);

}
}
}
#endif
