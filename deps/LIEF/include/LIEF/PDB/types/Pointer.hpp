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
#ifndef LIEF_PDB_TYPE_POINTER_H
#define LIEF_PDB_TYPE_POINTER_H

#include "LIEF/visibility.h"
#include "LIEF/PDB/Type.hpp"

namespace LIEF {
namespace pdb {
namespace types {

/// This class represents a `LF_POINTER` PDB type
class LIEF_API Pointer : public Type {
  public:
  using Type::Type;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::POINTER;
  }

  /// The underlying type pointed by this pointer
  std::unique_ptr<Type> underlying_type() const;

  ~Pointer() override;
};

}
}
}
#endif


