// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TYPE_VISITOR_H_
#define V8_TORQUE_TYPE_VISITOR_H_

#include <string>

#include "src/torque/ast.h"
#include "src/torque/types.h"

namespace v8 {
namespace internal {
namespace torque {

class TypeVisitor {
 public:
  static TypeVector ComputeTypeVector(const std::vector<TypeExpression*>& v) {
    TypeVector result;
    for (TypeExpression* t : v) {
      result.push_back(ComputeType(t));
    }
    return result;
  }

  static const Type* ComputeType(TypeExpression* type_expression);
  static void VisitClassFieldsAndMethods(
      ClassType* class_type, const ClassDeclaration* class_declaration);
  static void VisitStructMethods(StructType* struct_type,
                                 const StructDeclaration* struct_declaration);
  static Signature MakeSignature(const CallableDeclaration* declaration);
  static const StructType* ComputeTypeForStructExpression(
      TypeExpression* type_expression,
      const std::vector<const Type*>& term_argument_types);

 private:
  friend class TypeAlias;
  friend class TypeOracle;
  static const Type* ComputeType(TypeDeclaration* decl);
  static const AbstractType* ComputeType(AbstractTypeDeclaration* decl);
  static const Type* ComputeType(TypeAliasDeclaration* decl);
  static const StructType* ComputeType(
      StructDeclaration* decl,
      StructType::MaybeSpecializationKey specialized_from = base::nullopt);
  static const ClassType* ComputeType(ClassDeclaration* decl);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TYPE_VISITOR_H_
