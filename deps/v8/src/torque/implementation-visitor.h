// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_IMPLEMENTATION_VISITOR_H_
#define V8_TORQUE_IMPLEMENTATION_VISITOR_H_

#include <string>

#include "src/base/macros.h"
#include "src/torque/ast.h"
#include "src/torque/cfg.h"
#include "src/torque/file-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

// LocationReference is the representation of an l-value, so a value that might
// allow for assignment. For uniformity, this class can also represent
// unassignable temporaries. Assignable values fall in two categories:
//   - stack ranges that represent mutable variables, including structs.
//   - field or element access expressions that generate operator calls.
class LocationReference {
 public:
  // An assignable stack range.
  static LocationReference VariableAccess(VisitResult variable) {
    DCHECK(variable.IsOnStack());
    LocationReference result;
    result.variable_ = std::move(variable);
    return result;
  }
  // An unassignable value. {description} is only used for error messages.
  static LocationReference Temporary(VisitResult temporary,
                                     std::string description) {
    LocationReference result;
    result.temporary_ = std::move(temporary);
    result.temporary_description_ = std::move(description);
    return result;
  }
  // A heap reference, that is, a tagged value and an offset to encode an inner
  // pointer.
  static LocationReference HeapReference(VisitResult heap_reference) {
    LocationReference result;
    DCHECK(heap_reference.type()->IsReferenceType());
    result.heap_reference_ = std::move(heap_reference);
    return result;
  }
  static LocationReference ArrayAccess(VisitResult base, VisitResult offset) {
    LocationReference result;
    result.eval_function_ = std::string{"[]"};
    result.assign_function_ = std::string{"[]="};
    result.call_arguments_ = {base, offset};
    return result;
  }
  static LocationReference FieldAccess(VisitResult object,
                                       std::string fieldname) {
    LocationReference result;
    result.eval_function_ = "." + fieldname;
    result.assign_function_ = "." + fieldname + "=";
    result.call_arguments_ = {object};
    result.index_field_ = base::nullopt;
    return result;
  }
  static LocationReference IndexedFieldIndexedAccess(
      const LocationReference& indexed_field, VisitResult index) {
    LocationReference result;
    DCHECK(indexed_field.IsIndexedFieldAccess());
    std::string fieldname = *indexed_field.index_field_;
    result.eval_function_ = "." + fieldname + "[]";
    result.assign_function_ = "." + fieldname + "[]=";
    result.call_arguments_ = indexed_field.call_arguments_;
    result.call_arguments_.push_back(index);
    result.index_field_ = fieldname;
    return result;
  }
  static LocationReference IndexedFieldAccess(VisitResult object,
                                              std::string fieldname) {
    LocationReference result;
    result.call_arguments_ = {object};
    result.index_field_ = fieldname;
    return result;
  }

  bool IsConst() const { return temporary_.has_value(); }

  bool IsVariableAccess() const { return variable_.has_value(); }
  const VisitResult& variable() const {
    DCHECK(IsVariableAccess());
    return *variable_;
  }
  bool IsTemporary() const { return temporary_.has_value(); }
  const VisitResult& temporary() const {
    DCHECK(IsTemporary());
    return *temporary_;
  }
  bool IsHeapReference() const { return heap_reference_.has_value(); }
  const VisitResult& heap_reference() const {
    DCHECK(IsHeapReference());
    return *heap_reference_;
  }

  const Type* ReferencedType() const {
    if (IsHeapReference()) {
      return ReferenceType::cast(heap_reference().type())->referenced_type();
    }
    return GetVisitResult().type();
  }

  const VisitResult& GetVisitResult() const {
    if (IsVariableAccess()) return variable();
    DCHECK(IsTemporary());
    return temporary();
  }

  // For error reporting.
  const std::string& temporary_description() const {
    DCHECK(IsTemporary());
    return *temporary_description_;
  }

  bool IsArrayField() const { return index_field_.has_value(); }
  bool IsIndexedFieldAccess() const {
    return IsArrayField() && !IsCallAccess();
  }
  bool IsIndexedFieldIndexedAccess() const {
    return IsArrayField() && IsCallAccess();
  }
  bool IsCallAccess() const {
    bool is_call_access = eval_function_.has_value();
    DCHECK_EQ(is_call_access, assign_function_.has_value());
    return is_call_access;
  }
  const VisitResultVector& call_arguments() const {
    DCHECK(IsCallAccess());
    return call_arguments_;
  }
  const std::string& eval_function() const {
    DCHECK(IsCallAccess());
    return *eval_function_;
  }
  const std::string& assign_function() const {
    DCHECK(IsCallAccess());
    return *assign_function_;
  }

 private:
  base::Optional<VisitResult> variable_;
  base::Optional<VisitResult> temporary_;
  base::Optional<std::string> temporary_description_;
  base::Optional<VisitResult> heap_reference_;
  base::Optional<std::string> eval_function_;
  base::Optional<std::string> assign_function_;
  VisitResultVector call_arguments_;
  base::Optional<std::string> index_field_;

  LocationReference() = default;
};

struct InitializerResults {
  std::vector<Identifier*> names;
  NameValueMap field_value_map;
};

template <class T>
class Binding;

template <class T>
class BindingsManager {
 public:
  base::Optional<Binding<T>*> TryLookup(const std::string& name) {
    return current_bindings_[name];
  }

 private:
  friend class Binding<T>;
  std::unordered_map<std::string, base::Optional<Binding<T>*>>
      current_bindings_;
};

template <class T>
class Binding : public T {
 public:
  template <class... Args>
  Binding(BindingsManager<T>* manager, const std::string& name, Args&&... args)
      : T(std::forward<Args>(args)...),
        manager_(manager),
        name_(name),
        previous_binding_(this) {
    std::swap(previous_binding_, manager_->current_bindings_[name]);
  }
  template <class... Args>
  Binding(BindingsManager<T>* manager, const Identifier* name, Args&&... args)
      : Binding(manager, name->value, std::forward<Args>(args)...) {
    declaration_position_ = name->pos;
  }
  ~Binding() { manager_->current_bindings_[name_] = previous_binding_; }

  const std::string& name() const { return name_; }
  SourcePosition declaration_position() const { return declaration_position_; }

 private:
  BindingsManager<T>* manager_;
  const std::string name_;
  base::Optional<Binding*> previous_binding_;
  SourcePosition declaration_position_ = CurrentSourcePosition::Get();
  DISALLOW_COPY_AND_ASSIGN(Binding);
};

template <class T>
class BlockBindings {
 public:
  explicit BlockBindings(BindingsManager<T>* manager) : manager_(manager) {}
  void Add(std::string name, T value) {
    ReportErrorIfAlreadyBound(name);
    bindings_.push_back(base::make_unique<Binding<T>>(manager_, std::move(name),
                                                      std::move(value)));
  }

  void Add(const Identifier* name, T value) {
    ReportErrorIfAlreadyBound(name->value);
    bindings_.push_back(
        base::make_unique<Binding<T>>(manager_, name, std::move(value)));
  }

  std::vector<Binding<T>*> bindings() const {
    std::vector<Binding<T>*> result;
    result.reserve(bindings_.size());
    for (auto& b : bindings_) {
      result.push_back(b.get());
    }
    return result;
  }

 private:
  void ReportErrorIfAlreadyBound(const std::string& name) {
    for (const auto& binding : bindings_) {
      if (binding->name() == name) {
        ReportError(
            "redeclaration of name \"", name,
            "\" in the same block is illegal, previous declaration at: ",
            binding->declaration_position());
      }
    }
  }

  BindingsManager<T>* manager_;
  std::vector<std::unique_ptr<Binding<T>>> bindings_;
};

struct LocalValue {
  bool is_const;
  VisitResult value;
};

struct LocalLabel {
  Block* block;
  std::vector<const Type*> parameter_types;

  explicit LocalLabel(Block* block,
                      std::vector<const Type*> parameter_types = {})
      : block(block), parameter_types(std::move(parameter_types)) {}
};

struct Arguments {
  VisitResultVector parameters;
  std::vector<Binding<LocalLabel>*> labels;
};

// Determine if a callable should be considered as an overload.
bool IsCompatibleSignature(const Signature& sig, const TypeVector& types,
                           size_t label_count);

class ImplementationVisitor : public FileVisitor {
 public:
  void GenerateBuiltinDefinitions(std::string& file_name);
  void GenerateClassDefinitions(std::string& file_name);
  void GeneratePrintDefinitions(std::string& file_name);

  VisitResult Visit(Expression* expr);
  const Type* Visit(Statement* stmt);

  InitializerResults VisitInitializerResults(
      const AggregateType* aggregate,
      const std::vector<NameAndExpression>& expressions);

  void InitializeFieldFromSpread(VisitResult object, const Field& field,
                                 const InitializerResults& initializer_results);

  size_t InitializeAggregateHelper(
      const AggregateType* aggregate_type, VisitResult allocate_result,
      const InitializerResults& initializer_results);

  VisitResult AddVariableObjectSize(
      VisitResult object_size, const ClassType* current_class,
      const InitializerResults& initializer_results);

  void InitializeAggregate(const AggregateType* aggregate_type,
                           VisitResult allocate_result,
                           const InitializerResults& initializer_results);

  VisitResult TemporaryUninitializedStruct(const StructType* struct_type,
                                           const std::string& reason);
  VisitResult Visit(StructExpression* decl);

  LocationReference GetLocationReference(Expression* location);
  LocationReference GetLocationReference(IdentifierExpression* expr);
  LocationReference GetLocationReference(DereferenceExpression* expr);
  LocationReference GetLocationReference(FieldAccessExpression* expr);
  LocationReference GetLocationReference(ElementAccessExpression* expr);

  VisitResult GenerateFetchFromLocation(const LocationReference& reference);

  VisitResult GetBuiltinCode(Builtin* builtin);

  VisitResult Visit(LocationExpression* expr);

  void VisitAllDeclarables();
  void Visit(Declarable* delarable);
  void Visit(TypeAlias* decl);
  VisitResult InlineMacro(Macro* macro,
                          base::Optional<LocationReference> this_reference,
                          const std::vector<VisitResult>& arguments,
                          const std::vector<Block*> label_blocks);
  void VisitMacroCommon(Macro* macro);
  void Visit(Macro* macro);
  void Visit(Method* macro);
  void Visit(Builtin* builtin);
  void Visit(NamespaceConstant* decl);

  VisitResult Visit(CallExpression* expr, bool is_tail = false);
  VisitResult Visit(CallMethodExpression* expr);
  VisitResult Visit(IntrinsicCallExpression* intrinsic);
  const Type* Visit(TailCallStatement* stmt);

  VisitResult Visit(ConditionalExpression* expr);

  VisitResult Visit(LogicalOrExpression* expr);
  VisitResult Visit(LogicalAndExpression* expr);

  VisitResult Visit(IncrementDecrementExpression* expr);
  VisitResult Visit(AssignmentExpression* expr);
  VisitResult Visit(StringLiteralExpression* expr);
  VisitResult Visit(NumberLiteralExpression* expr);
  VisitResult Visit(AssumeTypeImpossibleExpression* expr);
  VisitResult Visit(TryLabelExpression* expr);
  VisitResult Visit(StatementExpression* expr);
  VisitResult Visit(NewExpression* expr);
  VisitResult Visit(SpreadExpression* expr);

  const Type* Visit(ReturnStatement* stmt);
  const Type* Visit(GotoStatement* stmt);
  const Type* Visit(IfStatement* stmt);
  const Type* Visit(WhileStatement* stmt);
  const Type* Visit(BreakStatement* stmt);
  const Type* Visit(ContinueStatement* stmt);
  const Type* Visit(ForLoopStatement* stmt);
  const Type* Visit(VarDeclarationStatement* stmt);
  const Type* Visit(VarDeclarationStatement* stmt,
                    BlockBindings<LocalValue>* block_bindings);
  const Type* Visit(ForOfLoopStatement* stmt);
  const Type* Visit(BlockStatement* block);
  const Type* Visit(ExpressionStatement* stmt);
  const Type* Visit(DebugStatement* stmt);
  const Type* Visit(AssertStatement* stmt);

  void BeginNamespaceFile(Namespace* nspace);
  void EndNamespaceFile(Namespace* nspace);

  void GenerateImplementation(const std::string& dir, Namespace* nspace);

  DECLARE_CONTEXTUAL_VARIABLE(ValueBindingsManager,
                              BindingsManager<LocalValue>);
  DECLARE_CONTEXTUAL_VARIABLE(LabelBindingsManager,
                              BindingsManager<LocalLabel>);
  DECLARE_CONTEXTUAL_VARIABLE(CurrentCallable, Callable*);
  DECLARE_CONTEXTUAL_VARIABLE(CurrentReturnValue, base::Optional<VisitResult>);

  // A BindingsManagersScope has to be active for local bindings to be created.
  // Shadowing an existing BindingsManagersScope by creating a new one hides all
  // existing bindings while the additional BindingsManagersScope is active.
  struct BindingsManagersScope {
    ValueBindingsManager::Scope value_bindings_manager;
    LabelBindingsManager::Scope label_bindings_manager;
  };

 private:
  base::Optional<Block*> GetCatchBlock();
  void GenerateCatchBlock(base::Optional<Block*> catch_block);

  // {StackScope} records the stack height at creation time and reconstructs it
  // when being destructed by emitting a {DeleteRangeInstruction}, except for
  // the slots protected by {StackScope::Yield}. Calling {Yield(v)} deletes all
  // slots above the initial stack height except for the slots of {v}, which are
  // moved to form the only slots above the initial height and marks them to
  // survive destruction of the {StackScope}. A typical pattern is the
  // following:
  //
  // VisitResult result;
  // {
  //   StackScope stack_scope(this);
  //   // ... create temporary slots ...
  //   result = stack_scope.Yield(surviving_slots);
  // }
  class StackScope {
   public:
    explicit StackScope(ImplementationVisitor* visitor) : visitor_(visitor) {
      base_ = visitor_->assembler().CurrentStack().AboveTop();
    }
    VisitResult Yield(VisitResult result) {
      DCHECK(!closed_);
      closed_ = true;
      if (!result.IsOnStack()) {
        if (!visitor_->assembler().CurrentBlockIsComplete()) {
          visitor_->assembler().DropTo(base_);
        }
        return result;
      }
      DCHECK_LE(base_, result.stack_range().begin());
      DCHECK_LE(result.stack_range().end(),
                visitor_->assembler().CurrentStack().AboveTop());
      visitor_->assembler().DropTo(result.stack_range().end());
      visitor_->assembler().DeleteRange(
          StackRange{base_, result.stack_range().begin()});
      base_ = visitor_->assembler().CurrentStack().AboveTop();
      return VisitResult(result.type(), visitor_->assembler().TopRange(
                                            result.stack_range().Size()));
    }

    void Close() {
      DCHECK(!closed_);
      closed_ = true;
      if (!visitor_->assembler().CurrentBlockIsComplete()) {
        visitor_->assembler().DropTo(base_);
      }
    }

    ~StackScope() {
      if (closed_) {
        DCHECK_IMPLIES(
            !visitor_->assembler().CurrentBlockIsComplete(),
            base_ == visitor_->assembler().CurrentStack().AboveTop());
      } else {
        Close();
      }
    }

   private:
    ImplementationVisitor* visitor_;
    BottomOffset base_;
    bool closed_ = false;
  };

  class BreakContinueActivator {
   public:
    BreakContinueActivator(Block* break_block, Block* continue_block)
        : break_binding_{&LabelBindingsManager::Get(), "_break",
                         LocalLabel{break_block}},
          continue_binding_{&LabelBindingsManager::Get(), "_continue",
                            LocalLabel{continue_block}} {}

   private:
    Binding<LocalLabel> break_binding_;
    Binding<LocalLabel> continue_binding_;
  };

  base::Optional<Binding<LocalValue>*> TryLookupLocalValue(
      const std::string& name);
  base::Optional<Binding<LocalLabel>*> TryLookupLabel(const std::string& name);
  Binding<LocalLabel>* LookupLabel(const std::string& name);
  Block* LookupSimpleLabel(const std::string& name);
  template <class Container>
  Callable* LookupCallable(const QualifiedName& name,
                           const Container& declaration_container,
                           const TypeVector& types,
                           const std::vector<Binding<LocalLabel>*>& labels,
                           const TypeVector& specialization_types,
                           bool silence_errors = false);
  bool TestLookupCallable(const QualifiedName& name,
                          const TypeVector& parameter_types);

  template <class Container>
  Callable* LookupCallable(const QualifiedName& name,
                           const Container& declaration_container,
                           const Arguments& arguments,
                           const TypeVector& specialization_types);

  Method* LookupMethod(const std::string& name, LocationReference target,
                       const Arguments& arguments,
                       const TypeVector& specialization_types);

  const Type* GetCommonType(const Type* left, const Type* right);

  VisitResult GenerateCopy(const VisitResult& to_copy);

  void GenerateAssignToLocation(const LocationReference& reference,
                                const VisitResult& assignment_value);

  void AddCallParameter(Callable* callable, VisitResult parameter,
                        const Type* parameter_type,
                        std::vector<VisitResult>* converted_arguments,
                        StackRange* argument_range,
                        std::vector<std::string>* constexpr_arguments);

  VisitResult GenerateCall(Callable* callable,
                           base::Optional<LocationReference> this_parameter,
                           Arguments parameters,
                           const TypeVector& specialization_types = {},
                           bool tail_call = false);
  VisitResult GenerateCall(const QualifiedName& callable_name,
                           Arguments parameters,
                           const TypeVector& specialization_types = {},
                           bool tail_call = false);
  VisitResult GenerateCall(std::string callable_name, Arguments parameters,
                           const TypeVector& specialization_types = {},
                           bool tail_call = false) {
    return GenerateCall(QualifiedName(std::move(callable_name)),
                        std::move(parameters), specialization_types, tail_call);
  }
  VisitResult GeneratePointerCall(Expression* callee,
                                  const Arguments& parameters, bool tail_call);

  void GenerateBranch(const VisitResult& condition, Block* true_block,
                      Block* false_block);

  typedef std::function<VisitResult()> VisitResultGenerator;
  void GenerateExpressionBranch(VisitResultGenerator, Block* true_block,
                                Block* false_block);
  void GenerateExpressionBranch(Expression* expression, Block* true_block,
                                Block* false_block);

  void GenerateMacroFunctionDeclaration(std::ostream& o,
                                        const std::string& macro_prefix,
                                        Macro* macro);
  void GenerateFunctionDeclaration(std::ostream& o,
                                   const std::string& macro_prefix,
                                   const std::string& name,
                                   const Signature& signature,
                                   const NameVector& parameter_names);

  VisitResult GenerateImplicitConvert(const Type* destination_type,
                                      VisitResult source);

  StackRange GenerateLabelGoto(LocalLabel* label,
                               base::Optional<StackRange> arguments = {});

  std::vector<Binding<LocalLabel>*> LabelsFromIdentifiers(
      const std::vector<std::string>& names);

  StackRange LowerParameter(const Type* type, const std::string& parameter_name,
                            Stack<std::string>* lowered_parameters);

  void LowerLabelParameter(const Type* type, const std::string& parameter_name,
                           std::vector<std::string>* lowered_parameters);

  std::string ExternalLabelName(const std::string& label_name);
  std::string ExternalLabelParameterName(const std::string& label_name,
                                         size_t i);
  std::string ExternalParameterName(const std::string& name);

  std::ostream& source_out() {
    Callable* callable = CurrentCallable::Get();
    if (!callable || callable->ShouldGenerateExternalCode()) {
      return CurrentNamespace()->source_stream();
    } else {
      return null_stream_;
    }
  }
  std::ostream& header_out() {
    Callable* callable = CurrentCallable::Get();
    if (!callable || callable->ShouldGenerateExternalCode()) {
      return CurrentNamespace()->header_stream();
    } else {
      return null_stream_;
    }
  }
  CfgAssembler& assembler() { return *assembler_; }

  void SetReturnValue(VisitResult return_value) {
    base::Optional<VisitResult>& current_return_value =
        CurrentReturnValue::Get();
    DCHECK_IMPLIES(current_return_value, *current_return_value == return_value);
    current_return_value = std::move(return_value);
  }

  VisitResult GetAndClearReturnValue() {
    VisitResult return_value = *CurrentReturnValue::Get();
    CurrentReturnValue::Get() = base::nullopt;
    return return_value;
  }

  base::Optional<CfgAssembler> assembler_;
  NullOStream null_stream_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_IMPLEMENTATION_VISITOR_H_
