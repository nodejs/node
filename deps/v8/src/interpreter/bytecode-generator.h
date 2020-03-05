// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_GENERATOR_H_
#define V8_INTERPRETER_BYTECODE_GENERATOR_H_

#include "src/ast/ast.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/function-kind.h"

namespace v8 {
namespace internal {

class AstNodeSourceRanges;
class AstStringConstants;
class BytecodeArray;
class UnoptimizedCompilationInfo;
enum class SourceRangeKind;

namespace interpreter {

class GlobalDeclarationsBuilder;
class LoopBuilder;
class BlockCoverageBuilder;
class BytecodeJumpTable;

class BytecodeGenerator final : public AstVisitor<BytecodeGenerator> {
 public:
  explicit BytecodeGenerator(
      UnoptimizedCompilationInfo* info,
      const AstStringConstants* ast_string_constants,
      std::vector<FunctionLiteral*>* eager_inner_literals);

  void GenerateBytecode(uintptr_t stack_limit);
  Handle<BytecodeArray> FinalizeBytecode(Isolate* isolate,
                                         Handle<Script> script);
  Handle<ByteArray> FinalizeSourcePositionTable(Isolate* isolate);

#ifdef DEBUG
  int CheckBytecodeMatches(Handle<BytecodeArray> bytecode);
#endif

#define DECLARE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Visiting function for declarations list and statements are overridden.
  void VisitGlobalDeclarations(Declaration::List* declarations);
  void VisitDeclarations(Declaration::List* declarations);
  void VisitStatements(const ZonePtrList<Statement>* statments);

 private:
  class ContextScope;
  class ControlScope;
  class ControlScopeForBreakable;
  class ControlScopeForIteration;
  class ControlScopeForTopLevel;
  class ControlScopeForTryCatch;
  class ControlScopeForTryFinally;
  class CurrentScope;
  class ExpressionResultScope;
  class EffectResultScope;
  class FeedbackSlotCache;
  class GlobalDeclarationsBuilder;
  class IteratorRecord;
  class NaryCodeCoverageSlots;
  class RegisterAllocationScope;
  class AccumulatorPreservingScope;
  class TestResultScope;
  class ValueResultScope;
  class OptionalChainNullLabelScope;

  using ToBooleanMode = BytecodeArrayBuilder::ToBooleanMode;

  enum class TestFallthrough { kThen, kElse, kNone };
  enum class TypeHint { kAny, kBoolean, kString };
  enum class AccumulatorPreservingMode { kNone, kPreserve };

  // An assignment has to evaluate its LHS before its RHS, but has to assign to
  // the LHS after both evaluations are done. This class stores the data
  // computed in the LHS evaulation that has to live across the RHS evaluation,
  // and is used in the actual LHS assignment.
  class AssignmentLhsData {
   public:
    static AssignmentLhsData NonProperty(Expression* expr);
    static AssignmentLhsData NamedProperty(Expression* object_expr,
                                           Register object,
                                           const AstRawString* name);
    static AssignmentLhsData KeyedProperty(Register object, Register key);
    static AssignmentLhsData PrivateMethodOrAccessor(AssignType type,
                                                     Property* property);
    static AssignmentLhsData NamedSuperProperty(
        RegisterList super_property_args);
    static AssignmentLhsData KeyedSuperProperty(
        RegisterList super_property_args);

    AssignType assign_type() const { return assign_type_; }
    Expression* expr() const {
      DCHECK(assign_type_ == NON_PROPERTY || assign_type_ == PRIVATE_METHOD ||
             assign_type_ == PRIVATE_GETTER_ONLY ||
             assign_type_ == PRIVATE_SETTER_ONLY ||
             assign_type_ == PRIVATE_GETTER_AND_SETTER);
      return expr_;
    }
    Expression* object_expr() const {
      DCHECK_EQ(assign_type_, NAMED_PROPERTY);
      return object_expr_;
    }
    Register object() const {
      DCHECK(assign_type_ == NAMED_PROPERTY || assign_type_ == KEYED_PROPERTY);
      return object_;
    }
    Register key() const {
      DCHECK(assign_type_ == KEYED_PROPERTY);
      return key_;
    }
    const AstRawString* name() const {
      DCHECK(assign_type_ == NAMED_PROPERTY);
      return name_;
    }
    RegisterList super_property_args() const {
      DCHECK(assign_type_ == NAMED_SUPER_PROPERTY ||
             assign_type_ == KEYED_SUPER_PROPERTY);
      return super_property_args_;
    }

   private:
    AssignmentLhsData(AssignType assign_type, Expression* expr,
                      RegisterList super_property_args, Register object,
                      Register key, Expression* object_expr,
                      const AstRawString* name)
        : assign_type_(assign_type),
          expr_(expr),
          super_property_args_(super_property_args),
          object_(object),
          key_(key),
          object_expr_(object_expr),
          name_(name) {}

    AssignType assign_type_;

    // Different assignment types use different fields:
    //
    // NON_PROPERTY: expr
    // NAMED_PROPERTY: object_expr, object, name
    // KEYED_PROPERTY, PRIVATE_METHOD: object, key
    // NAMED_SUPER_PROPERTY: super_property_args
    // KEYED_SUPER_PROPERT:  super_property_args
    Expression* expr_;
    RegisterList super_property_args_;
    Register object_;
    Register key_;
    Expression* object_expr_;
    const AstRawString* name_;
  };

  void GenerateBytecodeBody();
  void AllocateDeferredConstants(Isolate* isolate, Handle<Script> script);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

  // Dispatched from VisitBinaryOperation.
  void VisitArithmeticExpression(BinaryOperation* binop);
  void VisitCommaExpression(BinaryOperation* binop);
  void VisitLogicalOrExpression(BinaryOperation* binop);
  void VisitLogicalAndExpression(BinaryOperation* binop);
  void VisitNullishExpression(BinaryOperation* binop);

  // Dispatched from VisitNaryOperation.
  void VisitNaryArithmeticExpression(NaryOperation* expr);
  void VisitNaryCommaExpression(NaryOperation* expr);
  void VisitNaryLogicalOrExpression(NaryOperation* expr);
  void VisitNaryLogicalAndExpression(NaryOperation* expr);
  void VisitNaryNullishExpression(NaryOperation* expr);

  // Dispatched from VisitUnaryOperation.
  void VisitVoid(UnaryOperation* expr);
  void VisitTypeOf(UnaryOperation* expr);
  void VisitNot(UnaryOperation* expr);
  void VisitDelete(UnaryOperation* expr);

  // Visits a typeof expression for the value on which to perform the typeof.
  void VisitForTypeOfValue(Expression* expr);

  // Used by flow control routines to evaluate loop condition.
  void VisitCondition(Expression* expr);

  // Visit the arguments expressions in |args| and store them in |args_regs|,
  // growing |args_regs| for each argument visited.
  void VisitArguments(const ZonePtrList<Expression>* args,
                      RegisterList* arg_regs);

  // Visit a keyed super property load. The optional
  // |opt_receiver_out| register will have the receiver stored to it
  // if it's a valid register. The loaded value is placed in the
  // accumulator.
  void VisitKeyedSuperPropertyLoad(Property* property,
                                   Register opt_receiver_out);

  // Visit a named super property load. The optional
  // |opt_receiver_out| register will have the receiver stored to it
  // if it's a valid register. The loaded value is placed in the
  // accumulator.
  void VisitNamedSuperPropertyLoad(Property* property,
                                   Register opt_receiver_out);

  void VisitPropertyLoad(Register obj, Property* expr);
  void VisitPropertyLoadForRegister(Register obj, Property* expr,
                                    Register destination);

  AssignmentLhsData PrepareAssignmentLhs(
      Expression* lhs, AccumulatorPreservingMode accumulator_preserving_mode =
                           AccumulatorPreservingMode::kNone);
  void BuildAssignment(const AssignmentLhsData& data, Token::Value op,
                       LookupHoistingMode lookup_hoisting_mode);

  void BuildThisVariableLoad();

  Expression* GetDestructuringDefaultValue(Expression** target);
  void BuildDestructuringArrayAssignment(
      ArrayLiteral* pattern, Token::Value op,
      LookupHoistingMode lookup_hoisting_mode);
  void BuildDestructuringObjectAssignment(
      ObjectLiteral* pattern, Token::Value op,
      LookupHoistingMode lookup_hoisting_mode);

  void BuildLoadNamedProperty(const Expression* object_expr, Register object,
                              const AstRawString* name);
  void BuildStoreNamedProperty(const Expression* object_expr, Register object,
                               const AstRawString* name);

  void BuildVariableLoad(Variable* variable, HoleCheckMode hole_check_mode,
                         TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  void BuildVariableLoadForAccumulatorValue(
      Variable* variable, HoleCheckMode hole_check_mode,
      TypeofMode typeof_mode = NOT_INSIDE_TYPEOF);
  void BuildVariableAssignment(
      Variable* variable, Token::Value op, HoleCheckMode hole_check_mode,
      LookupHoistingMode lookup_hoisting_mode = LookupHoistingMode::kNormal);
  void BuildLiteralCompareNil(Token::Value compare_op,
                              BytecodeArrayBuilder::NilValue nil);
  void BuildReturn(int source_position = kNoSourcePosition);
  void BuildAsyncReturn(int source_position = kNoSourcePosition);
  void BuildAsyncGeneratorReturn();
  void BuildReThrow();
  void BuildHoleCheckForVariableAssignment(Variable* variable, Token::Value op);
  void BuildThrowIfHole(Variable* variable);

  void BuildNewLocalActivationContext();
  void BuildLocalActivationContextInitialization();
  void BuildNewLocalBlockContext(Scope* scope);
  void BuildNewLocalCatchContext(Scope* scope);
  void BuildNewLocalWithContext(Scope* scope);

  void BuildGeneratorPrologue();
  void BuildSuspendPoint(int position);

  void BuildAwait(int position = kNoSourcePosition);
  void BuildAwait(Expression* await_expr);

  void BuildFinalizeIteration(IteratorRecord iterator, Register done,
                              Register iteration_continuation_token);

  void BuildGetIterator(IteratorType hint);

  // Create an IteratorRecord with pre-allocated registers holding the next
  // method and iterator object.
  IteratorRecord BuildGetIteratorRecord(Register iterator_next,
                                        Register iterator_object,
                                        IteratorType hint);

  // Create an IteratorRecord allocating new registers to hold the next method
  // and iterator object.
  IteratorRecord BuildGetIteratorRecord(IteratorType hint);
  void BuildIteratorNext(const IteratorRecord& iterator, Register next_result);
  void BuildIteratorClose(const IteratorRecord& iterator,
                          Expression* expr = nullptr);
  void BuildCallIteratorMethod(Register iterator, const AstRawString* method,
                               RegisterList receiver_and_args,
                               BytecodeLabel* if_called,
                               BytecodeLabels* if_notcalled);

  void BuildFillArrayWithIterator(IteratorRecord iterator, Register array,
                                  Register index, Register value,
                                  FeedbackSlot next_value_slot,
                                  FeedbackSlot next_done_slot,
                                  FeedbackSlot index_slot,
                                  FeedbackSlot element_slot);
  // Create Array literals. |expr| can be nullptr, but if provided,
  // a boilerplate will be used to create an initial array for elements
  // before the first spread.
  void BuildCreateArrayLiteral(const ZonePtrList<Expression>* elements,
                               ArrayLiteral* expr);
  void BuildCreateObjectLiteral(Register literal, uint8_t flags, size_t entry);
  void AllocateTopLevelRegisters();
  void VisitArgumentsObject(Variable* variable);
  void VisitRestArgumentsArray(Variable* rest);
  void VisitCallSuper(Call* call);
  void BuildInvalidPropertyAccess(MessageTemplate tmpl, Property* property);
  void BuildPrivateBrandCheck(Property* property, Register object,
                              MessageTemplate tmpl);
  void BuildPrivateGetterAccess(Register obj, Register access_pair);
  void BuildPrivateSetterAccess(Register obj, Register access_pair,
                                Register value);
  void BuildPrivateMethods(ClassLiteral* expr, bool is_static,
                           Register home_object);
  void BuildClassLiteral(ClassLiteral* expr, Register name);
  void VisitClassLiteral(ClassLiteral* expr, Register name);
  void VisitNewTargetVariable(Variable* variable);
  void VisitThisFunctionVariable(Variable* variable);
  void BuildPrivateBrandInitialization(Register receiver);
  void BuildInstanceMemberInitialization(Register constructor,
                                         Register instance);
  void BuildGeneratorObjectVariableInitialization();
  void VisitBlockDeclarationsAndStatements(Block* stmt);
  void VisitSetHomeObject(Register value, Register home_object,
                          LiteralProperty* property);
  void VisitLiteralAccessor(Register home_object, LiteralProperty* property,
                            Register value_out);
  void VisitForInAssignment(Expression* expr);
  void VisitModuleNamespaceImports();

  // Visit a logical OR/AND within a test context, rewiring the jumps based
  // on the expression values.
  void VisitLogicalTest(Token::Value token, Expression* left, Expression* right,
                        int right_coverage_slot);
  void VisitNaryLogicalTest(Token::Value token, NaryOperation* expr,
                            const NaryCodeCoverageSlots* coverage_slots);

  // Visit a (non-RHS) test for a logical op, which falls through if the test
  // fails or jumps to the appropriate labels if it succeeds.
  void VisitLogicalTestSubExpression(Token::Value token, Expression* expr,
                                     BytecodeLabels* then_labels,
                                     BytecodeLabels* else_labels,
                                     int coverage_slot);

  // Helpers for binary and nary logical op value expressions.
  bool VisitLogicalOrSubExpression(Expression* expr, BytecodeLabels* end_labels,
                                   int coverage_slot);
  bool VisitLogicalAndSubExpression(Expression* expr,
                                    BytecodeLabels* end_labels,
                                    int coverage_slot);

  // Helper for binary and nary nullish op value expressions.
  bool VisitNullishSubExpression(Expression* expr, BytecodeLabels* end_labels,
                                 int coverage_slot);

  // Visit the body of a loop iteration.
  void VisitIterationBody(IterationStatement* stmt, LoopBuilder* loop_builder);

  // Visit a statement and switch scopes, the context is in the accumulator.
  void VisitInScope(Statement* stmt, Scope* scope);

  void BuildPushUndefinedIntoRegisterList(RegisterList* reg_list);

  void BuildLoadPropertyKey(LiteralProperty* property, Register out_reg);

  int AllocateBlockCoverageSlotIfEnabled(AstNode* node, SourceRangeKind kind);
  int AllocateNaryBlockCoverageSlotIfEnabled(NaryOperation* node, size_t index);

  void BuildIncrementBlockCoverageCounterIfEnabled(AstNode* node,
                                                   SourceRangeKind kind);
  void BuildIncrementBlockCoverageCounterIfEnabled(int coverage_array_slot);

  void BuildTest(ToBooleanMode mode, BytecodeLabels* then_labels,
                 BytecodeLabels* else_labels, TestFallthrough fallthrough);

  template <typename TryBodyFunc, typename CatchBodyFunc>
  void BuildTryCatch(TryBodyFunc try_body_func, CatchBodyFunc catch_body_func,
                     HandlerTable::CatchPrediction catch_prediction,
                     TryCatchStatement* stmt_for_coverage = nullptr);
  template <typename TryBodyFunc, typename FinallyBodyFunc>
  void BuildTryFinally(TryBodyFunc try_body_func,
                       FinallyBodyFunc finally_body_func,
                       HandlerTable::CatchPrediction catch_prediction,
                       TryFinallyStatement* stmt_for_coverage = nullptr);

  template <typename ExpressionFunc>
  void BuildOptionalChain(ExpressionFunc expression_func);

  // Visitors for obtaining expression result in the accumulator, in a
  // register, or just getting the effect. Some visitors return a TypeHint which
  // specifies the type of the result of the visited expression.
  TypeHint VisitForAccumulatorValue(Expression* expr);
  void VisitForAccumulatorValueOrTheHole(Expression* expr);
  V8_WARN_UNUSED_RESULT Register VisitForRegisterValue(Expression* expr);
  V8_INLINE void VisitForRegisterValue(Expression* expr, Register destination);
  void VisitAndPushIntoRegisterList(Expression* expr, RegisterList* reg_list);
  void VisitForEffect(Expression* expr);
  void VisitForTest(Expression* expr, BytecodeLabels* then_labels,
                    BytecodeLabels* else_labels, TestFallthrough fallthrough);
  void VisitForNullishTest(Expression* expr, BytecodeLabels* then_labels,
                           BytecodeLabels* test_next_labels,
                           BytecodeLabels* else_labels);

  void VisitInSameTestExecutionScope(Expression* expr);

  Register GetRegisterForLocalVariable(Variable* variable);

  // Returns the runtime function id for a store to super for the function's
  // language mode.
  inline Runtime::FunctionId StoreToSuperRuntimeId();
  inline Runtime::FunctionId StoreKeyedToSuperRuntimeId();

  // Returns a cached slot, or create and cache a new slot if one doesn't
  // already exists.
  FeedbackSlot GetCachedLoadGlobalICSlot(TypeofMode typeof_mode,
                                         Variable* variable);
  FeedbackSlot GetCachedStoreGlobalICSlot(LanguageMode language_mode,
                                          Variable* variable);
  FeedbackSlot GetCachedLoadICSlot(const Expression* expr,
                                   const AstRawString* name);
  FeedbackSlot GetCachedStoreICSlot(const Expression* expr,
                                    const AstRawString* name);
  FeedbackSlot GetDummyCompareICSlot();

  int GetCachedCreateClosureSlot(FunctionLiteral* literal);

  void AddToEagerLiteralsIfEager(FunctionLiteral* literal);

  // Checks if the visited expression is one shot, i.e executed only once. Any
  // expression either in a top level code or an IIFE that is not within a loop
  // is eligible for one shot optimizations.
  inline bool ShouldOptimizeAsOneShot() const;

  static constexpr ToBooleanMode ToBooleanModeFromTypeHint(TypeHint type_hint) {
    return type_hint == TypeHint::kBoolean ? ToBooleanMode::kAlreadyBoolean
                                           : ToBooleanMode::kConvertToBoolean;
  }

  inline Register generator_object() const;

  inline BytecodeArrayBuilder* builder() { return &builder_; }
  inline Zone* zone() const { return zone_; }
  inline DeclarationScope* closure_scope() const { return closure_scope_; }
  inline UnoptimizedCompilationInfo* info() const { return info_; }
  inline const AstStringConstants* ast_string_constants() const {
    return ast_string_constants_;
  }

  inline Scope* current_scope() const { return current_scope_; }
  inline void set_current_scope(Scope* scope) { current_scope_ = scope; }

  inline ControlScope* execution_control() const { return execution_control_; }
  inline void set_execution_control(ControlScope* scope) {
    execution_control_ = scope;
  }
  inline ContextScope* execution_context() const { return execution_context_; }
  inline void set_execution_context(ContextScope* context) {
    execution_context_ = context;
  }
  inline void set_execution_result(ExpressionResultScope* execution_result) {
    execution_result_ = execution_result;
  }
  ExpressionResultScope* execution_result() const { return execution_result_; }
  BytecodeRegisterAllocator* register_allocator() {
    return builder()->register_allocator();
  }

  GlobalDeclarationsBuilder* globals_builder() {
    DCHECK_NOT_NULL(globals_builder_);
    return globals_builder_;
  }
  inline LanguageMode language_mode() const;
  inline FunctionKind function_kind() const;
  inline FeedbackVectorSpec* feedback_spec();
  inline int feedback_index(FeedbackSlot slot) const;

  inline FeedbackSlotCache* feedback_slot_cache() {
    return feedback_slot_cache_;
  }

  inline HandlerTable::CatchPrediction catch_prediction() const {
    return catch_prediction_;
  }
  inline void set_catch_prediction(HandlerTable::CatchPrediction value) {
    catch_prediction_ = value;
  }

  Zone* zone_;
  BytecodeArrayBuilder builder_;
  UnoptimizedCompilationInfo* info_;
  const AstStringConstants* ast_string_constants_;
  DeclarationScope* closure_scope_;
  Scope* current_scope_;

  // External vector of literals to be eagerly compiled.
  std::vector<FunctionLiteral*>* eager_inner_literals_;

  FeedbackSlotCache* feedback_slot_cache_;

  GlobalDeclarationsBuilder* globals_builder_;
  BlockCoverageBuilder* block_coverage_builder_;
  ZoneVector<std::pair<FunctionLiteral*, size_t>> function_literals_;
  ZoneVector<std::pair<NativeFunctionLiteral*, size_t>>
      native_function_literals_;
  ZoneVector<std::pair<ObjectLiteral*, size_t>> object_literals_;
  ZoneVector<std::pair<ArrayLiteral*, size_t>> array_literals_;
  ZoneVector<std::pair<ClassLiteral*, size_t>> class_literals_;
  ZoneVector<std::pair<GetTemplateObject*, size_t>> template_objects_;

  ControlScope* execution_control_;
  ContextScope* execution_context_;
  ExpressionResultScope* execution_result_;

  Register incoming_new_target_or_generator_;

  BytecodeLabels* optional_chaining_null_labels_;

  // Dummy feedback slot for compare operations, where we don't care about
  // feedback
  SharedFeedbackSlot dummy_feedback_slot_;

  BytecodeJumpTable* generator_jump_table_;
  int suspend_count_;
  int loop_depth_;

  HandlerTable::CatchPrediction catch_prediction_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_GENERATOR_H_
