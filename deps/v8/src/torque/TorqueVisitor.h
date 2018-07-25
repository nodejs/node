// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_TORQUE_TORQUEVISITOR_H_
#define V8_TORQUE_TORQUEVISITOR_H_

// Generated from Torque.g4 by ANTLR 4.7.1

#pragma once

#include "./antlr4-runtime.h"
#include "TorqueParser.h"

/**
 * This class defines an abstract visitor for a parse tree
 * produced by TorqueParser.
 */
class TorqueVisitor : public antlr4::tree::AbstractParseTreeVisitor {
 public:
  /**
   * Visit parse trees produced by TorqueParser.
   */
  virtual antlrcpp::Any visitType(TorqueParser::TypeContext* context) = 0;

  virtual antlrcpp::Any visitTypeList(
      TorqueParser::TypeListContext* context) = 0;

  virtual antlrcpp::Any visitGenericSpecializationTypeList(
      TorqueParser::GenericSpecializationTypeListContext* context) = 0;

  virtual antlrcpp::Any visitOptionalGenericTypeList(
      TorqueParser::OptionalGenericTypeListContext* context) = 0;

  virtual antlrcpp::Any visitTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* context) = 0;

  virtual antlrcpp::Any visitLabelParameter(
      TorqueParser::LabelParameterContext* context) = 0;

  virtual antlrcpp::Any visitOptionalType(
      TorqueParser::OptionalTypeContext* context) = 0;

  virtual antlrcpp::Any visitOptionalLabelList(
      TorqueParser::OptionalLabelListContext* context) = 0;

  virtual antlrcpp::Any visitOptionalOtherwise(
      TorqueParser::OptionalOtherwiseContext* context) = 0;

  virtual antlrcpp::Any visitParameter(
      TorqueParser::ParameterContext* context) = 0;

  virtual antlrcpp::Any visitParameterList(
      TorqueParser::ParameterListContext* context) = 0;

  virtual antlrcpp::Any visitLabelDeclaration(
      TorqueParser::LabelDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitExpression(
      TorqueParser::ExpressionContext* context) = 0;

  virtual antlrcpp::Any visitConditionalExpression(
      TorqueParser::ConditionalExpressionContext* context) = 0;

  virtual antlrcpp::Any visitLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* context) = 0;

  virtual antlrcpp::Any visitLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* context) = 0;

  virtual antlrcpp::Any visitBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* context) = 0;

  virtual antlrcpp::Any visitEqualityExpression(
      TorqueParser::EqualityExpressionContext* context) = 0;

  virtual antlrcpp::Any visitRelationalExpression(
      TorqueParser::RelationalExpressionContext* context) = 0;

  virtual antlrcpp::Any visitShiftExpression(
      TorqueParser::ShiftExpressionContext* context) = 0;

  virtual antlrcpp::Any visitAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* context) = 0;

  virtual antlrcpp::Any visitMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* context) = 0;

  virtual antlrcpp::Any visitUnaryExpression(
      TorqueParser::UnaryExpressionContext* context) = 0;

  virtual antlrcpp::Any visitLocationExpression(
      TorqueParser::LocationExpressionContext* context) = 0;

  virtual antlrcpp::Any visitIncrementDecrement(
      TorqueParser::IncrementDecrementContext* context) = 0;

  virtual antlrcpp::Any visitAssignment(
      TorqueParser::AssignmentContext* context) = 0;

  virtual antlrcpp::Any visitAssignmentExpression(
      TorqueParser::AssignmentExpressionContext* context) = 0;

  virtual antlrcpp::Any visitPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* context) = 0;

  virtual antlrcpp::Any visitForInitialization(
      TorqueParser::ForInitializationContext* context) = 0;

  virtual antlrcpp::Any visitForLoop(TorqueParser::ForLoopContext* context) = 0;

  virtual antlrcpp::Any visitRangeSpecifier(
      TorqueParser::RangeSpecifierContext* context) = 0;

  virtual antlrcpp::Any visitForOfRange(
      TorqueParser::ForOfRangeContext* context) = 0;

  virtual antlrcpp::Any visitForOfLoop(
      TorqueParser::ForOfLoopContext* context) = 0;

  virtual antlrcpp::Any visitArgument(
      TorqueParser::ArgumentContext* context) = 0;

  virtual antlrcpp::Any visitArgumentList(
      TorqueParser::ArgumentListContext* context) = 0;

  virtual antlrcpp::Any visitHelperCall(
      TorqueParser::HelperCallContext* context) = 0;

  virtual antlrcpp::Any visitLabelReference(
      TorqueParser::LabelReferenceContext* context) = 0;

  virtual antlrcpp::Any visitVariableDeclaration(
      TorqueParser::VariableDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* context) = 0;

  virtual antlrcpp::Any visitHelperCallStatement(
      TorqueParser::HelperCallStatementContext* context) = 0;

  virtual antlrcpp::Any visitExpressionStatement(
      TorqueParser::ExpressionStatementContext* context) = 0;

  virtual antlrcpp::Any visitIfStatement(
      TorqueParser::IfStatementContext* context) = 0;

  virtual antlrcpp::Any visitWhileLoop(
      TorqueParser::WhileLoopContext* context) = 0;

  virtual antlrcpp::Any visitReturnStatement(
      TorqueParser::ReturnStatementContext* context) = 0;

  virtual antlrcpp::Any visitBreakStatement(
      TorqueParser::BreakStatementContext* context) = 0;

  virtual antlrcpp::Any visitContinueStatement(
      TorqueParser::ContinueStatementContext* context) = 0;

  virtual antlrcpp::Any visitGotoStatement(
      TorqueParser::GotoStatementContext* context) = 0;

  virtual antlrcpp::Any visitHandlerWithStatement(
      TorqueParser::HandlerWithStatementContext* context) = 0;

  virtual antlrcpp::Any visitTryCatch(
      TorqueParser::TryCatchContext* context) = 0;

  virtual antlrcpp::Any visitDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* context) = 0;

  virtual antlrcpp::Any visitStatement(
      TorqueParser::StatementContext* context) = 0;

  virtual antlrcpp::Any visitStatementList(
      TorqueParser::StatementListContext* context) = 0;

  virtual antlrcpp::Any visitStatementScope(
      TorqueParser::StatementScopeContext* context) = 0;

  virtual antlrcpp::Any visitStatementBlock(
      TorqueParser::StatementBlockContext* context) = 0;

  virtual antlrcpp::Any visitHelperBody(
      TorqueParser::HelperBodyContext* context) = 0;

  virtual antlrcpp::Any visitExtendsDeclaration(
      TorqueParser::ExtendsDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitGeneratesDeclaration(
      TorqueParser::GeneratesDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitConstexprDeclaration(
      TorqueParser::ConstexprDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitTypeDeclaration(
      TorqueParser::TypeDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* context) = 0;

  virtual antlrcpp::Any visitExternalMacro(
      TorqueParser::ExternalMacroContext* context) = 0;

  virtual antlrcpp::Any visitExternalRuntime(
      TorqueParser::ExternalRuntimeContext* context) = 0;

  virtual antlrcpp::Any visitBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitGenericSpecialization(
      TorqueParser::GenericSpecializationContext* context) = 0;

  virtual antlrcpp::Any visitMacroDeclaration(
      TorqueParser::MacroDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitConstDeclaration(
      TorqueParser::ConstDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitDeclaration(
      TorqueParser::DeclarationContext* context) = 0;

  virtual antlrcpp::Any visitModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* context) = 0;

  virtual antlrcpp::Any visitFile(TorqueParser::FileContext* context) = 0;
};

#endif  // V8_TORQUE_TORQUEVISITOR_H_
