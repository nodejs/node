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
#include "ELF/DataHandler/Node.hpp"

namespace LIEF {
namespace ELF {
namespace DataHandler {

bool Node::operator==(const Node& rhs) const {
  if (this == &rhs) {
    return true;
  }
  return type() == rhs.type() &&
         size() == rhs.size() &&
         offset() == rhs.offset();
}

bool Node::operator<(const Node& rhs) const {
  return ((type() == rhs.type() &&
         offset() <= rhs.offset() &&
         (offset() + size()) < (rhs.offset() + rhs.size())) ||
         (type() == rhs.type() &&
         offset() < rhs.offset() &&
         (offset() + size()) <= (rhs.offset() + rhs.size())));

}


bool Node::operator>(const Node& rhs) const {
  return type() == rhs.type() &&
        (offset() > rhs.offset() || (offset() + size()) > (rhs.offset() + rhs.size()));
}

}
}
}
