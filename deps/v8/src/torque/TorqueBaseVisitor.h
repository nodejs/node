// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_TORQUE_TORQUEBASEVISITOR_H_
#define V8_TORQUE_TORQUEBASEVISITOR_H_

// Generated from Torque.g4 by ANTLR 4.7.1

#pragma once

#include "./antlr4-runtime.h"
#include "TorqueVisitor.h"

/**
 * This class provides an empty implementation of TorqueVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the
 * available methods.
 */
class TorqueBaseVisitor : public TorqueVisitor {
 public:
  antlrcpp::Any visitType(TorqueParser::TypeContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitTypeList(TorqueParser::TypeListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitGenericSpecializationTypeList(
      TorqueParser::GenericSpecializationTypeListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitOptionalGenericTypeList(
      TorqueParser::OptionalGenericTypeListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitTypeListMaybeVarArgs(
      TorqueParser::TypeListMaybeVarArgsContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitLabelParameter(
      TorqueParser::LabelParameterContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitOptionalType(
      TorqueParser::OptionalTypeContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitOptionalLabelList(
      TorqueParser::OptionalLabelListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitOptionalOtherwise(
      TorqueParser::OptionalOtherwiseContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitParameter(TorqueParser::ParameterContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitParameterList(
      TorqueParser::ParameterListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitLabelDeclaration(
      TorqueParser::LabelDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExpression(TorqueParser::ExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitConditionalExpression(
      TorqueParser::ConditionalExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitLogicalORExpression(
      TorqueParser::LogicalORExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitLogicalANDExpression(
      TorqueParser::LogicalANDExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitBitwiseExpression(
      TorqueParser::BitwiseExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitEqualityExpression(
      TorqueParser::EqualityExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitRelationalExpression(
      TorqueParser::RelationalExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitShiftExpression(
      TorqueParser::ShiftExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitAdditiveExpression(
      TorqueParser::AdditiveExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitMultiplicativeExpression(
      TorqueParser::MultiplicativeExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitUnaryExpression(
      TorqueParser::UnaryExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitLocationExpression(
      TorqueParser::LocationExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitIncrementDecrement(
      TorqueParser::IncrementDecrementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitAssignment(TorqueParser::AssignmentContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitAssignmentExpression(
      TorqueParser::AssignmentExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitStructExpression(
      TorqueParser::StructExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitFunctionPointerExpression(
      TorqueParser::FunctionPointerExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitPrimaryExpression(
      TorqueParser::PrimaryExpressionContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitForInitialization(
      TorqueParser::ForInitializationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitForLoop(TorqueParser::ForLoopContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitRangeSpecifier(
      TorqueParser::RangeSpecifierContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitForOfRange(TorqueParser::ForOfRangeContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitForOfLoop(TorqueParser::ForOfLoopContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitArgument(TorqueParser::ArgumentContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitArgumentList(
      TorqueParser::ArgumentListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitHelperCall(TorqueParser::HelperCallContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitLabelReference(
      TorqueParser::LabelReferenceContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitVariableDeclaration(
      TorqueParser::VariableDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitVariableDeclarationWithInitialization(
      TorqueParser::VariableDeclarationWithInitializationContext* ctx)
      override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitHelperCallStatement(
      TorqueParser::HelperCallStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExpressionStatement(
      TorqueParser::ExpressionStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitIfStatement(
      TorqueParser::IfStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitWhileLoop(TorqueParser::WhileLoopContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitReturnStatement(
      TorqueParser::ReturnStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitBreakStatement(
      TorqueParser::BreakStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitContinueStatement(
      TorqueParser::ContinueStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitGotoStatement(
      TorqueParser::GotoStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitHandlerWithStatement(
      TorqueParser::HandlerWithStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitTryLabelStatement(
      TorqueParser::TryLabelStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitDiagnosticStatement(
      TorqueParser::DiagnosticStatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitStatement(TorqueParser::StatementContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitStatementList(
      TorqueParser::StatementListContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitStatementScope(
      TorqueParser::StatementScopeContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitStatementBlock(
      TorqueParser::StatementBlockContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitHelperBody(TorqueParser::HelperBodyContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitFieldDeclaration(
      TorqueParser::FieldDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitFieldListDeclaration(
      TorqueParser::FieldListDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExtendsDeclaration(
      TorqueParser::ExtendsDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitGeneratesDeclaration(
      TorqueParser::GeneratesDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitConstexprDeclaration(
      TorqueParser::ConstexprDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitTypeDeclaration(
      TorqueParser::TypeDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitTypeAliasDeclaration(
      TorqueParser::TypeAliasDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExternalBuiltin(
      TorqueParser::ExternalBuiltinContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExternalMacro(
      TorqueParser::ExternalMacroContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExternalRuntime(
      TorqueParser::ExternalRuntimeContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitBuiltinDeclaration(
      TorqueParser::BuiltinDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitGenericSpecialization(
      TorqueParser::GenericSpecializationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitMacroDeclaration(
      TorqueParser::MacroDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitExternConstDeclaration(
      TorqueParser::ExternConstDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitConstDeclaration(
      TorqueParser::ConstDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitStructDeclaration(
      TorqueParser::StructDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitDeclaration(
      TorqueParser::DeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitModuleDeclaration(
      TorqueParser::ModuleDeclarationContext* ctx) override {
    return visitChildren(ctx);
  }

  antlrcpp::Any visitFile(TorqueParser::FileContext* ctx) override {
    return visitChildren(ctx);
  }
};

#endif  // V8_TORQUE_TORQUEBASEVISITOR_H_
