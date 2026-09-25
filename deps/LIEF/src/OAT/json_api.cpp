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
#include "LIEF/config.h"

#include "logging.hpp"
#include "LIEF/OAT/json.hpp"

#ifdef LIEF_JSON_SUPPORT
#include "OAT/json_internal.hpp"
#endif

#include "LIEF/OAT.hpp"

namespace LIEF {
namespace OAT {

std::string to_json(const Object& v) {
#ifdef LIEF_JSON_SUPPORT
  JsonVisitor visitor;
  v.accept(visitor);
  return visitor.get().dump();
#else
  LIEF_WARN("JSON support is not enabled");
  return "";
#endif
}

} // namespace OAT
} // namespace LIEF
