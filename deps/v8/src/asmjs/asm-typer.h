// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_ASMJS_ASM_TYPER_H_
#define SRC_ASMJS_ASM_TYPER_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "src/allocation.h"
#include "src/asmjs/asm-types.h"
#include "src/ast/ast-type-bounds.h"
#include "src/ast/ast-types.h"
#include "src/ast/ast.h"
#include "src/effects.h"
#include "src/messages.h"
#include "src/type-info.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

class AsmType;
class AsmTyperHarnessBuilder;
class SourceLayoutTracker;

class AsmTyper final {
 public:
  enum StandardMember {
    kHeap = -4,
    kFFI = -3,
    kStdlib = -2,
    kModule = -1,
    kNone = 0,
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
    kMathClz32,
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

  ~AsmTyper() = default;
  AsmTyper(Isolate* isolate, Zone* zone, Handle<Script> script,
           FunctionLiteral* root);

  bool Validate();
  // Do asm.js validation in phases (to interleave with conversion to wasm).
  bool ValidateBeforeFunctionsPhase();
  bool ValidateInnerFunction(FunctionDeclaration* decl);
  bool ValidateAfterFunctionsPhase();
  void ClearFunctionNodeTypes();

  Handle<JSMessageObject> error_message() const { return error_message_; }
  const MessageLocation* message_location() const { return &message_location_; }

  AsmType* TypeOf(AstNode* node) const;
  AsmType* TypeOf(Variable* v) const;
  StandardMember VariableAsStandardMember(Variable* var);

  // Allow the asm-wasm-builder to trigger failures (for interleaved
  // validating).
  AsmType* FailWithMessage(const char* text);

  typedef std::unordered_set<StandardMember, std::hash<int> > StdlibSet;

  StdlibSet StdlibUses() const { return stdlib_uses_; }

  // Each FFI import has a usage-site signature associated with it.
  struct FFIUseSignature {
    Variable* var;
    ZoneVector<AsmType*> arg_types_;
    AsmType* return_type_;
    FFIUseSignature(Variable* v, Zone* zone)
        : var(v), arg_types_(zone), return_type_(nullptr) {}
  };

  const ZoneVector<FFIUseSignature>& FFIUseSignatures() {
    return ffi_use_signatures_;
  }

 private:
  friend class v8::internal::wasm::AsmTyperHarnessBuilder;

  class VariableInfo : public ZoneObject {
   public:
    enum Mutability {
      kInvalidMutability,
      kLocal,
      kMutableGlobal,
      // *VIOLATION* We support const variables in asm.js, as per the
      //
      // https://discourse.wicg.io/t/allow-const-global-variables/684
      //
      // Global const variables are treated as if they were numeric literals,
      // and can be used anywhere a literal can be used.
      kConstGlobal,
      kImmutableGlobal,
    };

    explicit VariableInfo(AsmType* t) : type_(t) {}

    VariableInfo* Clone(Zone* zone) const;

    bool IsMutable() const {
      return mutability_ == kLocal || mutability_ == kMutableGlobal;
    }

    bool IsGlobal() const {
      return mutability_ == kImmutableGlobal || mutability_ == kConstGlobal ||
             mutability_ == kMutableGlobal;
    }

    bool IsStdlib() const { return standard_member_ == kStdlib; }
    bool IsFFI() const { return standard_member_ == kFFI; }
    bool IsHeap() const { return standard_member_ == kHeap; }

    void MarkDefined() { missing_definition_ = false; }
    void SetFirstForwardUse(const MessageLocation& source_location);

    StandardMember standard_member() const { return standard_member_; }
    void set_standard_member(StandardMember standard_member) {
      standard_member_ = standard_member;
    }

    AsmType* type() const { return type_; }
    void set_type(AsmType* type) { type_ = type; }

    Mutability mutability() const { return mutability_; }
    void set_mutability(Mutability mutability) { mutability_ = mutability; }

    bool missing_definition() const { return missing_definition_; }

    const MessageLocation* source_location() { return &source_location_; }

    static VariableInfo* ForSpecialSymbol(Zone* zone,
                                          StandardMember standard_member);

   private:
    AsmType* type_;
    StandardMember standard_member_ = kNone;
    Mutability mutability_ = kInvalidMutability;
    // missing_definition_ is set to true for forward definition - i.e., use
    // before definition.
    bool missing_definition_ = false;
    // Used for error messages.
    MessageLocation source_location_;
  };

  // RAII-style manager for the in_function_ member variable.
  struct FunctionScope {
    explicit FunctionScope(AsmTyper* typer) : typer_(typer) {
      DCHECK(!typer_->in_function_);
      typer_->in_function_ = true;
      typer_->local_scope_.Clear();
      typer_->return_type_ = AsmType::None();
    }

    ~FunctionScope() {
      DCHECK(typer_->in_function_);
      typer_->in_function_ = false;
    }

    AsmTyper* typer_;
  };

  // FlattenedStatements is an iterator class for ZoneList<Statement*> that
  // flattens the Block construct in the AST. This is here because we need it in
  // the tests.
  class FlattenedStatements {
   public:
    explicit FlattenedStatements(Zone* zone, ZoneList<Statement*>* s);
    Statement* Next();

   private:
    struct Context {
      explicit Context(ZoneList<Statement*>* s) : statements_(s) {}
      ZoneList<Statement*>* statements_;
      int next_index_ = 0;
    };

    ZoneVector<Context> context_stack_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FlattenedStatements);
  };

  class SourceLayoutTracker {
   public:
    SourceLayoutTracker() = default;
    bool IsValid() const;
    void AddUseAsm(const AstNode& node) { use_asm_.AddNewElement(node); }
    void AddGlobal(const AstNode& node) { globals_.AddNewElement(node); }
    void AddFunction(const AstNode& node) { functions_.AddNewElement(node); }
    void AddTable(const AstNode& node) { tables_.AddNewElement(node); }
    void AddExport(const AstNode& node) { exports_.AddNewElement(node); }

   private:
    class Section {
     public:
      Section() = default;
      Section(const Section&) = default;
      Section& operator=(const Section&) = default;

      void AddNewElement(const AstNode& node);
      bool IsPrecededBy(const Section& other) const;

     private:
      int start_ = kNoSourcePosition;
      int end_ = kNoSourcePosition;
    };

    Section use_asm_;
    Section globals_;
    Section functions_;
    Section tables_;
    Section exports_;

    DISALLOW_COPY_AND_ASSIGN(SourceLayoutTracker);
  };

  using ObjectTypeMap = ZoneMap<std::string, VariableInfo*>;
  void InitializeStdlib();
  void SetTypeOf(AstNode* node, AsmType* type);

  void AddForwardReference(VariableProxy* proxy, VariableInfo* info);
  bool AddGlobal(Variable* global, VariableInfo* info);
  bool AddLocal(Variable* global, VariableInfo* info);
  // Used for 5.5 GlobalVariableTypeAnnotations
  VariableInfo* ImportLookup(Property* expr);
  // 3.3 Environment Lookup
  // NOTE: In the spec, the lookup function's prototype is
  //
  //   Lookup(Delta, Gamma, x)
  //
  // Delta is the global_scope_ member, and Gamma, local_scope_.
  VariableInfo* Lookup(Variable* variable) const;

  // All of the ValidateXXX methods below return AsmType::None() in case of
  // validation failure.

  // 6.1 ValidateModule
  AsmType* ValidateModuleBeforeFunctionsPhase(FunctionLiteral* fun);
  AsmType* ValidateModuleFunction(FunctionDeclaration* fun_decl);
  AsmType* ValidateModuleFunctions(FunctionLiteral* fun);
  AsmType* ValidateModuleAfterFunctionsPhase(FunctionLiteral* fun);
  AsmType* ValidateGlobalDeclaration(Assignment* assign);
  // 6.2 ValidateExport
  AsmType* ExportType(VariableProxy* fun_export);
  AsmType* ValidateExport(ReturnStatement* exports);
  // 6.3 ValidateFunctionTable
  AsmType* ValidateFunctionTable(Assignment* assign);
  // 6.4 ValidateFunction
  AsmType* ValidateFunction(FunctionDeclaration* fun_decl);
  // 6.5 ValidateStatement
  AsmType* ValidateStatement(Statement* statement);
  // 6.5.1 BlockStatement
  AsmType* ValidateBlockStatement(Block* block);
  // 6.5.2 ExpressionStatement
  AsmType* ValidateExpressionStatement(ExpressionStatement* expr);
  // 6.5.3 EmptyStatement
  AsmType* ValidateEmptyStatement(EmptyStatement* empty);
  // 6.5.4 IfStatement
  AsmType* ValidateIfStatement(IfStatement* if_stmt);
  // 6.5.5 ReturnStatement
  AsmType* ValidateReturnStatement(ReturnStatement* ret_stmt);
  // 6.5.6 IterationStatement
  // 6.5.6.a WhileStatement
  AsmType* ValidateWhileStatement(WhileStatement* while_stmt);
  // 6.5.6.b DoWhileStatement
  AsmType* ValidateDoWhileStatement(DoWhileStatement* do_while);
  // 6.5.6.c ForStatement
  AsmType* ValidateForStatement(ForStatement* for_stmt);
  // 6.5.7 BreakStatement
  AsmType* ValidateBreakStatement(BreakStatement* brk_stmt);
  // 6.5.8 ContinueStatement
  AsmType* ValidateContinueStatement(ContinueStatement* cont_stmt);
  // 6.5.9 LabelledStatement
  // NOTE: we don't need to handle these: Labelled statements are
  // BreakableStatements in our AST, but BreakableStatement is not a concrete
  // class -- and we're handling all of BreakableStatement's subclasses.
  // 6.5.10 SwitchStatement
  AsmType* ValidateSwitchStatement(SwitchStatement* stmt);
  // 6.6 ValidateCase
  AsmType* ValidateCase(CaseClause* label, int32_t* case_lbl);
  // 6.7 ValidateDefault
  AsmType* ValidateDefault(CaseClause* label);
  // 6.8 ValidateExpression
  AsmType* ValidateExpression(Expression* expr);
  AsmType* ValidateCompareOperation(CompareOperation* cmp);
  AsmType* ValidateBinaryOperation(BinaryOperation* binop);
  // 6.8.1 Expression
  AsmType* ValidateCommaExpression(BinaryOperation* comma);
  // 6.8.2 NumericLiteral
  AsmType* ValidateNumericLiteral(Literal* literal);
  // 6.8.3 Identifier
  AsmType* ValidateIdentifier(VariableProxy* proxy);
  // 6.8.4 CallExpression
  AsmType* ValidateCallExpression(Call* call);
  // 6.8.5 MemberExpression
  AsmType* ValidateMemberExpression(Property* prop);
  // 6.8.6 AssignmentExpression
  AsmType* ValidateAssignmentExpression(Assignment* assignment);
  // 6.8.7 UnaryExpression
  AsmType* ValidateUnaryExpression(UnaryOperation* unop);
  // 6.8.8 MultiplicativeExpression
  AsmType* ValidateMultiplicativeExpression(BinaryOperation* binop);
  // 6.8.9 AdditiveExpression
  AsmType* ValidateAdditiveExpression(BinaryOperation* binop,
                                      uint32_t intish_count);
  // 6.8.10 ShiftExpression
  AsmType* ValidateShiftExpression(BinaryOperation* binop);
  // 6.8.11 RelationalExpression
  AsmType* ValidateRelationalExpression(CompareOperation* cmpop);
  // 6.8.12 EqualityExpression
  AsmType* ValidateEqualityExpression(CompareOperation* cmpop);
  // 6.8.13 BitwiseANDExpression
  AsmType* ValidateBitwiseANDExpression(BinaryOperation* binop);
  // 6.8.14 BitwiseXORExpression
  AsmType* ValidateBitwiseXORExpression(BinaryOperation* binop);
  // 6.8.15 BitwiseORExpression
  AsmType* ValidateBitwiseORExpression(BinaryOperation* binop);
  // 6.8.16 ConditionalExpression
  AsmType* ValidateConditionalExpression(Conditional* cond);
  // 6.9 ValidateCall
  AsmType* ValidateCall(AsmType* return_type, Call* call);
  // 6.10 ValidateHeapAccess
  enum HeapAccessType { LoadFromHeap, StoreToHeap };
  AsmType* ValidateHeapAccess(Property* heap, HeapAccessType access_type);
  // 6.11 ValidateFloatCoercion
  bool IsCallToFround(Call* call);
  AsmType* ValidateFloatCoercion(Call* call);

  // 5.1 ParameterTypeAnnotations
  AsmType* ParameterTypeAnnotations(Variable* parameter,
                                    Expression* annotation);
  // 5.2 ReturnTypeAnnotations
  AsmType* ReturnTypeAnnotations(ReturnStatement* statement);
  // 5.4 VariableTypeAnnotations
  // 5.5 GlobalVariableTypeAnnotations
  AsmType* VariableTypeAnnotations(
      Expression* initializer,
      VariableInfo::Mutability global = VariableInfo::kLocal);
  AsmType* ImportExpression(Property* import);
  AsmType* NewHeapView(CallNew* new_heap_view);

  Isolate* isolate_;
  Zone* zone_;
  Handle<Script> script_;
  FunctionLiteral* root_;
  bool in_function_ = false;

  AsmType* return_type_ = nullptr;

  ZoneVector<VariableInfo*> forward_definitions_;
  ZoneVector<FFIUseSignature> ffi_use_signatures_;
  ObjectTypeMap stdlib_types_;
  ObjectTypeMap stdlib_math_types_;

  // The ASM module name. This member is used to prevent globals from redefining
  // the module name.
  VariableInfo* module_info_;
  Handle<String> module_name_;

  // 3 Environments
  ZoneHashMap global_scope_;  // 3.1 Global environment
  ZoneHashMap local_scope_;   // 3.2 Variable environment

  std::uintptr_t stack_limit_;
  bool stack_overflow_ = false;
  std::unordered_map<AstNode*, AsmType*> module_node_types_;
  std::unordered_map<AstNode*, AsmType*> function_node_types_;
  static const int kErrorMessageLimit = 128;
  AsmType* fround_type_;
  AsmType* ffi_type_;
  Handle<JSMessageObject> error_message_;
  MessageLocation message_location_;
  StdlibSet stdlib_uses_;

  SourceLayoutTracker source_layout_;
  ReturnStatement* module_return_;
  ZoneVector<Assignment*> function_pointer_tables_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AsmTyper);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // SRC_ASMJS_ASM_TYPER_H_
