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
#include <utility>

#include "LIEF/Visitor.hpp"

#include "LIEF/PE/ResourceData.hpp"

namespace LIEF {
namespace PE {

void ResourceData::swap(ResourceData& other) noexcept {
  ResourceNode::swap(other);

  std::swap(content_,    other.content_);
  std::swap(code_page_,  other.code_page_);
  std::swap(reserved_,   other.reserved_);
}

void ResourceData::accept(Visitor& visitor) const {
  visitor.visit(*this);
}


}
}
