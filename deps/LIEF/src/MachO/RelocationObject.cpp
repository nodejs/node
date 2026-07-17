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
#include "logging.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/RelocationObject.hpp"
#include "LIEF/MachO/Section.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

RelocationObject::RelocationObject(const details::relocation_info& relocinfo) :
  is_pcrel_{static_cast<bool>(relocinfo.r_pcrel)}
{
  address_ = static_cast<uint32_t>(relocinfo.r_address);
  size_    = static_cast<uint8_t>(relocinfo.r_length);
  type_    = static_cast<uint8_t>(relocinfo.r_type);
}

RelocationObject::RelocationObject(const details::scattered_relocation_info& scattered_relocinfo) :
  is_pcrel_{static_cast<bool>(scattered_relocinfo.r_pcrel)},
  is_scattered_{true},
  value_{scattered_relocinfo.r_value}
{
  address_ = scattered_relocinfo.r_address;
  size_    = static_cast<uint8_t>(scattered_relocinfo.r_length);
  type_    = static_cast<uint8_t>(scattered_relocinfo.r_type);
}

void RelocationObject::swap(RelocationObject& other) noexcept {
  Relocation::swap(other);

  std::swap(is_pcrel_,     other.is_pcrel_);
  std::swap(is_scattered_, other.is_scattered_);
  std::swap(value_,        other.value_);
}

size_t RelocationObject::size() const {
  if (size_ < 2) {
    return ((size_t)size_ + 1) * 8;
  }
  return sizeof(uint32_t) * 8;
}


uint64_t RelocationObject::address() const {
  const Section* sec = section();
  if (sec == nullptr) {
    return Relocation::address();
  }

  return address_ + section()->offset();
}

int32_t RelocationObject::value() const {
  if (!is_scattered()) {
    LIEF_ERR("This relocation is not a 'scattered' one");
    return -1;
  }
  return value_;
}

void RelocationObject::size(size_t size) {
  switch(size) {
    case 8:  size_ = 0; break;
    case 16: size_ = 1; break;
    case 32: size_ = 2; break;
    default: LIEF_ERR("Size must not be bigger than 32 bits");
  }
}

void RelocationObject::value(int32_t value) {
  if (!is_scattered()) {
    LIEF_ERR("This relocation is not a 'scattered' one");
    return;
  }
  value_ = value;
}

void RelocationObject::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

}
}
