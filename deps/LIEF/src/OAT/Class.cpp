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
#include <climits>

#include "LIEF/OAT/Class.hpp"
#include "LIEF/OAT/hash.hpp"
#include "LIEF/OAT/EnumToString.hpp"
#include "LIEF/DEX/Method.hpp"

#include "LIEF/DEX/Class.hpp"

#include "logging.hpp"

#if defined(_MSC_VER)
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#endif

namespace LIEF {
namespace OAT {

Class::Class(const Class&) = default;
Class& Class::operator=(const Class&) = default;

Class::Class() = default;

Class::Class(OAT_CLASS_STATUS status, OAT_CLASS_TYPES type,
             DEX::Class* dex_class, std::vector<uint32_t> bitmap) :
  dex_class_{dex_class},
  status_{status},
  type_{type},
  method_bitmap_{std::move(bitmap)}
{}


bool Class::has_dex_class() const {
  return dex_class_ != nullptr;
}

const DEX::Class* Class::dex_class() const {
  return dex_class_;
}

DEX::Class* Class::dex_class() {
  return const_cast<DEX::Class*>(static_cast<const Class*>(this)->dex_class());
}

OAT_CLASS_STATUS Class::status() const {
  return status_;
}

OAT_CLASS_TYPES Class::type() const {
  return type_;
}

Class::it_methods Class::methods() {
  return methods_;
}

Class::it_const_methods Class::methods() const {
  return methods_;
}

DEX::dex2dex_class_info_t Class::dex2dex_info() const {
  if (const DEX::Class* cls = dex_class()) {
    return cls->dex2dex_info();
  }
  return {};
}


const std::string& Class::fullname() const {
  return dex_class()->fullname();
}

size_t Class::index() const {
  if (has_dex_class()) {
    return dex_class()->index();
  }
  return SIZE_MAX;
}

const std::vector<uint32_t>& Class::bitmap() const {
  return method_bitmap_;
}

bool Class::is_quickened(const DEX::Method& m) const {
  if (!has_dex_class()) {
    return false;
  }
  const DEX::Class& cls = *dex_class();

  if (m.bytecode().empty()) {
    return false;
  }

  const auto& methods = cls.methods();
  const auto it_method_index = std::find_if(std::begin(methods), std::end(methods),
      [&m] (const DEX::Method& mth) {
        return &m == &mth;
      });

  if (it_method_index == std::end(methods)) {
    LIEF_ERR("Can't find '{}' in {}", m.name(), cls.fullname());
    return false;
  }

  uint32_t relative_index = std::distance(std::begin(methods), it_method_index);
  return is_quickened(relative_index);

}

bool Class::is_quickened(uint32_t relative_index) const {
  if (type() == OAT_CLASS_TYPES::OAT_CLASS_NONE_COMPILED) {
    return false;
  }

  if (type() == OAT_CLASS_TYPES::OAT_CLASS_ALL_COMPILED) {
    return true;
  }

  if (type() == OAT_CLASS_TYPES::OAT_CLASS_SOME_COMPILED) {
    const uint32_t bitmap_idx  = relative_index >> 5;
    const uint32_t bitmap_mask = 1 << (relative_index & 0x1F);
    if (bitmap_idx > method_bitmap_.size()) {
      LIEF_ERR("bitmap_idx: 0x{:x} is corrupted", bitmap_idx);
      return false;
    }

    return (method_bitmap_[bitmap_idx] & bitmap_mask) != 0;
  }
  return false;
}

uint32_t Class::method_offsets_index(const DEX::Method& m) const {
  if (!has_dex_class()) {
    return UINT_MAX;
  }
  const DEX::Class& cls = *dex_class();

  const auto& methods = cls.methods();
  const auto it_method_index = std::find_if(std::begin(methods), std::end(methods),
      [&m] (const DEX::Method& mth) {
        return &m == &mth;
      });

  if (it_method_index == std::end(methods)) {
    LIEF_ERR("Can't find '{}' in {}", m.name(), cls.fullname());
    return UINT_MAX;
  }

  uint32_t relative_index = std::distance(std::begin(methods), it_method_index);
  return method_offsets_index(relative_index);
}

uint32_t Class::method_offsets_index(uint32_t relative_index) const {

  if (!is_quickened(relative_index) || type() == OAT_CLASS_TYPES::OAT_CLASS_NONE_COMPILED) {
    return UINT_MAX;
  }

  if (type() == OAT_CLASS_TYPES::OAT_CLASS_ALL_COMPILED) {
    return relative_index;
  }

  if (type() == OAT_CLASS_TYPES::OAT_CLASS_SOME_COMPILED) {
    const uint32_t bitmap_end_idx    = relative_index >> 5;
    const uint32_t partial_word_bits = relative_index & 0x1f;
    uint32_t count = 0;
    for (uint32_t word = 0; word < bitmap_end_idx; ++word) {
      count += __builtin_popcount(method_bitmap_[word]);
    }

    if (partial_word_bits != 0) {
      count += __builtin_popcount(method_bitmap_[bitmap_end_idx] & ~(0xffffffffu << partial_word_bits));
    }

    return count;
  }

  return UINT_MAX;
}

uint32_t Class::relative_index(const DEX::Method& m) const {
  if (!has_dex_class()) {
    return UINT_MAX;
  }
  const DEX::Class& cls = *dex_class();

  const auto& methods = cls.methods();
  const auto it_method_index = std::find_if(std::begin(methods), std::end(methods),
      [&m] (const DEX::Method& mth) {
        return &m == &mth;
      });

  if (it_method_index == std::end(methods)) {
    LIEF_ERR("Can't find '{}' in {}", m.name(), cls.fullname());
    return UINT_MAX;
  }

  return std::distance(std::begin(methods), it_method_index);
}

uint32_t Class::relative_index(uint32_t method_absolute_index) const {
  if (!has_dex_class()) {
    return UINT_MAX;
  }
  const DEX::Class& cls = *dex_class();

  const auto& methods = cls.methods();
  const auto it_method_index = std::find_if(std::begin(methods), std::end(methods),
      [method_absolute_index] (const DEX::Method& mth) {
        return mth.index() == method_absolute_index;
      });

  if (it_method_index == std::end(methods)) {
    LIEF_ERR("Can't find find method with index {:d} in {}", method_absolute_index, cls.fullname());
    return UINT_MAX;
  }

  return std::distance(std::begin(methods), it_method_index);

}


void Class::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Class& cls) {
  os << cls.fullname() << " - "
     << to_string(cls.status()) << " - "
     << to_string(cls.type()) << " - "
     << std::dec << cls.methods().size() << " methods";
  return os;
}

Class::~Class() = default;



}
}
