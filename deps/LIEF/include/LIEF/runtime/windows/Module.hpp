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
#ifndef LIEF_RUNTIME_WIN_MODULE_H
#define LIEF_RUNTIME_WIN_MODULE_H

#include "LIEF/runtime/Module.hpp"
#include "LIEF/runtime/utils.hpp"

namespace LIEF {

namespace PE {
class Binary;
struct ParserConfig;
}

namespace runtime {
namespace windows {

/// This class exposes a Windows-specific API for a module
class LIEF_API Module : public runtime::Module {
  public:
  using runtime::Module::Module;

  static std::unique_ptr<Module> from_handle(void* H);

  /// Return the `HMODULE` handle as an opaque pointer.
  ///
  /// Return a nullptr if the function fails or if the handler can't be found
  void* handle() const;

  /// Resolve the symbol with the given name for the current module
  void* dlsym(const std::string& name) const;

  /// Parse the PE module from its path on the filesystem
  std::unique_ptr<PE::Binary> parse_from_path() const;

  /// Parse the PE module from its path on the filesystem and given the parser
  /// configuration
  std::unique_ptr<PE::Binary>
      parse_from_path(const PE::ParserConfig& config) const;

  /// Parse the PE module from memory with the given configuration
  std::unique_ptr<PE::Binary>
      parse_from_memory(const PE::ParserConfig& config) const;

  /// Parse the PE module from memory
  std::unique_ptr<PE::Binary> parse_from_memory() const;

  ~Module() override = default;

  static constexpr bool classof(const runtime::Module*) {
    return platform() == PLATFORMS::WINDOWS;
  }
};

/// Load the windows library with the given path or name.
LIEF_API std::unique_ptr<Module> dlopen(const std::string& name);

/// Try to get the Module with the given name.
///
/// Return a nullptr if the module is not found
///
///
/// ```cpp
/// if (auto ntdll = find_module("ntdll.dll")) {
///   std::cout << ntdll->path() << '\n';
/// }
/// ```
/// \note This function relies on the Windows API `GetModuleHandle`
///       which is more efficient than the generic implementation
///       LIEF::runtime::module_from_name
LIEF_API std::unique_ptr<Module> find_module(const std::string& name);

}
}
}
#endif
