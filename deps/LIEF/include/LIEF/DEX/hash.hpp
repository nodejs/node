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
#ifndef LIEF_DEX_HASH_H
#define LIEF_DEX_HASH_H

#include "LIEF/visibility.h"
#include "LIEF/hash.hpp"

namespace LIEF {
class Object;

namespace DEX {

class Class;
class Field;
class File;
class Header;
class MapItem;
class MapList;
class Method;
class Prototype;
class CodeInfo;
class Type;

/// Class which implements a visitor to compute
/// a **deterministic** hash for LIEF DEX objects
class LIEF_API Hash : public LIEF::Hash {
  public:
  static LIEF::Hash::value_type hash(const Object& obj);

  public:
  using LIEF::Hash::Hash;
  using LIEF::Hash::visit;

  public:
  void visit(const File& file)          override;
  void visit(const Header& header)      override;
  void visit(const Class& cls)          override;
  void visit(const Field& field)        override;
  void visit(const Method& method)      override;
  void visit(const CodeInfo& code_info) override;
  void visit(const Type& type)          override;
  void visit(const Prototype& type)     override;
  void visit(const MapItem& item)       override;
  void visit(const MapList& list)       override;

  ~Hash() override;
};

}
}

#endif
