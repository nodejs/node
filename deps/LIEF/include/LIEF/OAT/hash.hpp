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
#ifndef LIEF_OAT_HASH_H
#define LIEF_OAT_HASH_H

#include "LIEF/visibility.h"
#include "LIEF/hash.hpp"

namespace LIEF {
class Object;
namespace OAT {

class Binary;
class Class;
class DexFile;
class Header;
class Method;

class LIEF_API Hash : public LIEF::Hash {
  public:
  static LIEF::Hash::value_type hash(const Object& obj);

  public:
  using LIEF::Hash::Hash;
  using LIEF::Hash::visit;

  public:
  void visit(const Binary& binary)        override;
  void visit(const Header& header)        override;
  void visit(const DexFile& dex_file)     override;
  void visit(const Class& cls)            override;
  void visit(const Method& method)        override;

  ~Hash() override;
};

}
}

#endif
