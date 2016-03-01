// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPING_ASM_H_
#define V8_TYPING_ASM_H_

#include "src/allocation.h"
#include "src/ast/ast.h"
#include "src/effects.h"
#include "src/type-info.h"
#include "src/types.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class TypeCache;

class AsmTyper : public AstVisitor {
 public:
  explicit AsmTyper(Isolate* isolate, Zone* zone, Script* script,
                    FunctionLiteral* root);
  bool Validate();
  void set_allow_simd(bool simd);
  const char* error_message() { return error_message_; }

  enum StandardMember {
    kNone = 0,
    kStdlib,
    kInfinity,
    kNaN,
    kMathAcos,
    kMathAsin,
    kMathAtan,
    kMathCos,
    kMathSin,
    kMathTan,
    kMathExp,
    kMathLog,
    kMathCeil,
    kMathFloor,
    kMathSqrt,
    kMathAbs,
    kMathMin,
    kMathMax,
    kMathAtan2,
    kMathPow,
    kMathImul,
    kMathFround,
    kMathE,
    kMathLN10,
    kMathLN2,
    kMathLOG2E,
    kMathLOG10E,
    kMathPI,
    kMathSQRT1_2,
    kMathSQRT2,
  };

  StandardMember VariableAsStandardMember(Variable* variable);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  Zone* zone_;
  Isolate* isolate_;
  Script* script_;
  FunctionLiteral* root_;
  bool valid_;
  bool allow_simd_;

  struct VariableInfo : public ZoneObject {
    Type* type;
    bool is_check_function;
    bool is_constructor_function;
    StandardMember standard_member;

    VariableInfo()
        : type(NULL),
          is_check_function(false),
          is_constructor_function(false),
          standard_member(kNone) {}
    explicit VariableInfo(Type* t)
        : type(t),
          is_check_function(false),
          is_constructor_function(false),
          standard_member(kNone) {}
  };

  // Information for bi-directional typing with a cap on nesting depth.
  Type* expected_type_;
  Type* computed_type_;
  VariableInfo* property_info_;
  int intish_;  // How many ops we've gone without a x|0.

  Type* return_type_;  // Return type of last function.
  size_t array_size_;  // Array size of last ArrayLiteral.

  typedef ZoneMap<std::string, VariableInfo*> ObjectTypeMap;
  ObjectTypeMap stdlib_types_;
  ObjectTypeMap stdlib_heap_types_;
  ObjectTypeMap stdlib_math_types_;
#define V(NAME, Name, name, lane_count, lane_type) \
  ObjectTypeMap stdlib_simd_##name##_types_;       \
  VariableInfo* stdlib_simd_##name##_constructor_type_;
  SIMD128_TYPES(V)
#undef V

  // Map from Variable* to global/local variable Type*.
  ZoneHashMap global_variable_type_;
  ZoneHashMap local_variable_type_;

  bool in_function_;  // In module function?
  bool building_function_tables_;

  TypeCache const& cache_;

  static const int kErrorMessageLimit = 100;
  char error_message_[kErrorMessageLimit];

  static const int kMaxUncombinedAdditiveSteps = 1 << 20;
  static const int kMaxUncombinedMultiplicativeSteps = 1;

  void InitializeStdlib();
  void InitializeStdlibSIMD();

  void VisitDeclarations(ZoneList<Declaration*>* d) override;
  void VisitStatements(ZoneList<Statement*>* s) override;

  void VisitExpressionAnnotation(Expression* e, Variable* var, bool is_return);
  void VisitFunctionAnnotation(FunctionLiteral* f);
  void VisitAsmModule(FunctionLiteral* f);

  void VisitHeapAccess(Property* expr, bool assigning, Type* assignment_type);

  Expression* GetReceiverOfPropertyAccess(Expression* expr, const char* name);
  bool IsMathObject(Expression* expr);
  bool IsSIMDObject(Expression* expr);
  bool IsSIMDTypeObject(Expression* expr, const char* name);
  bool IsStdlibObject(Expression* expr);

  void VisitSIMDProperty(Property* expr);

  int ElementShiftSize(Type* type);
  Type* StorageType(Type* type);

  void SetType(Variable* variable, Type* type);
  Type* GetType(Variable* variable);
  VariableInfo* GetVariableInfo(Variable* variable, bool setting);
  void SetVariableInfo(Variable* variable, const VariableInfo* info);

  VariableInfo* LibType(ObjectTypeMap* map, Handle<String> name);
  void VisitLibraryAccess(ObjectTypeMap* map, Property* expr);

  void SetResult(Expression* expr, Type* type);
  void IntersectResult(Expression* expr, Type* type);

  void VisitWithExpectation(Expression* expr, Type* expected_type,
                            const char* msg);

  void VisitLiteral(Literal* expr, bool is_return);

  void VisitIntegerBitwiseOperator(BinaryOperation* expr, Type* left_expected,
                                   Type* right_expected, Type* result_type,
                                   bool conversion);

  Zone* zone() const { return zone_; }

#define DECLARE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(AsmTyper);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_TYPING_ASM_H_
