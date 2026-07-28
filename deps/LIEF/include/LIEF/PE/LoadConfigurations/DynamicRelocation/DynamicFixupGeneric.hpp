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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_GENERIC_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_FIXUP_GENERIC_H
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicFixup.hpp"

#include "LIEF/iterators.hpp"

namespace LIEF {
namespace PE {
class Relocation;

/// This class represents a generic entry where fixups are regular
/// relocations (LIEF::PE::Relocation)
class LIEF_API DynamicFixupGeneric : public DynamicFixup {
  public:
  using relocations_t = std::vector<std::unique_ptr<Relocation>>;
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;
  using it_const_relocations = const_ref_iterator<const relocations_t&, Relocation*>;

  DynamicFixupGeneric();

  DynamicFixupGeneric(const DynamicFixupGeneric&);
  DynamicFixupGeneric& operator=(const DynamicFixupGeneric&);

  DynamicFixupGeneric(DynamicFixupGeneric&&);
  DynamicFixupGeneric& operator=(DynamicFixupGeneric&&);

  std::unique_ptr<DynamicFixup> clone() const override {
    return std::unique_ptr<DynamicFixupGeneric>(new DynamicFixupGeneric(*this));
  }

  /// Iterator over the relocations
  it_relocations relocations() {
    return relocations_;
  }

  it_const_relocations relocations() const {
    return relocations_;
  }

  static bool classof(const DynamicFixup* fixup) {
    return fixup->kind() == KIND::GENERIC;
  }

  std::string to_string() const override;

  ~DynamicFixupGeneric() override;

  /// \private
  LIEF_LOCAL static
    std::unique_ptr<DynamicFixupGeneric> parse(Parser& ctx, SpanStream& strm);

  private:
  relocations_t relocations_;
};


}
}

#endif
