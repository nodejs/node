// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_TORQUE_TORQUELISTENER_H_
#define V8_TORQUE_TORQUELISTENER_H_

// Generated from Torque.g4 by ANTLR 4.7.1

#pragma once

#include "./antlr4-runtime.h"
#include "TorqueParser.h"

/**
 * This interface defines an abstract listener for a parse tree produced by
 * TorqueParser.
 */
class TorqueListener : public antlr4::tree::ParseTreeListener {
 public:
  virtual void enterType(TorqueParser::TypeContext* ctx) = 0;
  virtual void exitType(TorqueParser::TypeContext* ctx) = 0;

  virtual void enterTypeList(TorqueParser::TypeListContext* ctx) = 0;
  virtual void exitTypeList(TorqueParser::TypeListContext* ctx) = 0;

  virtual void enterGenericSpecializationTypeList(
      TorqueParser::GenericSpecializationTypeListContext* ctx) = 0;
  virtual void exitGenericSpecializationTypeList(
      TorqueParser::GenericSpecializationTypeListContext* ctx) = 0;

  virtual void enterOptionalGenericTypeList(
      TorqueParser::OptionalGenericTypeListContext* ctx) = 0;
  virtual void exitOptionalGenericTypeList(
      TorqueParser::OptionalGenericTypeListContext* ctx) = 0;

  virtual void enterTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* ctx) = 0;
  virtual void exitTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* ctx) = 0;

  virtual void enterLabelParameter(
      TorqueParser::LabelParameterContext* ctx) = 0;
  virtual void exitLabelParameter(TorqueParser::LabelParameterContext* ctx) = 0;

  virtual void enterOptionalType(TorqueParser::OptionalTypeContext* ctx) = 0;
  virtual void exitOptionalType(TorqueParser::OptionalTypeContext* ctx) = 0;

  virtual void enterOptionalLabelList(
      TorqueParser::OptionalLabelListContext* ctx) = 0;
  virtual void exitOptionalLabelList(
      TorqueParser::OptionalLabelListContext* ctx) = 0;

  virtual void enterOptionalOtherwise(
      TorqueParser::OptionalOtherwiseContext* ctx) = 0;
  virtual void exitOptionalOtherwise(
      TorqueParser::OptionalOtherwiseContext* ctx) = 0;

  virtual void enterParameter(TorqueParser::ParameterContext* ctx) = 0;
  virtual void exitParameter(TorqueParser::ParameterContext* ctx) = 0;

  virtual void enterParameterList(TorqueParser::ParameterListContext* ctx) = 0;
  virtual void exitParameterList(TorqueParser::ParameterListContext* ctx) = 0;

  virtual void enterLabelDeclaration(
      TorqueParser::LabelDeclarationContext* ctx) = 0;
  virtual void exitLabelDeclaration(
      TorqueParser::LabelDeclarationContext* ctx) = 0;

  virtual void enterExpression(TorqueParser::ExpressionContext* ctx) = 0;
  virtual void exitExpression(TorqueParser::ExpressionContext* ctx) = 0;

  virtual void enterConditionalExpression(
      TorqueParser::ConditionalExpressionContext* ctx) = 0;
  virtual void exitConditionalExpression(
      TorqueParser::ConditionalExpressionContext* ctx) = 0;

  virtual void enterLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* ctx) = 0;
  virtual void exitLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* ctx) = 0;

  virtual void enterLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* ctx) = 0;
  virtual void exitLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* ctx) = 0;

  virtual void enterBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* ctx) = 0;
  virtual void exitBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* ctx) = 0;

  virtual void enterEqualityExpression(
      TorqueParser::EqualityExpressionContext* ctx) = 0;
  virtual void exitEqualityExpression(
      TorqueParser::EqualityExpressionContext* ctx) = 0;

  virtual void enterRelationalExpression(
      TorqueParser::RelationalExpressionContext* ctx) = 0;
  virtual void exitRelationalExpression(
      TorqueParser::RelationalExpressionContext* ctx) = 0;

  virtual void enterShiftExpression(
      TorqueParser::ShiftExpressionContext* ctx) = 0;
  virtual void exitShiftExpression(
      TorqueParser::ShiftExpressionContext* ctx) = 0;

  virtual void enterAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* ctx) = 0;
  virtual void exitAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* ctx) = 0;

  virtual void enterMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* ctx) = 0;
  virtual void exitMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* ctx) = 0;

  virtual void enterUnaryExpression(
      TorqueParser::UnaryExpressionContext* ctx) = 0;
  virtual void exitUnaryExpression(
      TorqueParser::UnaryExpressionContext* ctx) = 0;

  virtual void enterLocationExpression(
      TorqueParser::LocationExpressionContext* ctx) = 0;
  virtual void exitLocationExpression(
      TorqueParser::LocationExpressionContext* ctx) = 0;

  virtual void enterIncrementDecrement(
      TorqueParser::IncrementDecrementContext* ctx) = 0;
  virtual void exitIncrementDecrement(
      TorqueParser::IncrementDecrementContext* ctx) = 0;

  virtual void enterAssignment(TorqueParser::AssignmentContext* ctx) = 0;
  virtual void exitAssignment(TorqueParser::AssignmentContext* ctx) = 0;

  virtual void enterAssignmentExpression(
      TorqueParser::AssignmentExpressionContext* ctx) = 0;
  virtual void exitAssignmentExpression(
      TorqueParser::AssignmentExpressionContext* ctx) = 0;

  virtual void enterPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* ctx) = 0;
  virtual void exitPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* ctx) = 0;

  virtual void enterForInitialization(
      TorqueParser::ForInitializationContext* ctx) = 0;
  virtual void exitForInitialization(
      TorqueParser::ForInitializationContext* ctx) = 0;

  virtual void enterForLoop(TorqueParser::ForLoopContext* ctx) = 0;
  virtual void exitForLoop(TorqueParser::ForLoopContext* ctx) = 0;

  virtual void enterRangeSpecifier(
      TorqueParser::RangeSpecifierContext* ctx) = 0;
  virtual void exitRangeSpecifier(TorqueParser::RangeSpecifierContext* ctx) = 0;

  virtual void enterForOfRange(TorqueParser::ForOfRangeContext* ctx) = 0;
  virtual void exitForOfRange(TorqueParser::ForOfRangeContext* ctx) = 0;

  virtual void enterForOfLoop(TorqueParser::ForOfLoopContext* ctx) = 0;
  virtual void exitForOfLoop(TorqueParser::ForOfLoopContext* ctx) = 0;

  virtual void enterArgument(TorqueParser::ArgumentContext* ctx) = 0;
  virtual void exitArgument(TorqueParser::ArgumentContext* ctx) = 0;

  virtual void enterArgumentList(TorqueParser::ArgumentListContext* ctx) = 0;
  virtual void exitArgumentList(TorqueParser::ArgumentListContext* ctx) = 0;

  virtual void enterHelperCall(TorqueParser::HelperCallContext* ctx) = 0;
  virtual void exitHelperCall(TorqueParser::HelperCallContext* ctx) = 0;

  virtual void enterLabelReference(
      TorqueParser::LabelReferenceContext* ctx) = 0;
  virtual void exitLabelReference(TorqueParser::LabelReferenceContext* ctx) = 0;

  virtual void enterVariableDeclaration(
      TorqueParser::VariableDeclarationContext* ctx) = 0;
  virtual void exitVariableDeclaration(
      TorqueParser::VariableDeclarationContext* ctx) = 0;

  virtual void enterVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* ctx) = 0;
  virtual void exitVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* ctx) = 0;

  virtual void enterHelperCallStatement(
      TorqueParser::HelperCallStatementContext* ctx) = 0;
  virtual void exitHelperCallStatement(
      TorqueParser::HelperCallStatementContext* ctx) = 0;

  virtual void enterExpressionStatement(
      TorqueParser::ExpressionStatementContext* ctx) = 0;
  virtual void exitExpressionStatement(
      TorqueParser::ExpressionStatementContext* ctx) = 0;

  virtual void enterIfStatement(TorqueParser::IfStatementContext* ctx) = 0;
  virtual void exitIfStatement(TorqueParser::IfStatementContext* ctx) = 0;

  virtual void enterWhileLoop(TorqueParser::WhileLoopContext* ctx) = 0;
  virtual void exitWhileLoop(TorqueParser::WhileLoopContext* ctx) = 0;

  virtual void enterReturnStatement(
      TorqueParser::ReturnStatementContext* ctx) = 0;
  virtual void exitReturnStatement(
      TorqueParser::ReturnStatementContext* ctx) = 0;

  virtual void enterBreakStatement(
      TorqueParser::BreakStatementContext* ctx) = 0;
  virtual void exitBreakStatement(TorqueParser::BreakStatementContext* ctx) = 0;

  virtual void enterContinueStatement(
      TorqueParser::ContinueStatementContext* ctx) = 0;
  virtual void exitContinueStatement(
      TorqueParser::ContinueStatementContext* ctx) = 0;

  virtual void enterGotoStatement(TorqueParser::GotoStatementContext* ctx) = 0;
  virtual void exitGotoStatement(TorqueParser::GotoStatementContext* ctx) = 0;

  virtual void enterHandlerWithStatement(
      TorqueParser::HandlerWithStatementContext* ctx) = 0;
  virtual void exitHandlerWithStatement(
      TorqueParser::HandlerWithStatementContext* ctx) = 0;

  virtual void enterTryCatch(TorqueParser::TryCatchContext* ctx) = 0;
  virtual void exitTryCatch(TorqueParser::TryCatchContext* ctx) = 0;

  virtual void enterDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* ctx) = 0;
  virtual void exitDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* ctx) = 0;

  virtual void enterStatement(TorqueParser::StatementContext* ctx) = 0;
  virtual void exitStatement(TorqueParser::StatementContext* ctx) = 0;

  virtual void enterStatementList(TorqueParser::StatementListContext* ctx) = 0;
  virtual void exitStatementList(TorqueParser::StatementListContext* ctx) = 0;

  virtual void enterStatementScope(
      TorqueParser::StatementScopeContext* ctx) = 0;
  virtual void exitStatementScope(TorqueParser::StatementScopeContext* ctx) = 0;

  virtual void enterStatementBlock(
      TorqueParser::StatementBlockContext* ctx) = 0;
  virtual void exitStatementBlock(TorqueParser::StatementBlockContext* ctx) = 0;

  virtual void enterHelperBody(TorqueParser::HelperBodyContext* ctx) = 0;
  virtual void exitHelperBody(TorqueParser::HelperBodyContext* ctx) = 0;

  virtual void enterExtendsDeclaration(
      TorqueParser::ExtendsDeclarationContext* ctx) = 0;
  virtual void exitExtendsDeclaration(
      TorqueParser::ExtendsDeclarationContext* ctx) = 0;

  virtual void enterGeneratesDeclaration(
      TorqueParser::GeneratesDeclarationContext* ctx) = 0;
  virtual void exitGeneratesDeclaration(
      TorqueParser::GeneratesDeclarationContext* ctx) = 0;

  virtual void enterConstexprDeclaration(
      TorqueParser::ConstexprDeclarationContext* ctx) = 0;
  virtual void exitConstexprDeclaration(
      TorqueParser::ConstexprDeclarationContext* ctx) = 0;

  virtual void enterTypeDeclaration(
      TorqueParser::TypeDeclarationContext* ctx) = 0;
  virtual void exitTypeDeclaration(
      TorqueParser::TypeDeclarationContext* ctx) = 0;

  virtual void enterExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* ctx) = 0;
  virtual void exitExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* ctx) = 0;

  virtual void enterExternalMacro(TorqueParser::ExternalMacroContext* ctx) = 0;
  virtual void exitExternalMacro(TorqueParser::ExternalMacroContext* ctx) = 0;

  virtual void enterExternalRuntime(
      TorqueParser::ExternalRuntimeContext* ctx) = 0;
  virtual void exitExternalRuntime(
      TorqueParser::ExternalRuntimeContext* ctx) = 0;

  virtual void enterBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* ctx) = 0;
  virtual void exitBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* ctx) = 0;

  virtual void enterGenericSpecialization(
      TorqueParser::GenericSpecializationContext* ctx) = 0;
  virtual void exitGenericSpecialization(
      TorqueParser::GenericSpecializationContext* ctx) = 0;

  virtual void enterMacroDeclaration(
      TorqueParser::MacroDeclarationContext* ctx) = 0;
  virtual void exitMacroDeclaration(
      TorqueParser::MacroDeclarationContext* ctx) = 0;

  virtual void enterConstDeclaration(
      TorqueParser::ConstDeclarationContext* ctx) = 0;
  virtual void exitConstDeclaration(
      TorqueParser::ConstDeclarationContext* ctx) = 0;

  virtual void enterDeclaration(TorqueParser::DeclarationContext* ctx) = 0;
  virtual void exitDeclaration(TorqueParser::DeclarationContext* ctx) = 0;

  virtual void enterModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* ctx) = 0;
  virtual void exitModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* ctx) = 0;

  virtual void enterFile(TorqueParser::FileContext* ctx) = 0;
  virtual void exitFile(TorqueParser::FileContext* ctx) = 0;
};

#endif  // V8_TORQUE_TORQUELISTENER_H_
