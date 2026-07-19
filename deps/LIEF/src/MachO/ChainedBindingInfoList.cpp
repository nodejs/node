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
#include "ChainedBindingInfoList.hpp"

#include "LIEF/MachO/ChainedBindingInfo.hpp"

namespace LIEF {
namespace MachO {

std::unique_ptr<ChainedBindingInfoList>
  ChainedBindingInfoList::create(const ChainedBindingInfo& other)
{
  auto result = std::make_unique<ChainedBindingInfoList>(other.format(), other.is_weak_import());
  result->segment_         = const_cast<SegmentCommand*>(other.segment());
  result->symbol_          = const_cast<Symbol*>(other.symbol());
  result->library_ordinal_ = other.library_ordinal();
  result->addend_          = other.addend();
  result->library_         = const_cast<DylibCommand*>(other.library());
  result->address_         = other.address();
  result->ptr_format_      = other.ptr_format();
  result->offset_          = other.offset();
  result->btypes_          = other.btypes_;
  result->arm64_bind_      = other.arm64_bind_;
  return result;
}

void ChainedBindingInfoList::swap(ChainedBindingInfoList& other) noexcept {
  ChainedBindingInfo::swap(other);
  std::swap(elements_, other.elements_);
}

}
}
