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

#include "LIEF/OAT/Method.hpp"
#include "LIEF/OAT/hash.hpp"
#include "LIEF/OAT/Class.hpp"
#include "LIEF/DEX/Method.hpp"

namespace LIEF {
namespace OAT {

Method::Method() = default;
Method::Method(const Method&) = default;
Method& Method::operator=(const Method&) = default;

Method::Method(DEX::Method* method, Class* oat_class, std::vector<uint8_t>  quick_code) :
  dex_method_{method},
  class_{oat_class},
  quick_code_{std::move(quick_code)}
{}

const Class* Method::oat_class() const {
  return class_;
}

Class* Method::oat_class() {
  return const_cast<Class*>(static_cast<const Method*>(this)->oat_class());
}


bool Method::has_dex_method() const {
  return dex_method_ != nullptr;
}

const DEX::Method* Method::dex_method() const {
  return dex_method_;
}

DEX::Method* Method::dex_method() {
  return const_cast<DEX::Method*>(static_cast<const Method*>(this)->dex_method());
}

bool Method::is_dex2dex_optimized() const {
  return !dex2dex_info().empty();
}

bool Method::is_compiled() const {
  return !quick_code_.empty();
}


std::string Method::name() const {
  if (dex_method_ == nullptr) {
    return "";
  }

  return dex_method_->name();
}

const DEX::dex2dex_method_info_t& Method::dex2dex_info() const {
  return dex_method_->dex2dex_info();
}


const Method::quick_code_t& Method::quick_code() const {
  return quick_code_;
}

void Method::quick_code(const Method::quick_code_t& code) {
  quick_code_ = code;
}

void Method::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Method& meth) {
  std::string pretty_name = meth.oat_class()->fullname();
  pretty_name = pretty_name.substr(1, pretty_name.size() - 2);

  os << pretty_name << "." << meth.name();
  if (meth.is_compiled()) {
    os << " - Compiled";
  }

  if (meth.is_dex2dex_optimized()) {
    os << " - Optimized";
  }

  return os;
}

Method::~Method() = default;



}
}
