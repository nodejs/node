// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_TORQUE_TORQUEBASELISTENER_H_
#define V8_TORQUE_TORQUEBASELISTENER_H_

// Generated from Torque.g4 by ANTLR 4.7.1

#pragma once

#include "./antlr4-runtime.h"
#include "TorqueListener.h"

/**
 * This class provides an empty implementation of TorqueListener,
 * which can be extended to create a listener which only needs to handle a
 * subset of the available methods.
 */
class TorqueBaseListener : public TorqueListener {
 public:
  void enterType(TorqueParser::TypeContext* /*ctx*/) override {}
  void exitType(TorqueParser::TypeContext* /*ctx*/) override {}

  void enterTypeList(TorqueParser::TypeListContext* /*ctx*/) override {}
  void exitTypeList(TorqueParser::TypeListContext* /*ctx*/) override {}

  void enterGenericSpecializationTypeList(
      TorqueParser::GenericSpecializationTypeListContext* /*ctx*/) override {}
  void exitGenericSpecializationTypeList(
      TorqueParser::GenericSpecializationTypeListContext* /*ctx*/) override {}

  void enterOptionalGenericTypeList(
      TorqueParser::OptionalGenericTypeListContext* /*ctx*/) override {}
  void exitOptionalGenericTypeList(
      TorqueParser::OptionalGenericTypeListContext* /*ctx*/) override {}

  void enterTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* /*ctx*/) override {}
  void exitTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* /*ctx*/) override {}

  void enterLabelParameter(
      TorqueParser::LabelParameterContext* /*ctx*/) override {}
  void exitLabelParameter(
      TorqueParser::LabelParameterContext* /*ctx*/) override {}

  void enterOptionalType(TorqueParser::OptionalTypeContext* /*ctx*/) override {}
  void exitOptionalType(TorqueParser::OptionalTypeContext* /*ctx*/) override {}

  void enterOptionalLabelList(
      TorqueParser::OptionalLabelListContext* /*ctx*/) override {}
  void exitOptionalLabelList(
      TorqueParser::OptionalLabelListContext* /*ctx*/) override {}

  void enterOptionalOtherwise(
      TorqueParser::OptionalOtherwiseContext* /*ctx*/) override {}
  void exitOptionalOtherwise(
      TorqueParser::OptionalOtherwiseContext* /*ctx*/) override {}

  void enterParameter(TorqueParser::ParameterContext* /*ctx*/) override {}
  void exitParameter(TorqueParser::ParameterContext* /*ctx*/) override {}

  void enterParameterList(
      TorqueParser::ParameterListContext* /*ctx*/) override {}
  void exitParameterList(TorqueParser::ParameterListContext* /*ctx*/) override {
  }

  void enterLabelDeclaration(
      TorqueParser::LabelDeclarationContext* /*ctx*/) override {}
  void exitLabelDeclaration(
      TorqueParser::LabelDeclarationContext* /*ctx*/) override {}

  void enterExpression(TorqueParser::ExpressionContext* /*ctx*/) override {}
  void exitExpression(TorqueParser::ExpressionContext* /*ctx*/) override {}

  void enterConditionalExpression(
      TorqueParser::ConditionalExpressionContext* /*ctx*/) override {}
  void exitConditionalExpression(
      TorqueParser::ConditionalExpressionContext* /*ctx*/) override {}

  void enterLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* /*ctx*/) override {}
  void exitLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* /*ctx*/) override {}

  void enterLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* /*ctx*/) override {}
  void exitLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* /*ctx*/) override {}

  void enterBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* /*ctx*/) override {}
  void exitBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* /*ctx*/) override {}

  void enterEqualityExpression(
      TorqueParser::EqualityExpressionContext* /*ctx*/) override {}
  void exitEqualityExpression(
      TorqueParser::EqualityExpressionContext* /*ctx*/) override {}

  void enterRelationalExpression(
      TorqueParser::RelationalExpressionContext* /*ctx*/) override {}
  void exitRelationalExpression(
      TorqueParser::RelationalExpressionContext* /*ctx*/) override {}

  void enterShiftExpression(
      TorqueParser::ShiftExpressionContext* /*ctx*/) override {}
  void exitShiftExpression(
      TorqueParser::ShiftExpressionContext* /*ctx*/) override {}

  void enterAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* /*ctx*/) override {}
  void exitAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* /*ctx*/) override {}

  void enterMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* /*ctx*/) override {}
  void exitMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* /*ctx*/) override {}

  void enterUnaryExpression(
      TorqueParser::UnaryExpressionContext* /*ctx*/) override {}
  void exitUnaryExpression(
      TorqueParser::UnaryExpressionContext* /*ctx*/) override {}

  void enterLocationExpression(
      TorqueParser::LocationExpressionContext* /*ctx*/) override {}
  void exitLocationExpression(
      TorqueParser::LocationExpressionContext* /*ctx*/) override {}

  void enterIncrementDecrement(
      TorqueParser::IncrementDecrementContext* /*ctx*/) override {}
  void exitIncrementDecrement(
      TorqueParser::IncrementDecrementContext* /*ctx*/) override {}

  void enterAssignment(TorqueParser::AssignmentContext* /*ctx*/) override {}
  void exitAssignment(TorqueParser::AssignmentContext* /*ctx*/) override {}

  void enterAssignmentExpression(
      TorqueParser::AssignmentExpressionContext* /*ctx*/) override {}
  void exitAssignmentExpression(
      TorqueParser::AssignmentExpressionContext* /*ctx*/) override {}

  void enterPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* /*ctx*/) override {}
  void exitPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* /*ctx*/) override {}

  void enterForInitialization(
      TorqueParser::ForInitializationContext* /*ctx*/) override {}
  void exitForInitialization(
      TorqueParser::ForInitializationContext* /*ctx*/) override {}

  void enterForLoop(TorqueParser::ForLoopContext* /*ctx*/) override {}
  void exitForLoop(TorqueParser::ForLoopContext* /*ctx*/) override {}

  void enterRangeSpecifier(
      TorqueParser::RangeSpecifierContext* /*ctx*/) override {}
  void exitRangeSpecifier(
      TorqueParser::RangeSpecifierContext* /*ctx*/) override {}

  void enterForOfRange(TorqueParser::ForOfRangeContext* /*ctx*/) override {}
  void exitForOfRange(TorqueParser::ForOfRangeContext* /*ctx*/) override {}

  void enterForOfLoop(TorqueParser::ForOfLoopContext* /*ctx*/) override {}
  void exitForOfLoop(TorqueParser::ForOfLoopContext* /*ctx*/) override {}

  void enterArgument(TorqueParser::ArgumentContext* /*ctx*/) override {}
  void exitArgument(TorqueParser::ArgumentContext* /*ctx*/) override {}

  void enterArgumentList(TorqueParser::ArgumentListContext* /*ctx*/) override {}
  void exitArgumentList(TorqueParser::ArgumentListContext* /*ctx*/) override {}

  void enterHelperCall(TorqueParser::HelperCallContext* /*ctx*/) override {}
  void exitHelperCall(TorqueParser::HelperCallContext* /*ctx*/) override {}

  void enterLabelReference(
      TorqueParser::LabelReferenceContext* /*ctx*/) override {}
  void exitLabelReference(
      TorqueParser::LabelReferenceContext* /*ctx*/) override {}

  void enterVariableDeclaration(
      TorqueParser::VariableDeclarationContext* /*ctx*/) override {}
  void exitVariableDeclaration(
      TorqueParser::VariableDeclarationContext* /*ctx*/) override {}

  void enterVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* /*ctx*/)
      override {}
  void exitVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* /*ctx*/)
      override {}

  void enterHelperCallStatement(
      TorqueParser::HelperCallStatementContext* /*ctx*/) override {}
  void exitHelperCallStatement(
      TorqueParser::HelperCallStatementContext* /*ctx*/) override {}

  void enterExpressionStatement(
      TorqueParser::ExpressionStatementContext* /*ctx*/) override {}
  void exitExpressionStatement(
      TorqueParser::ExpressionStatementContext* /*ctx*/) override {}

  void enterIfStatement(TorqueParser::IfStatementContext* /*ctx*/) override {}
  void exitIfStatement(TorqueParser::IfStatementContext* /*ctx*/) override {}

  void enterWhileLoop(TorqueParser::WhileLoopContext* /*ctx*/) override {}
  void exitWhileLoop(TorqueParser::WhileLoopContext* /*ctx*/) override {}

  void enterReturnStatement(
      TorqueParser::ReturnStatementContext* /*ctx*/) override {}
  void exitReturnStatement(
      TorqueParser::ReturnStatementContext* /*ctx*/) override {}

  void enterBreakStatement(
      TorqueParser::BreakStatementContext* /*ctx*/) override {}
  void exitBreakStatement(
      TorqueParser::BreakStatementContext* /*ctx*/) override {}

  void enterContinueStatement(
      TorqueParser::ContinueStatementContext* /*ctx*/) override {}
  void exitContinueStatement(
      TorqueParser::ContinueStatementContext* /*ctx*/) override {}

  void enterGotoStatement(
      TorqueParser::GotoStatementContext* /*ctx*/) override {}
  void exitGotoStatement(TorqueParser::GotoStatementContext* /*ctx*/) override {
  }

  void enterHandlerWithStatement(
      TorqueParser::HandlerWithStatementContext* /*ctx*/) override {}
  void exitHandlerWithStatement(
      TorqueParser::HandlerWithStatementContext* /*ctx*/) override {}

  void enterTryCatch(TorqueParser::TryCatchContext* /*ctx*/) override {}
  void exitTryCatch(TorqueParser::TryCatchContext* /*ctx*/) override {}

  void enterDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* /*ctx*/) override {}
  void exitDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* /*ctx*/) override {}

  void enterStatement(TorqueParser::StatementContext* /*ctx*/) override {}
  void exitStatement(TorqueParser::StatementContext* /*ctx*/) override {}

  void enterStatementList(
      TorqueParser::StatementListContext* /*ctx*/) override {}
  void exitStatementList(TorqueParser::StatementListContext* /*ctx*/) override {
  }

  void enterStatementScope(
      TorqueParser::StatementScopeContext* /*ctx*/) override {}
  void exitStatementScope(
      TorqueParser::StatementScopeContext* /*ctx*/) override {}

  void enterStatementBlock(
      TorqueParser::StatementBlockContext* /*ctx*/) override {}
  void exitStatementBlock(
      TorqueParser::StatementBlockContext* /*ctx*/) override {}

  void enterHelperBody(TorqueParser::HelperBodyContext* /*ctx*/) override {}
  void exitHelperBody(TorqueParser::HelperBodyContext* /*ctx*/) override {}

  void enterExtendsDeclaration(
      TorqueParser::ExtendsDeclarationContext* /*ctx*/) override {}
  void exitExtendsDeclaration(
      TorqueParser::ExtendsDeclarationContext* /*ctx*/) override {}

  void enterGeneratesDeclaration(
      TorqueParser::GeneratesDeclarationContext* /*ctx*/) override {}
  void exitGeneratesDeclaration(
      TorqueParser::GeneratesDeclarationContext* /*ctx*/) override {}

  void enterConstexprDeclaration(
      TorqueParser::ConstexprDeclarationContext* /*ctx*/) override {}
  void exitConstexprDeclaration(
      TorqueParser::ConstexprDeclarationContext* /*ctx*/) override {}

  void enterTypeDeclaration(
      TorqueParser::TypeDeclarationContext* /*ctx*/) override {}
  void exitTypeDeclaration(
      TorqueParser::TypeDeclarationContext* /*ctx*/) override {}

  void enterExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* /*ctx*/) override {}
  void exitExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* /*ctx*/) override {}

  void enterExternalMacro(
      TorqueParser::ExternalMacroContext* /*ctx*/) override {}
  void exitExternalMacro(TorqueParser::ExternalMacroContext* /*ctx*/) override {
  }

  void enterExternalRuntime(
      TorqueParser::ExternalRuntimeContext* /*ctx*/) override {}
  void exitExternalRuntime(
      TorqueParser::ExternalRuntimeContext* /*ctx*/) override {}

  void enterBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* /*ctx*/) override {}
  void exitBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* /*ctx*/) override {}

  void enterGenericSpecialization(
      TorqueParser::GenericSpecializationContext* /*ctx*/) override {}
  void exitGenericSpecialization(
      TorqueParser::GenericSpecializationContext* /*ctx*/) override {}

  void enterMacroDeclaration(
      TorqueParser::MacroDeclarationContext* /*ctx*/) override {}
  void exitMacroDeclaration(
      TorqueParser::MacroDeclarationContext* /*ctx*/) override {}

  void enterConstDeclaration(
      TorqueParser::ConstDeclarationContext* /*ctx*/) override {}
  void exitConstDeclaration(
      TorqueParser::ConstDeclarationContext* /*ctx*/) override {}

  void enterDeclaration(TorqueParser::DeclarationContext* /*ctx*/) override {}
  void exitDeclaration(TorqueParser::DeclarationContext* /*ctx*/) override {}

  void enterModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* /*ctx*/) override {}
  void exitModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* /*ctx*/) override {}

  void enterFile(TorqueParser::FileContext* /*ctx*/) override {}
  void exitFile(TorqueParser::FileContext* /*ctx*/) override {}

  void enterEveryRule(antlr4::ParserRuleContext* /*ctx*/) override {}
  void exitEveryRule(antlr4::ParserRuleContext* /*ctx*/) override {}
  void visitTerminal(antlr4::tree::TerminalNode* /*node*/) override {}
  void visitErrorNode(antlr4::tree::ErrorNode* /*node*/) override {}
};

#endif  // V8_TORQUE_TORQUEBASELISTENER_H_
