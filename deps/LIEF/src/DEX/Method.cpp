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

#include "LIEF/DEX/Method.hpp"
#include "LIEF/DEX/Prototype.hpp"
#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/hash.hpp"
#include "LIEF/DEX/enums.hpp"
#include "LIEF/DEX/EnumToString.hpp"

#include <numeric>
#include <utility>


namespace LIEF {
namespace DEX {

Method::Method(const Method&) = default;
Method& Method::operator=(const Method&) = default;

Method::Method() = default;


Method::Method(std::string name, Class* parent) :
  name_{std::move(name)},
  parent_{parent}
{}

const std::string& Method::name() const {
  return name_;
}

uint64_t Method::code_offset() const {
  return code_offset_;
}

const Method::bytecode_t& Method::bytecode() const {
  return bytecode_;
}

bool Method::has_class() const {
  return parent_ != nullptr;
}

const Class* Method::cls() const {
  return parent_;
}

Class* Method::cls() {
  return const_cast<Class*>(static_cast<const Method*>(this)->cls());
}

size_t Method::index() const {
  return original_index_;
}

void Method::insert_dex2dex_info(uint32_t pc, uint32_t index) {
  dex2dex_info_.emplace(pc, index);
}

const dex2dex_method_info_t& Method::dex2dex_info() const {
  return dex2dex_info_;
}

bool Method::is_virtual() const {
  return is_virtual_;
}

void Method::set_virtual(bool v) {
  is_virtual_ = v;
}


bool Method::has(ACCESS_FLAGS f) const {
  return (access_flags_ & f) != 0u;
}

Method::access_flags_list_t Method::access_flags() const {
  Method::access_flags_list_t flags;

  std::copy_if(std::begin(access_flags_list),
      std::end(access_flags_list),
      std::back_inserter(flags),
      [this] (ACCESS_FLAGS f) { return has(f); });

  return flags;

}

const Prototype* Method::prototype() const {
  return prototype_;
}

Prototype* Method::prototype() {
  return const_cast<Prototype*>(static_cast<const Method*>(this)->prototype());
}

const CodeInfo& Method::code_info() const {
  return code_info_;
}

void Method::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Method& method) {
  const auto* proto = method.prototype();
  if (!proto) {
    os << method.name() << "()";
    return os;
  }
  Prototype::it_const_params ps = proto->parameters_type();
  std::string pretty_cls_name;
  if (const auto* cls = method.cls()) {
    pretty_cls_name = cls->fullname();
  }

  if (!pretty_cls_name.empty()) {
    pretty_cls_name = pretty_cls_name.substr(1, pretty_cls_name.size() - 2);
    std::replace(std::begin(pretty_cls_name), std::end(pretty_cls_name), '/', '.');
  }

  Method::access_flags_list_t aflags = method.access_flags();
  std::string flags_str = std::accumulate(
      std::begin(aflags), std::end(aflags),
      std::string{},
      [] (const std::string& l, ACCESS_FLAGS r) {
        std::string str = to_string(r);
        std::transform(std::begin(str), std::end(str), std::begin(str), ::tolower);
        return l.empty() ? str : l + " " + str;
      });

  if (!flags_str.empty()) {
    os << flags_str << " ";
  }
  if (const auto* t = proto->return_type()) {
    os << *t << " ";
  }
  os << pretty_cls_name << "->" << method.name();

  os << "(";
  for (size_t i = 0; i < ps.size(); ++i) {
    if (i > 0) {
      os << ", ";
    }
    os << ps[i] << " p" << std::dec << i;
  }
  os << ")";

  return os;
}

Method::~Method() = default;

}
}
