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
#include "LIEF/Object.hpp"
#include "LIEF/hash.hpp"

namespace LIEF {
Object::Object() = default;
Object::Object(const Object&) = default;
Object& Object::operator=(const Object&) = default;
Object::~Object() = default;

bool Object::operator==(const Object& other) const {
  if (this == &other) {
    return true;
  }

  return hash(*this) == hash(other);
}

}
