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
#ifndef LIEF_DWARF_EDITOR_TYPE_H
#define LIEF_DWARF_EDITOR_TYPE_H
#include <memory>
#include <cassert>

#include "LIEF/visibility.h"

namespace LIEF {
namespace dwarf {
namespace editor {
class PointerType;

namespace details {
class Type;
}

/// This class is the base class for any types created when editing DWARF debug
/// info.
///
/// A type is owned by a LIEF::dwarf::editor::CompilationUnit and should be
/// created from this class.
class LIEF_API Type {
  public:
  Type() = delete;
  Type(std::unique_ptr<details::Type> impl);

  /// Create a pointer type pointing to this type
  std::unique_ptr<PointerType> pointer_to() const;

  virtual ~Type();

  /// \private
  static std::unique_ptr<Type> create(std::unique_ptr<details::Type> impl);

  /// \private
  details::Type& impl() {
    assert(impl_ != nullptr);
    return *impl_;
  }

  const details::Type& impl() const {
    assert(impl_ != nullptr);
    return *impl_;
  }

  protected:
  std::unique_ptr<details::Type> impl_;
};

}
}
}
#endif
