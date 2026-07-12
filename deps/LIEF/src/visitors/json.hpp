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
#ifndef LIEF_VISITOR_JSONS_H
#define LIEF_VISITOR_JSONS_H

#include "LIEF/Visitor.hpp"

#ifndef LIEF_NLOHMANN_JSON_EXTERNAL
#include "internal/nlohmann/json.hpp"
#else
#include <nlohmann/json.hpp>
#endif

using json = nlohmann::json;

namespace LIEF {

class JsonVisitor : public Visitor {
  public:
  JsonVisitor();
  JsonVisitor(json node);
  JsonVisitor(const JsonVisitor&);
  JsonVisitor& operator=(const JsonVisitor&);

  inline json get() const {
    return node_;
  }

  protected:
  json node_;
};

}
#endif
