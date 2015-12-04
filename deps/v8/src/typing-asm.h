// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPING_ASM_H_
#define V8_TYPING_ASM_H_

#include "src/allocation.h"
#include "src/ast.h"
#include "src/effects.h"
#include "src/type-info.h"
#include "src/types.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class ZoneTypeCache;

class AsmTyper : public AstVisitor {
 public:
  explicit AsmTyper(Isolate* isolate, Zone* zone, Script* script,
                    FunctionLiteral* root);
  bool Validate();
  const char* error_message() { return error_message_; }

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  Script* script_;
  FunctionLiteral* root_;
  bool valid_;

  // Information for bi-directional typing with a cap on nesting depth.
  Type* expected_type_;
  Type* computed_type_;
  int intish_;  // How many ops we've gone without a x|0.

  Type* return_type_;  // Return type of last function.
  size_t array_size_;  // Array size of last ArrayLiteral.

  typedef ZoneMap<std::string, Type*> ObjectTypeMap;
  ObjectTypeMap stdlib_types_;
  ObjectTypeMap stdlib_heap_types_;
  ObjectTypeMap stdlib_math_types_;

  // Map from Variable* to global/local variable Type*.
  ZoneHashMap global_variable_type_;
  ZoneHashMap local_variable_type_;

  bool in_function_;  // In module function?
  bool building_function_tables_;

  ZoneTypeCache const& cache_;

  static const int kErrorMessageLimit = 100;
  char error_message_[kErrorMessageLimit];

  static const int kMaxUncombinedAdditiveSteps = 1 << 20;
  static const int kMaxUncombinedMultiplicativeSteps = 1;

  void InitializeStdlib();

  void VisitDeclarations(ZoneList<Declaration*>* d) override;
  void VisitStatements(ZoneList<Statement*>* s) override;

  void VisitExpressionAnnotation(Expression* e);
  void VisitFunctionAnnotation(FunctionLiteral* f);
  void VisitAsmModule(FunctionLiteral* f);

  void VisitHeapAccess(Property* expr);

  int ElementShiftSize(Type* type);

  void SetType(Variable* variable, Type* type);
  Type* GetType(Variable* variable);

  Type* LibType(ObjectTypeMap map, Handle<String> name);

  void SetResult(Expression* expr, Type* type);
  void IntersectResult(Expression* expr, Type* type);

  void VisitWithExpectation(Expression* expr, Type* expected_type,
                            const char* msg);

#define DECLARE_VISIT(type) virtual void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(AsmTyper);
};
}
}  // namespace v8::internal

#endif  // V8_TYPING_ASM_H_
