/* Copyright 2021 - 2025 R. Thomas
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
#include "LIEF/DEX/Field.hpp"
#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/hash.hpp"
#include "LIEF/DEX/enums.hpp"
#include "LIEF/DEX/EnumToString.hpp"
#include "LIEF/DEX/Method.hpp"

#include <numeric>
#include <utility>


namespace LIEF {
namespace DEX {

Field::Field(const Field&) = default;
Field& Field::operator=(const Field&) = default;

Field::Field() = default;

Field::Field(std::string name, Class* parent) :
  name_{std::move(name)},
  parent_{parent}
{}

const std::string& Field::name() const {
  return name_;
}

bool Field::has_class() const {
  return parent_ != nullptr;
}

const Class* Field::cls() const {
  return parent_;
}

Class* Field::cls() {
  return const_cast<Class*>(static_cast<const Field*>(this)->cls());
}

size_t Field::index() const {
  return original_index_;
}

bool Field::is_static() const {
    return is_static_;
}

void Field::set_static(bool v) {
    is_static_ = v;
}


bool Field::has(ACCESS_FLAGS f) const {
  return (access_flags_ & f) != 0u;
}

Field::access_flags_list_t Field::access_flags() const {
  Field::access_flags_list_t flags;

  std::copy_if(std::begin(access_flags_list), std::end(access_flags_list),
               std::back_inserter(flags),
               [this] (ACCESS_FLAGS f) { return has(f); });

  return flags;

}

const Type* Field::type() const {
  CHECK(type_ != nullptr, "Type is null!");
  return type_;
}

Type* Field::type() {
  return const_cast<Type*>(static_cast<const Field*>(this)->type());
}

void Field::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Field& field) {
  std::string pretty_cls_name = field.cls()->fullname();
  if (!pretty_cls_name.empty()) {
    pretty_cls_name = pretty_cls_name.substr(1, pretty_cls_name.size() - 2);
    std::replace(std::begin(pretty_cls_name), std::end(pretty_cls_name), '/', '.');
  }

  Method::access_flags_list_t aflags = field.access_flags();
  std::string flags_str = std::accumulate(
      std::begin(aflags),
      std::end(aflags),
      std::string{},
      [] (const std::string& l, ACCESS_FLAGS r) {
        std::string str = to_string(r);
        std::transform(std::begin(str), std::end(str), std::begin(str), ::tolower);
        return l.empty() ? str : l + " " + str;
      });

  if (!flags_str.empty()) {
    os << flags_str << " ";
  }
  os << field.type()
     << " "
     << pretty_cls_name << "->" << field.name();

  return os;
}

Field::~Field() = default;

}
}
