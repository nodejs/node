// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TYPE_ORACLE_H_
#define V8_TORQUE_TYPE_ORACLE_H_

#include "src/torque/contextual.h"
#include "src/torque/declarable.h"
#include "src/torque/declarations.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class TypeOracle : public ContextualClass<TypeOracle> {
 public:
  explicit TypeOracle(Declarations* declarations)
      : declarations_(declarations) {}

  static const Type* GetArgumentsType() {
    return Get().GetBuiltinType(ARGUMENTS_TYPE_STRING);
  }

  static const Type* GetBoolType() {
    return Get().GetBuiltinType(BOOL_TYPE_STRING);
  }

  static const Type* GetConstexprBoolType() {
    return Get().GetBuiltinType(CONSTEXPR_BOOL_TYPE_STRING);
  }

  static const Type* GetVoidType() {
    return Get().GetBuiltinType(VOID_TYPE_STRING);
  }

  static const Type* GetObjectType() {
    return Get().GetBuiltinType(OBJECT_TYPE_STRING);
  }

  static const Type* GetConstStringType() {
    return Get().GetBuiltinType(CONST_STRING_TYPE_STRING);
  }

  static const Type* GetIntPtrType() {
    return Get().GetBuiltinType(INTPTR_TYPE_STRING);
  }

  static const Type* GetNeverType() {
    return Get().GetBuiltinType(NEVER_TYPE_STRING);
  }

  static const Type* GetConstInt31Type() {
    return Get().GetBuiltinType(CONST_INT31_TYPE_STRING);
  }

  static bool IsImplicitlyConverableFrom(const Type* to, const Type* from) {
    std::string name = GetGeneratedCallableName(kFromConstexprMacroName, {to});
    return Get().declarations_->TryLookupMacro(name, {from}) != nullptr;
  }

 private:
  const Type* GetBuiltinType(const std::string& name) {
    return declarations_->LookupGlobalType(name);
  }

  Declarations* declarations_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TYPE_ORACLE_H_
