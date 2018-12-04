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
  // For error reporting.
  const std::string& temporary_description() const {
    DCHECK(IsTemporary());
    return *temporary_description_;
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
  base::Optional<std::string> eval_function_;
  base::Optional<std::string> assign_function_;
  VisitResultVector call_arguments_;

  LocationReference() = default;
};

class ImplementationVisitor : public FileVisitor {
 public:
  explicit ImplementationVisitor(GlobalContext& global_context)
      : FileVisitor(global_context) {}

  void Visit(Ast* ast) { Visit(ast->default_module()); }

  VisitResult Visit(Expression* expr);
  const Type* Visit(Statement* stmt);
  void Visit(Declaration* decl);

  VisitResult Visit(StructExpression* decl);

  LocationReference GetLocationReference(Expression* location);
  LocationReference GetLocationReference(IdentifierExpression* expr);
  LocationReference GetLocationReference(FieldAccessExpression* expr);
  LocationReference GetLocationReference(ElementAccessExpression* expr);

  VisitResult GenerateFetchFromLocation(const LocationReference& reference);

  VisitResult GetBuiltinCode(Builtin* builtin);

  VisitResult Visit(IdentifierExpression* expr);
  VisitResult Visit(FieldAccessExpression* expr) {
    StackScope scope(this);
    return scope.Yield(GenerateFetchFromLocation(GetLocationReference(expr)));
  }
  VisitResult Visit(ElementAccessExpression* expr) {
    StackScope scope(this);
    return scope.Yield(GenerateFetchFromLocation(GetLocationReference(expr)));
  }

  void Visit(ModuleDeclaration* decl);
  void Visit(DefaultModuleDeclaration* decl) {
    Visit(implicit_cast<ModuleDeclaration*>(decl));
  }
  void Visit(ExplicitModuleDeclaration* decl) {
    Visit(implicit_cast<ModuleDeclaration*>(decl));
  }
  void Visit(TypeDeclaration* decl) {}
  void Visit(TypeAliasDeclaration* decl) {}
  void Visit(ExternConstDeclaration* decl) {}
  void Visit(StructDeclaration* decl);
  void Visit(StandardDeclaration* decl);
  void Visit(GenericDeclaration* decl) {}
  void Visit(SpecializationDeclaration* decl);

  void Visit(TorqueMacroDeclaration* decl, const Signature& signature,
             Statement* body);
  void Visit(TorqueBuiltinDeclaration* decl, const Signature& signature,
             Statement* body);
  void Visit(ExternalMacroDeclaration* decl, const Signature& signature,
             Statement* body) {}
  void Visit(ExternalBuiltinDeclaration* decl, const Signature& signature,
             Statement* body) {}
  void Visit(ExternalRuntimeDeclaration* decl, const Signature& signature,
             Statement* body) {}
  void Visit(CallableNode* decl, const Signature& signature, Statement* body);
  void Visit(ConstDeclaration* decl);

  VisitResult Visit(CallExpression* expr, bool is_tail = false);
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

  const Type* Visit(ReturnStatement* stmt);
  const Type* Visit(GotoStatement* stmt);
  const Type* Visit(IfStatement* stmt);
  const Type* Visit(WhileStatement* stmt);
  const Type* Visit(BreakStatement* stmt);
  const Type* Visit(ContinueStatement* stmt);
  const Type* Visit(ForLoopStatement* stmt);
  const Type* Visit(VarDeclarationStatement* stmt);
  const Type* Visit(ForOfLoopStatement* stmt);
  const Type* Visit(BlockStatement* block);
  const Type* Visit(ExpressionStatement* stmt);
  const Type* Visit(DebugStatement* stmt);
  const Type* Visit(AssertStatement* stmt);

  void BeginModuleFile(Module* module);
  void EndModuleFile(Module* module);

  void GenerateImplementation(const std::string& dir, Module* module);

 private:
  std::string GetBaseAssemblerName(Module* module);

  std::string GetDSLAssemblerName(Module* module);

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
      DCHECK(!yield_called_);
      yield_called_ = true;
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

    ~StackScope() {
      if (yield_called_) {
        DCHECK_IMPLIES(
            !visitor_->assembler().CurrentBlockIsComplete(),
            base_ == visitor_->assembler().CurrentStack().AboveTop());
      } else if (!visitor_->assembler().CurrentBlockIsComplete()) {
        visitor_->assembler().DropTo(base_);
      }
    }

   private:
    ImplementationVisitor* visitor_;
    BottomOffset base_;
    bool yield_called_ = false;
  };

  Callable* LookupCall(const std::string& name, const Arguments& arguments,
                       const TypeVector& specialization_types);

  const Type* GetCommonType(const Type* left, const Type* right);

  VisitResult GenerateCopy(const VisitResult& to_copy);

  void GenerateAssignToLocation(const LocationReference& reference,
                                const VisitResult& assignment_value);

  VisitResult GenerateCall(const std::string& callable_name,
                           Arguments parameters,
                           const TypeVector& specialization_types = {},
                           bool tail_call = false);
  VisitResult GeneratePointerCall(Expression* callee,
                                  const Arguments& parameters, bool tail_call);

  bool GenerateLabeledStatementBlocks(
      const std::vector<Statement*>& blocks,
      const std::vector<Label*>& statement_labels, Block* merge_block);

  void GenerateBranch(const VisitResult& condition, Label* true_label,
                      Label* false_label);

  bool GenerateExpressionBranch(Expression* expression,
                                const std::vector<Label*>& statement_labels,
                                const std::vector<Statement*>& statement_blocks,
                                Block* merge_block);

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

  void Specialize(const SpecializationKey& key, CallableNode* callable,
                  const CallableNodeSignature* signature,
                  Statement* body) override {
    Declarations::GenericScopeActivator scope(declarations(), key);
    Visit(callable, MakeSignature(signature), body);
  }

  void CreateBlockForLabel(Label* label, Stack<const Type*> stack);

  void GenerateLabelBind(Label* label);

  StackRange GenerateLabelGoto(Label* label,
                               base::Optional<StackRange> arguments = {});

  std::vector<Label*> LabelsFromIdentifiers(
      const std::vector<std::string>& names);

  StackRange LowerParameter(const Type* type, const std::string& parameter_name,
                            Stack<std::string>* lowered_parameters);

  std::string ExternalLabelParameterName(Label* label, size_t i);

  std::ostream& source_out() { return module_->source_stream(); }

  std::ostream& header_out() { return module_->header_stream(); }

  CfgAssembler& assembler() { return *assembler_; }

  void SetReturnValue(VisitResult return_value) {
    DCHECK_IMPLIES(return_value_, *return_value_ == return_value);
    return_value_ = std::move(return_value);
  }

  VisitResult GetAndClearReturnValue() {
    VisitResult return_value = *return_value_;
    return_value_ = base::nullopt;
    return return_value;
  }

  base::Optional<CfgAssembler> assembler_;
  base::Optional<VisitResult> return_value_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_IMPLEMENTATION_VISITOR_H_
