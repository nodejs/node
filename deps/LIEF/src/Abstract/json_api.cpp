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
#include "LIEF/Abstract/json.hpp"

#ifdef LIEF_JSON_SUPPORT
#include "Abstract/json_internal.hpp"
#endif

namespace LIEF {

std::string to_json_from_abstract([[maybe_unused]] const Object& v) {
#if LIEF_JSON_SUPPORT
  AbstractJsonVisitor visitor;
  v.accept(visitor);
  return visitor.get().dump();
#else
  LIEF_WARN("JSON support is not enabled");
  return "";
#endif
}

} // namespace LIEF
