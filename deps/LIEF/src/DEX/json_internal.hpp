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
#ifndef LIEF_DEX_JSON_INTERNAL_H_
#define LIEF_DEX_JSON_INTERNAL_H_

#include "LIEF/visibility.h"
#include "visitors/json.hpp"

namespace LIEF {
namespace DEX {

json to_json_obj(const Object& v);

/// Class that implements the Visitor pattern to output
/// a JSON representation of an ELF object
class JsonVisitor : public LIEF::JsonVisitor {
  public:
  using LIEF::JsonVisitor::JsonVisitor;

  public:
  void visit(const File& file)         override;
  void visit(const Header& header)     override;
  void visit(const Class& cls)         override;
  void visit(const Method& method)     override;
  void visit(const Field& field)       override;
  void visit(const CodeInfo& codeinfo) override;
  void visit(const Type& type)         override;
  void visit(const Prototype& type)    override;
  void visit(const MapItem& item)      override;
  void visit(const MapList& list)      override;
};

}
}

#endif // LIEF_JSON_SUPPORT
