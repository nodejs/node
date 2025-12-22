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
#include "frozen.hpp"
#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"

namespace LIEF {
namespace MachO {

DyldBindingInfo::DyldBindingInfo(CLASS cls, TYPE type, uint64_t address,
                                 int64_t addend, int32_t oridnal, bool is_weak, bool is_non_weak_definition,
                                 uint64_t offset) :
  class_{cls},
  binding_type_{type},
  is_non_weak_definition_{is_non_weak_definition},
  offset_{offset}
{
  library_ordinal_ = oridnal;
  addend_          = addend;
  is_weak_import_  = is_weak;
  address_         = address;
}

void DyldBindingInfo::swap(DyldBindingInfo& other) noexcept {
  BindingInfo::swap(other);
  std::swap(class_,                   other.class_);
  std::swap(binding_type_,            other.binding_type_);
  std::swap(is_non_weak_definition_,  other.is_non_weak_definition_);
  std::swap(offset_,                  other.offset_);
}

void DyldBindingInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

const char* to_string(DyldBindingInfo::CLASS e) {
  #define ENTRY(X) std::pair(DyldBindingInfo::CLASS::X, #X)
  STRING_MAP enums2str {
    ENTRY(WEAK),
    ENTRY(LAZY),
    ENTRY(STANDARD),
    ENTRY(THREADED),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(DyldBindingInfo::TYPE e) {
  #define ENTRY(X) std::pair(DyldBindingInfo::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(POINTER),
    ENTRY(TEXT_ABSOLUTE32),
    ENTRY(TEXT_PCREL32),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
