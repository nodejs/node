// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

bool Type::IsSubtypeOf(const Type* supertype) const {
  const Type* subtype = this;
  while (subtype != nullptr) {
    if (subtype == supertype) return true;
    subtype = subtype->parent();
  }
  return false;
}

bool Type::IsAbstractName(const std::string& name) const {
  if (!IsAbstractType()) return false;
  return AbstractType::cast(this)->name() == name;
}

std::string AbstractType::GetGeneratedTNodeTypeName() const {
  std::string result = GetGeneratedTypeName();
  DCHECK_EQ(result.substr(0, 6), "TNode<");
  result = result.substr(6, result.length() - 7);
  return result;
}

std::string FunctionPointerType::ToString() const {
  std::stringstream result;
  result << "builtin (";
  bool first = true;
  for (const Type* t : parameter_types_) {
    if (!first) {
      result << ", ";
      first = false;
    }
    result << t;
  }
  result << ") => " << return_type_;
  return result.str();
}

std::string FunctionPointerType::MangledName() const {
  std::stringstream result;
  result << "FT";
  bool first = true;
  for (const Type* t : parameter_types_) {
    if (!first) {
      result << ", ";
      first = false;
    }
    std::string arg_type_string = t->MangledName();
    result << arg_type_string.size() << arg_type_string;
  }
  std::string return_type_string = return_type_->MangledName();
  result << return_type_string.size() << return_type_string;
  return result.str();
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
