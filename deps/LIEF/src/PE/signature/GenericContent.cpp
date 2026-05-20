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
#include "LIEF/Visitor.hpp"
#include "LIEF/PE/signature/GenericContent.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

static constexpr const char GENERIC_OBJID[] = "LIEF_CONTENT_GENERIC";

GenericContent::GenericContent() :
  ContentInfo::Content(GENERIC_OBJID)
{}

GenericContent::GenericContent(oid_t oid) :
  ContentInfo::Content(GENERIC_OBJID),
  oid_(std::move(oid))
{}

GenericContent::~GenericContent() = default;

void GenericContent::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

bool GenericContent::classof(const ContentInfo::Content* content) {
  return content->content_type() == GENERIC_OBJID;
}

void GenericContent::print(std::ostream& os) const {
  os << fmt::format("oid: {}\n", oid());
}


}
}
