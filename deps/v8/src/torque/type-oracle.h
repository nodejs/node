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
  static const AbstractType* GetAbstractType(
      const Type* parent, std::string name, bool transient,
      std::string generated,
      base::Optional<const AbstractType*> non_constexpr_version) {
    AbstractType* result =
        new AbstractType(parent, transient, std::move(name),
                         std::move(generated), non_constexpr_version);
    Get().nominal_types_.push_back(std::unique_ptr<AbstractType>(result));
    return result;
  }

  static StructType* GetStructType(const std::string& name) {
    StructType* result = new StructType(CurrentNamespace(), name);
    Get().struct_types_.push_back(std::unique_ptr<StructType>(result));
    return result;
  }

  static ClassType* GetClassType(const Type* parent, const std::string& name,
                                 bool is_extern, bool transient,
                                 const std::string& generates) {
    ClassType* result = new ClassType(parent, CurrentNamespace(), name,
                                      is_extern, transient, generates);
    Get().struct_types_.push_back(std::unique_ptr<ClassType>(result));
    return result;
  }

  static const BuiltinPointerType* GetBuiltinPointerType(
      TypeVector argument_types, const Type* return_type) {
    TypeOracle& self = Get();
    const Type* builtin_type = self.GetBuiltinType(BUILTIN_POINTER_TYPE_STRING);
    const BuiltinPointerType* result = self.function_pointer_types_.Add(
        BuiltinPointerType(builtin_type, argument_types, return_type,
                           self.all_builtin_pointer_types_.size()));
    if (result->function_pointer_type_id() ==
        self.all_builtin_pointer_types_.size()) {
      self.all_builtin_pointer_types_.push_back(result);
    }
    return result;
  }

  static const std::vector<const BuiltinPointerType*>&
  AllBuiltinPointerTypes() {
    return Get().all_builtin_pointer_types_;
  }

  static const Type* GetUnionType(UnionType type) {
    if (base::Optional<const Type*> single = type.GetSingleMember()) {
      return *single;
    }
    return Get().union_types_.Add(std::move(type));
  }

  static const Type* GetUnionType(const Type* a, const Type* b) {
    if (a->IsSubtypeOf(b)) return b;
    if (b->IsSubtypeOf(a)) return a;
    UnionType result = UnionType::FromType(a);
    result.Extend(b);
    return GetUnionType(std::move(result));
  }

  static const TopType* GetTopType(std::string reason,
                                   const Type* source_type) {
    TopType* result = new TopType(std::move(reason), source_type);
    Get().top_types_.push_back(std::unique_ptr<TopType>(result));
    return result;
  }

  static const Type* GetArgumentsType() {
    return Get().GetBuiltinType(ARGUMENTS_TYPE_STRING);
  }

  static const Type* GetBoolType() {
    return Get().GetBuiltinType(BOOL_TYPE_STRING);
  }

  static const Type* GetConstexprBoolType() {
    return Get().GetBuiltinType(CONSTEXPR_BOOL_TYPE_STRING);
  }

  static const Type* GetConstexprIntPtrType() {
    return Get().GetBuiltinType(CONSTEXPR_INTPTR_TYPE_STRING);
  }

  static const Type* GetVoidType() {
    return Get().GetBuiltinType(VOID_TYPE_STRING);
  }

  static const Type* GetRawPtrType() {
    return Get().GetBuiltinType(RAWPTR_TYPE_STRING);
  }

  static const Type* GetMapType() {
    return Get().GetBuiltinType(MAP_TYPE_STRING);
  }

  static const Type* GetObjectType() {
    return Get().GetBuiltinType(OBJECT_TYPE_STRING);
  }

  static const Type* GetJSObjectType() {
    return Get().GetBuiltinType(JSOBJECT_TYPE_STRING);
  }

  static const Type* GetTaggedType() {
    return Get().GetBuiltinType(TAGGED_TYPE_STRING);
  }

  static const Type* GetSmiType() {
    return Get().GetBuiltinType(SMI_TYPE_STRING);
  }

  static const Type* GetConstStringType() {
    return Get().GetBuiltinType(CONST_STRING_TYPE_STRING);
  }

  static const Type* GetStringType() {
    return Get().GetBuiltinType(STRING_TYPE_STRING);
  }

  static const Type* GetNumberType() {
    return Get().GetBuiltinType(NUMBER_TYPE_STRING);
  }

  static const Type* GetIntPtrType() {
    return Get().GetBuiltinType(INTPTR_TYPE_STRING);
  }

  static const Type* GetUIntPtrType() {
    return Get().GetBuiltinType(UINTPTR_TYPE_STRING);
  }

  static const Type* GetInt32Type() {
    return Get().GetBuiltinType(INT32_TYPE_STRING);
  }

  static const Type* GetUint32Type() {
    return Get().GetBuiltinType(UINT32_TYPE_STRING);
  }

  static const Type* GetInt16Type() {
    return Get().GetBuiltinType(INT16_TYPE_STRING);
  }

  static const Type* GetUint16Type() {
    return Get().GetBuiltinType(UINT16_TYPE_STRING);
  }

  static const Type* GetInt8Type() {
    return Get().GetBuiltinType(INT8_TYPE_STRING);
  }

  static const Type* GetUint8Type() {
    return Get().GetBuiltinType(UINT8_TYPE_STRING);
  }

  static const Type* GetFloat64Type() {
    return Get().GetBuiltinType(FLOAT64_TYPE_STRING);
  }

  static const Type* GetNeverType() {
    return Get().GetBuiltinType(NEVER_TYPE_STRING);
  }

  static const Type* GetConstInt31Type() {
    return Get().GetBuiltinType(CONST_INT31_TYPE_STRING);
  }

  static bool IsImplicitlyConvertableFrom(const Type* to, const Type* from) {
    for (Generic* from_constexpr :
         Declarations::LookupGeneric(kFromConstexprMacroName)) {
      if (base::Optional<Callable*> specialization =
              from_constexpr->GetSpecialization({to, from})) {
        if ((*specialization)->signature().GetExplicitTypes() ==
            TypeVector{from}) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  const Type* GetBuiltinType(const std::string& name) {
    return Declarations::LookupGlobalType(name);
  }

  Deduplicator<BuiltinPointerType> function_pointer_types_;
  std::vector<const BuiltinPointerType*> all_builtin_pointer_types_;
  Deduplicator<UnionType> union_types_;
  std::vector<std::unique_ptr<Type>> nominal_types_;
  std::vector<std::unique_ptr<Type>> struct_types_;
  std::vector<std::unique_ptr<Type>> top_types_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TYPE_ORACLE_H_
