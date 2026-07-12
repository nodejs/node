
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

#include "LIEF/DEX/Class.hpp"
#include "LIEF/DEX/Field.hpp"
#include "LIEF/DEX/Method.hpp"
#include "LIEF/DEX/hash.hpp"

namespace LIEF {
namespace DEX {
Class::Class() = default;

Class::Class(std::string  fullname, uint32_t access_flags,
             Class* parent, std::string source_filename) :
  fullname_{std::move(fullname)},
  access_flags_{access_flags},
  parent_{parent},
  source_filename_{std::move(source_filename)},
  original_index_{UINT_MAX}
{}

std::string Class::package_normalized(const std::string& pkg) {
  std::string package_normalized = pkg;

  // 1. Remove the '/' at the end
  if (package_normalized.back() == '/') {
    package_normalized = package_normalized.substr(0, package_normalized.size() - 1);
  }

  // 2. Replace '.' with '/'
  std::replace(std::begin(package_normalized), std::end(package_normalized), '.', '/');
  return package_normalized;
}

std::string Class::fullname_normalized(const std::string& pkg, const std::string& cls_name) {
  return "L" + Class::package_normalized(pkg) + "/" + cls_name + ";";
}

std::string Class::fullname_normalized(const std::string& pkg_cls) {
  std::string package_normalized = pkg_cls;

  // 1. Replace '.' with '/'
  std::replace(std::begin(package_normalized), std::end(package_normalized), '.', '/');

  // 2. Add 'L' at the beginning
  if (package_normalized.front() != 'L') {
    package_normalized = 'L' + package_normalized;
  }

  // 3. Add ';' at the end
  if (package_normalized.back() != ';') {
    package_normalized = package_normalized + ';';
  }

  return package_normalized;
}

const std::string& Class::fullname() const {
  return fullname_;
}


std::string Class::package_name() const {
  size_t pos = fullname_.find_last_of('/');
  if (pos == std::string::npos || fullname_.size() < 2) {
    return "";
  }
  return fullname_.substr(1, pos - 1);
}

std::string Class::name() const {
  size_t pos = fullname_.find_last_of('/');
  if (pos == std::string::npos) {
    return fullname_.substr(1, fullname_.size() - 2);
  } else {
    return fullname_.substr(pos + 1, fullname_.size() - pos - 2);
  }
}

std::string Class::pretty_name() const {
  if (fullname_.size() <= 2) {
    return fullname_;
  }

  std::string pretty_name = fullname_.substr(1, fullname_.size() - 2);
  std::replace(std::begin(pretty_name), std::end(pretty_name), '/', '.');
  return pretty_name;
}


bool Class::has(ACCESS_FLAGS f) const {
  return (access_flags_ & f) > 0;
}

Class::access_flags_list_t Class::access_flags() const {

  Class::access_flags_list_t flags;

  std::copy_if(std::begin(access_flags_list), std::end(access_flags_list),
               std::back_inserter(flags),
               [this] (ACCESS_FLAGS f) { return has(f); });

  return flags;
}


bool Class::has_parent() const {
  return parent_ != nullptr;
}

const Class* Class::parent() const {
  return parent_;
}

Class* Class::parent() {
  return const_cast<Class*>(static_cast<const Class*>(this)->parent());
}

Class::it_const_methods Class::methods() const {
  return methods_;
}

Class::it_methods Class::methods() {
  return methods_;
}

Class::it_const_fields Class::fields() const {
  return fields_;
}

Class::it_fields Class::fields() {
  return fields_;
}

Class::it_named_methods Class::methods(const std::string& name) {
  return {methods_, [name] (const Method* meth) {
    return meth->name() == name;
  }};
}

Class::it_const_named_methods Class::methods(const std::string& name) const {
  return {methods_, [name] (const Method* meth) {
    return meth->name() == name;
  }};
}


Class::it_named_fields Class::fields(const std::string& name) {
  return {fields_, [name] (const Field* f) {
    return f->name() == name;
  }};
}

Class::it_const_named_fields Class::fields(const std::string& name) const {
  return {fields_, [name] (const Field* f) {
    return f->name() == name;
  }};
}

size_t Class::index() const {
  return original_index_;
}

const std::string& Class::source_filename() const {
  return source_filename_;
}

dex2dex_class_info_t Class::dex2dex_info() const {
  dex2dex_class_info_t info;
  for (Method* method : methods_) {
    if (!method->dex2dex_info().empty()) {
      info.emplace(method, method->dex2dex_info());
    }
  }
  return info;
}

void Class::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Class& cls) {
  os << cls.pretty_name();
  if (!cls.source_filename().empty()) {
    os << " - " << cls.source_filename();
  }

  os << " - " << std::dec << cls.methods().size() << " Methods";

  return os;
}

Class::~Class() = default;

}
}
