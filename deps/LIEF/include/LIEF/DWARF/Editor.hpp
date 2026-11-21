/* Copyright 2022 - 2026 R. Thomas
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
#ifndef LIEF_DWARF_EDITOR_H
#define LIEF_DWARF_EDITOR_H
#include <memory>
#include <string>
#include <cstdint>

#include "LIEF/compiler_attributes.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class Binary;
namespace dwarf {

namespace details {
class Editor;
}

namespace editor {
class CompilationUnit;
}

/// This class exposes the main API to create DWARF information
class LIEF_API Editor {
  public:
  Editor() = delete;
  Editor(std::unique_ptr<details::Editor> impl);

  enum class FORMAT : uint32_t {
    ELF,
    MACHO,
    PE,
  };

  enum class ARCH : uint32_t {
    UNKNOWN,
    X64,
    X86,
    AARCH64,
    ARM,
  };

  /// Instantiate an editor for the given binary object
  static std::unique_ptr<Editor> from_binary(LIEF::Binary& bin);

  /// Instantiate an editor for the given format and arch
  static std::unique_ptr<Editor> create(FORMAT fmt, ARCH arch);

  /// Create a new compilation unit
  std::unique_ptr<editor::CompilationUnit>
      create_compilation_unit() LIEF_LIFETIMEBOUND;

  /// Write the DWARF file to the specified output
  void write(const std::string& output);

  ~Editor();

  private:
  std::unique_ptr<details::Editor> impl_;
};

}
}
#endif
