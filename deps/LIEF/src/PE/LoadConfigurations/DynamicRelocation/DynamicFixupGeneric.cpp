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
#include <sstream>
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixupGeneric.hpp"

#include "logging.hpp"

namespace LIEF::PE {

DynamicFixupGeneric::DynamicFixupGeneric(DynamicFixupGeneric&&) = default;
DynamicFixupGeneric& DynamicFixupGeneric::operator=(DynamicFixupGeneric&&) = default;
DynamicFixupGeneric::~DynamicFixupGeneric() = default;

DynamicFixupGeneric::DynamicFixupGeneric() :
  DynamicFixup(KIND::GENERIC)
{}

DynamicFixupGeneric::DynamicFixupGeneric(const DynamicFixupGeneric& other) :
  DynamicFixup(other)
{
  if (!other.relocations_.empty()) {
    relocations_.reserve(other.relocations_.size());
    std::transform(other.relocations_.begin(), other.relocations_.end(),
                   std::back_inserter(relocations_),
      [] (const std::unique_ptr<Relocation>& R) {
        return std::make_unique<Relocation>(*R);
      }
    );
  }
}

DynamicFixupGeneric& DynamicFixupGeneric::operator=(const DynamicFixupGeneric& other) {
  if (this == &other) {
    return *this;
  }
  DynamicFixup::operator=(other);

  if (!other.relocations_.empty()) {
    relocations_.reserve(other.relocations_.size());
    std::transform(other.relocations_.begin(), other.relocations_.end(),
                   std::back_inserter(relocations_),
      [] (const std::unique_ptr<Relocation>& R) {
        return std::make_unique<Relocation>(*R);
      }
    );
  }

  return *this;
}

std::unique_ptr<DynamicFixupGeneric>
  DynamicFixupGeneric::parse(Parser& ctx, SpanStream& strm)
{
  auto generic = std::make_unique<DynamicFixupGeneric>();
  generic->relocations_ = Relocation::parse(ctx, strm);
  return generic;
}

std::string DynamicFixupGeneric::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Fixup RVAs (Generic)\n";
  size_t idx = 0;
  for (const Relocation& R : relocations()) {
    for (const RelocationEntry& E : R.entries()) {
      oss << format("  [{:04d}] RVA: 0x{:08x} Type: {}\n", idx++, E.address(),
                    PE::to_string(E.type()));
    }
  }
  return oss.str();

}
}
