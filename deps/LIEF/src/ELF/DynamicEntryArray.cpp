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
#include "LIEF/ELF/DynamicEntryArray.hpp"
#include "LIEF/Visitor.hpp"

#include "logging.hpp"

#include <algorithm>

namespace LIEF {
namespace ELF {

DynamicEntryArray& DynamicEntryArray::remove(uint64_t function) {
  array_.erase(std::remove_if(std::begin(array_), std::end(array_),
                              [function] (uint64_t v) { return v == function; }),
               std::end(array_));
  return *this;
}

DynamicEntryArray& DynamicEntryArray::insert(size_t pos, uint64_t function) {
  if (pos == array_.size()) {
    return append(function);
  }

  if (pos > array_.size()) {
    LIEF_ERR("pos: {:d} is out of range", pos);
    return *this;
  }

  array_.insert(std::begin(array_) + pos, function);
  return *this;
}

const uint64_t& DynamicEntryArray::operator[](size_t idx) const {
  static uint64_t GARBAGE = 0;
  if (idx >= array_.size()) {
    LIEF_WARN("DynamicEntryArray[{}] is out-of-range", idx);
    return GARBAGE;
  }
  return array_[idx];
}

uint64_t& DynamicEntryArray::operator[](size_t idx) {
  return const_cast<uint64_t&>(static_cast<const DynamicEntryArray*>(this)->operator[](idx));
}

void DynamicEntryArray::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DynamicEntryArray::print(std::ostream& os) const {
  const array_t& array = this->array();
  DynamicEntry::print(os);
  os << '[' << fmt::format("0x{:04x}", fmt::join(array, ", ")) << ']';
  return os;
}

}
}



