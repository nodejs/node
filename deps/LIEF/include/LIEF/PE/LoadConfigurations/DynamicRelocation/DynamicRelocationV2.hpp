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
#ifndef LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_V2_H
#define LIEF_PE_LOAD_CONFIGURATION_DYNAMIC_RELOCATION_V2_H

#include <memory>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicRelocationBase.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;

/// This class represents a dynamic relocation (`IMAGE_DYNAMIC_RELOCATION64_V2` or
/// `IMAGE_DYNAMIC_RELOCATION32_V2`)
class LIEF_API DynamicRelocationV2 : public DynamicRelocation {
  public:
  DynamicRelocationV2() :
    DynamicRelocation(2)
  {}

  DynamicRelocationV2(const DynamicRelocationV2&) = default;
  DynamicRelocationV2& operator=(const DynamicRelocationV2&) = default;

  DynamicRelocationV2(DynamicRelocationV2&&) = default;
  DynamicRelocationV2& operator=(DynamicRelocationV2&&) = default;

  std::unique_ptr<DynamicRelocation> clone() const override {
    return std::unique_ptr<DynamicRelocationV2>(new DynamicRelocationV2(*this));
  }

  std::string to_string() const override;

  static bool classof(const DynamicRelocation* reloc) {
    return reloc->version() == 2;
  }

  ~DynamicRelocationV2() override = default;

  /// \private
  template<class PE_T> LIEF_LOCAL static
    std::unique_ptr<DynamicRelocationV2> parse(Parser& ctx, BinaryStream& strm);

};

}
}

#endif
