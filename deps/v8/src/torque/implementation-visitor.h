// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_IMPLEMENTATION_VISITOR_H_
#define V8_TORQUE_IMPLEMENTATION_VISITOR_H_

#include <string>

#include "src/base/macros.h"
#include "src/torque/ast.h"
#include "src/torque/file-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

struct LocationReference {
  LocationReference(Value* value, VisitResult base, VisitResult index)
      : value(value), base(base), index(index) {}
  Value* value;
  VisitResult base;
  VisitResult index;
};

class ImplementationVisitor : public FileVisitor {
 public:
  explicit ImplementationVisitor(GlobalContext& global_context)
      : FileVisitor(global_context), indent_(0), next_temp_(0) {}

  void Visit(Ast* ast) { Visit(ast->default_module()); }

  VisitResult Visit(Expression* expr);
  const Type* Visit(Statement* stmt);
  void Visit(Declaration* decl);

  VisitResult Visit(StructExpression* decl);

  LocationReference GetLocationReference(LocationExpression* location);
  LocationReference GetLocationReference(IdentifierExpression* expr) {
    return LocationReference(declarations()->LookupValue(expr->name), {}, {});
  }
  LocationReference GetLocationReference(FieldAccessExpression* expr);
  LocationReference GetLocationReference(ElementAccessExpression* expr) {
    return LocationReference({}, Visit(expr->array), Visit(expr->index));
  }

  std::string RValueFlattenStructs(VisitResult result);

  VisitResult GenerateFetchFromLocation(LocationReference reference) {
    const Value* value = reference.value;
    return VisitResult(value->type(), value);
  }
  VisitResult GenerateFetchFromLocation(LocationExpression* location,
                                        LocationReference reference);
  VisitResult GenerateFetchFromLocation(IdentifierExpression* expr,
                                        LocationReference reference) {
    return GenerateFetchFromLocation(reference);
  }
  VisitResult GenerateFetchFromLocation(FieldAccessExpression* expr,
                                        LocationReference reference);
  VisitResult GenerateFetchFromLocation(ElementAccessExpression* expr,
                                        LocationReference reference) {
    Arguments arguments;
    arguments.parameters = {reference.base, reference.index};
    return GenerateCall("[]", arguments);
  }

  VisitResult GetBuiltinCode(Builtin* builtin);

  VisitResult Visit(IdentifierExpression* expr);
  VisitResult Visit(FieldAccessExpression* expr) {
    return GenerateFetchFromLocation(expr, GetLocationReference(expr));
  }
  VisitResult Visit(ElementAccessExpression* expr) {
    return GenerateFetchFromLocation(expr, GetLocationReference(expr));
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

  const Type* Visit(TryLabelStatement* stmt);
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

  void GenerateIndent();

  class ScopedIndent {
   public:
    explicit ScopedIndent(ImplementationVisitor* visitor, bool new_lines = true)
        : new_lines_(new_lines), visitor_(visitor) {
      if (new_lines) visitor->GenerateIndent();
      visitor->source_out() << "{";
      if (new_lines) visitor->source_out() << "\n";
      visitor->indent_++;
    }
    ~ScopedIndent() {
      visitor_->indent_--;
      visitor_->GenerateIndent();
      visitor_->source_out() << "}";
      if (new_lines_) visitor_->source_out() << "\n";
    }

   private:
    bool new_lines_;
    ImplementationVisitor* visitor_;
  };

  Callable* LookupCall(const std::string& name, const Arguments& arguments,
                       const TypeVector& specialization_types);

  bool GenerateChangedVarFromControlSplit(const Variable* v, bool first = true);

  void GetFlattenedStructsVars(const Variable* base,
                               std::set<const Variable*>* vars);

  void GenerateChangedVarsFromControlSplit(AstNode* node);

  const Type* GetCommonType(const Type* left, const Type* right);

  VisitResult GenerateCopy(const VisitResult& to_copy);

  void GenerateAssignToVariable(Variable* var, VisitResult value);

  void GenerateAssignToLocation(LocationExpression* location,
                                const LocationReference& reference,
                                VisitResult assignment_value);

  void GenerateVariableDeclaration(const Variable* var);

  Variable* GeneratePredeclaredVariableDeclaration(
      const std::string& name,
      const base::Optional<VisitResult>& initialization);

  Variable* GenerateVariableDeclaration(
      AstNode* node, const std::string& name, bool is_const,
      const base::Optional<const Type*>& type,
      const base::Optional<VisitResult>& initialization = {});

  void GenerateParameter(const std::string& parameter_name);

  void GenerateParameterList(const NameVector& list, size_t first = 0);

  VisitResult GenerateCall(const std::string& callable_name,
                           Arguments parameters,
                           const TypeVector& specialization_types = {},
                           bool tail_call = false);
  VisitResult GeneratePointerCall(Expression* callee,
                                  const Arguments& parameters, bool tail_call);

  bool GenerateLabeledStatementBlocks(
      const std::vector<Statement*>& blocks,
      const std::vector<Label*>& statement_labels, Label* merge_label);

  void GenerateBranch(const VisitResult& condition, Label* true_label,
                      Label* false_label);

  bool GenerateExpressionBranch(Expression* expression,
                                const std::vector<Label*>& statement_labels,
                                const std::vector<Statement*>& statement_blocks,
                                Label* merge_label);

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

  std::string NewTempVariable();

  std::string GenerateNewTempVariable(const Type* type);

  void GenerateLabelDefinition(Label* label, AstNode* node = nullptr);

  void GenerateLabelBind(Label* label);

  void GenerateLabelGoto(Label* label);

  std::vector<Label*> LabelsFromIdentifiers(
      const std::vector<std::string>& names);

  std::ostream& source_out() { return module_->source_stream(); }

  std::ostream& header_out() { return module_->header_stream(); }

  size_t indent_;
  int32_t next_temp_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_IMPLEMENTATION_VISITOR_H_
