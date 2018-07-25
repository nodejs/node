// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_AST_GENERATOR_H_
#define V8_TORQUE_AST_GENERATOR_H_

#include "src/torque/TorqueBaseVisitor.h"
#include "src/torque/ast.h"
#include "src/torque/global-context.h"

namespace v8 {
namespace internal {
namespace torque {

class AstGenerator : public TorqueBaseVisitor {
 public:
  antlrcpp::Any visitParameterList(
      TorqueParser::ParameterListContext* context) override;

  antlrcpp::Any visitTypeList(TorqueParser::TypeListContext* context) override;

  antlrcpp::Any visitTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* context) override;

  antlrcpp::Any visitModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* context) override;

  antlrcpp::Any visitMacroDeclaration(
      TorqueParser::MacroDeclarationContext* context) override;

  antlrcpp::Any visitBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* context) override;

  antlrcpp::Any visitExternalMacro(
      TorqueParser::ExternalMacroContext* context) override;

  antlrcpp::Any visitExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* context) override;

  antlrcpp::Any visitExternalRuntime(
      TorqueParser::ExternalRuntimeContext* context) override;

  antlrcpp::Any visitGenericSpecialization(
      TorqueParser::GenericSpecializationContext* context) override;

  antlrcpp::Any visitConstDeclaration(
      TorqueParser::ConstDeclarationContext* context) override;

  antlrcpp::Any visitTypeDeclaration(
      TorqueParser::TypeDeclarationContext* context) override;

  antlrcpp::Any visitVariableDeclaration(
      TorqueParser::VariableDeclarationContext* context) override;

  antlrcpp::Any visitVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* context)
      override;

  antlrcpp::Any visitHelperCall(
      TorqueParser::HelperCallContext* context) override;

  antlrcpp::Any visitHelperCallStatement(
      TorqueParser::HelperCallStatementContext* context) override;

  antlrcpp::Any visitConditionalExpression(
      TorqueParser::ConditionalExpressionContext* context) override;

  antlrcpp::Any visitLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* context) override;

  antlrcpp::Any visitLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* context) override;

  antlrcpp::Any visitBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* context) override;

  antlrcpp::Any visitEqualityExpression(
      TorqueParser::EqualityExpressionContext* context) override;

  antlrcpp::Any visitRelationalExpression(
      TorqueParser::RelationalExpressionContext* context) override;

  antlrcpp::Any visitShiftExpression(
      TorqueParser::ShiftExpressionContext* context) override;

  antlrcpp::Any visitAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* context) override;

  antlrcpp::Any visitMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* context) override;

  antlrcpp::Any visitUnaryExpression(
      TorqueParser::UnaryExpressionContext* context) override;

  antlrcpp::Any visitLocationExpression(
      TorqueParser::LocationExpressionContext* locationExpression) override;

  antlrcpp::Any visitIncrementDecrement(
      TorqueParser::IncrementDecrementContext* context) override;

  antlrcpp::Any visitAssignment(
      TorqueParser::AssignmentContext* context) override;

  antlrcpp::Any visitPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* context) override;

  antlrcpp::Any visitTryCatch(TorqueParser::TryCatchContext* context) override;

  antlrcpp::Any visitStatementScope(
      TorqueParser::StatementScopeContext* context) override;

  antlrcpp::Any visitExpressionStatement(
      TorqueParser::ExpressionStatementContext* context) override;

  antlrcpp::Any visitReturnStatement(
      TorqueParser::ReturnStatementContext* context) override;

  antlrcpp::Any visitGotoStatement(
      TorqueParser::GotoStatementContext* context) override;

  antlrcpp::Any visitIfStatement(
      TorqueParser::IfStatementContext* context) override;

  antlrcpp::Any visitWhileLoop(
      TorqueParser::WhileLoopContext* context) override;

  antlrcpp::Any visitBreakStatement(
      TorqueParser::BreakStatementContext* context) override;

  antlrcpp::Any visitContinueStatement(
      TorqueParser::ContinueStatementContext* context) override;

  antlrcpp::Any visitForLoop(TorqueParser::ForLoopContext* context) override;

  antlrcpp::Any visitForOfLoop(
      TorqueParser::ForOfLoopContext* context) override;

  antlrcpp::Any visitDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* context) override;

  antlrcpp::Any aggregateResult(antlrcpp::Any aggregate,
                                const antlrcpp::Any& nextResult) override {
    if (aggregate.isNull())
      return std::move(const_cast<antlrcpp::Any&>(nextResult));
    if (nextResult.isNull()) return aggregate;
    UNREACHABLE();
    return {};
  }

  template <class T>
  T* RegisterNode(T* node) {
    ast_.AddNode(std::unique_ptr<AstNode>(node));
    return node;
  }

  LabelAndTypesVector GetOptionalLabelAndTypeList(
      TorqueParser::OptionalLabelListContext* context);
  TypeExpression* GetType(TorqueParser::TypeContext* context);
  TypeExpression* GetOptionalType(TorqueParser::OptionalTypeContext* context);
  std::vector<TypeExpression*> GetTypeVector(
      TorqueParser::TypeListContext* type_list);

  ParameterList GetOptionalParameterList(
      TorqueParser::ParameterListContext* context);

  Statement* GetOptionalHelperBody(TorqueParser::HelperBodyContext* context);

  void visitSourceFile(SourceFileContext* context);

  SourcePosition Pos(antlr4::ParserRuleContext* context);

  Ast GetAst() && { return std::move(ast_); }

 private:
  Ast ast_;
  SourceId current_source_file_;
  SourceFileContext* source_file_context_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_AST_GENERATOR_H_
