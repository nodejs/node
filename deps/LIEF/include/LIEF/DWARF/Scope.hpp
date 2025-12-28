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
#ifndef LIEF_DWARF_SCOPE_H
#define LIEF_DWARF_SCOPE_H

#include <cstdint>
#include <memory>
#include <string>

#include "LIEF/visibility.h"

namespace LIEF {
namespace dwarf {

namespace details {
class Scope;
}

/// This class materializes a scope in which Function, Variable, Type, ...
/// can be defined.
class LIEF_API Scope {
  public:
  enum class TYPE : uint32_t {
    UNKNOWN = 0,
    UNION,
    CLASS,
    STRUCT,
    NAMESPACE,
    FUNCTION,
    COMPILATION_UNIT,
  };
  Scope(std::unique_ptr<details::Scope> impl);

  /// Name of the scope. For instance namespace's name or function's name.
  std::string name() const;

  /// Parent scope (if any)
  std::unique_ptr<Scope> parent() const;

  /// The current scope type
  TYPE type() const;

  /// Represent the whole chain of all (parent) scopes using the provided
  /// separator. E.g. `ns1::ns2::Class1::Struct2::Type`
  std::string chained(const std::string& sep = "::") const;

  ~Scope();
  private:
  std::unique_ptr<details::Scope> impl_;
};

}
}
#endif
