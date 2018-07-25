// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_TORQUE_TORQUEPARSER_H_
#define V8_TORQUE_TORQUEPARSER_H_

// Generated from Torque.g4 by ANTLR 4.7.1

#pragma once

#include "./antlr4-runtime.h"

class TorqueParser : public antlr4::Parser {
 public:
  enum {
    T__0 = 1,
    T__1 = 2,
    T__2 = 3,
    T__3 = 4,
    T__4 = 5,
    T__5 = 6,
    T__6 = 7,
    T__7 = 8,
    T__8 = 9,
    T__9 = 10,
    T__10 = 11,
    T__11 = 12,
    T__12 = 13,
    T__13 = 14,
    T__14 = 15,
    T__15 = 16,
    T__16 = 17,
    T__17 = 18,
    T__18 = 19,
    T__19 = 20,
    T__20 = 21,
    MACRO = 22,
    BUILTIN = 23,
    RUNTIME = 24,
    MODULE = 25,
    JAVASCRIPT = 26,
    IMPLICIT = 27,
    DEFERRED = 28,
    IF = 29,
    CAST_KEYWORD = 30,
    CONVERT_KEYWORD = 31,
    FOR = 32,
    WHILE = 33,
    RETURN = 34,
    CONSTEXPR = 35,
    CONTINUE = 36,
    BREAK = 37,
    GOTO = 38,
    OTHERWISE = 39,
    TRY = 40,
    CATCH = 41,
    LABEL = 42,
    LABELS = 43,
    TAIL = 44,
    ISNT = 45,
    IS = 46,
    LET = 47,
    EXTERN = 48,
    ASSERT = 49,
    UNREACHABLE_TOKEN = 50,
    DEBUG_TOKEN = 51,
    ASSIGNMENT = 52,
    ASSIGNMENT_OPERATOR = 53,
    EQUAL = 54,
    PLUS = 55,
    MINUS = 56,
    MULTIPLY = 57,
    DIVIDE = 58,
    MODULO = 59,
    BIT_OR = 60,
    BIT_AND = 61,
    BIT_NOT = 62,
    MAX = 63,
    MIN = 64,
    NOT_EQUAL = 65,
    LESS_THAN = 66,
    LESS_THAN_EQUAL = 67,
    GREATER_THAN = 68,
    GREATER_THAN_EQUAL = 69,
    SHIFT_LEFT = 70,
    SHIFT_RIGHT = 71,
    SHIFT_RIGHT_ARITHMETIC = 72,
    VARARGS = 73,
    EQUALITY_OPERATOR = 74,
    INCREMENT = 75,
    DECREMENT = 76,
    NOT = 77,
    STRING_LITERAL = 78,
    IDENTIFIER = 79,
    WS = 80,
    BLOCK_COMMENT = 81,
    LINE_COMMENT = 82,
    DECIMAL_LITERAL = 83
  };

  enum {
    RuleType = 0,
    RuleTypeList = 1,
    RuleGenericSpecializationTypeList = 2,
    RuleOptionalGenericTypeList = 3,
    RuleTypeListMaybeVarArgs = 4,
    RuleLabelParameter = 5,
    RuleOptionalType = 6,
    RuleOptionalLabelList = 7,
    RuleOptionalOtherwise = 8,
    RuleParameter = 9,
    RuleParameterList = 10,
    RuleLabelDeclaration = 11,
    RuleExpression = 12,
    RuleConditionalExpression = 13,
    RuleLogicalORExpression = 14,
    RuleLogicalANDExpression = 15,
    RuleBitwiseExpression = 16,
    RuleEqualityExpression = 17,
    RuleRelationalExpression = 18,
    RuleShiftExpression = 19,
    RuleAdditiveExpression = 20,
    RuleMultiplicativeExpression = 21,
    RuleUnaryExpression = 22,
    RuleLocationExpression = 23,
    RuleIncrementDecrement = 24,
    RuleAssignment = 25,
    RuleAssignmentExpression = 26,
    RulePrimaryExpression = 27,
    RuleForInitialization = 28,
    RuleForLoop = 29,
    RuleRangeSpecifier = 30,
    RuleForOfRange = 31,
    RuleForOfLoop = 32,
    RuleArgument = 33,
    RuleArgumentList = 34,
    RuleHelperCall = 35,
    RuleLabelReference = 36,
    RuleVariableDeclaration = 37,
    RuleVariableDeclarationWithInitialization = 38,
    RuleHelperCallStatement = 39,
    RuleExpressionStatement = 40,
    RuleIfStatement = 41,
    RuleWhileLoop = 42,
    RuleReturnStatement = 43,
    RuleBreakStatement = 44,
    RuleContinueStatement = 45,
    RuleGotoStatement = 46,
    RuleHandlerWithStatement = 47,
    RuleTryCatch = 48,
    RuleDiagnosticStatement = 49,
    RuleStatement = 50,
    RuleStatementList = 51,
    RuleStatementScope = 52,
    RuleStatementBlock = 53,
    RuleHelperBody = 54,
    RuleExtendsDeclaration = 55,
    RuleGeneratesDeclaration = 56,
    RuleConstexprDeclaration = 57,
    RuleTypeDeclaration = 58,
    RuleExternalBuiltin = 59,
    RuleExternalMacro = 60,
    RuleExternalRuntime = 61,
    RuleBuiltinDeclaration = 62,
    RuleGenericSpecialization = 63,
    RuleMacroDeclaration = 64,
    RuleConstDeclaration = 65,
    RuleDeclaration = 66,
    RuleModuleDeclaration = 67,
    RuleFile = 68
  };

  explicit TorqueParser(antlr4::TokenStream* input);
  ~TorqueParser();

  std::string getGrammarFileName() const override;
  const antlr4::atn::ATN& getATN() const override { return _atn; };
  const std::vector<std::string>& getTokenNames() const override {
    return _tokenNames;
  };  // deprecated: use vocabulary instead.
  const std::vector<std::string>& getRuleNames() const override;
  antlr4::dfa::Vocabulary& getVocabulary() const override;

  class TypeContext;
  class TypeListContext;
  class GenericSpecializationTypeListContext;
  class OptionalGenericTypeListContext;
  class TypeListMaybeVarArgsContext;
  class LabelParameterContext;
  class OptionalTypeContext;
  class OptionalLabelListContext;
  class OptionalOtherwiseContext;
  class ParameterContext;
  class ParameterListContext;
  class LabelDeclarationContext;
  class ExpressionContext;
  class ConditionalExpressionContext;
  class LogicalORExpressionContext;
  class LogicalANDExpressionContext;
  class BitwiseExpressionContext;
  class EqualityExpressionContext;
  class RelationalExpressionContext;
  class ShiftExpressionContext;
  class AdditiveExpressionContext;
  class MultiplicativeExpressionContext;
  class UnaryExpressionContext;
  class LocationExpressionContext;
  class IncrementDecrementContext;
  class AssignmentContext;
  class AssignmentExpressionContext;
  class PrimaryExpressionContext;
  class ForInitializationContext;
  class ForLoopContext;
  class RangeSpecifierContext;
  class ForOfRangeContext;
  class ForOfLoopContext;
  class ArgumentContext;
  class ArgumentListContext;
  class HelperCallContext;
  class LabelReferenceContext;
  class VariableDeclarationContext;
  class VariableDeclarationWithInitializationContext;
  class HelperCallStatementContext;
  class ExpressionStatementContext;
  class IfStatementContext;
  class WhileLoopContext;
  class ReturnStatementContext;
  class BreakStatementContext;
  class ContinueStatementContext;
  class GotoStatementContext;
  class HandlerWithStatementContext;
  class TryCatchContext;
  class DiagnosticStatementContext;
  class StatementContext;
  class StatementListContext;
  class StatementScopeContext;
  class StatementBlockContext;
  class HelperBodyContext;
  class ExtendsDeclarationContext;
  class GeneratesDeclarationContext;
  class ConstexprDeclarationContext;
  class TypeDeclarationContext;
  class ExternalBuiltinContext;
  class ExternalMacroContext;
  class ExternalRuntimeContext;
  class BuiltinDeclarationContext;
  class GenericSpecializationContext;
  class MacroDeclarationContext;
  class ConstDeclarationContext;
  class DeclarationContext;
  class ModuleDeclarationContext;
  class FileContext;

  class TypeContext : public antlr4::ParserRuleContext {
   public:
    TypeContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    antlr4::tree::TerminalNode* CONSTEXPR();
    antlr4::tree::TerminalNode* BUILTIN();
    TypeListContext* typeList();
    TypeContext* type();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TypeContext* type();

  class TypeListContext : public antlr4::ParserRuleContext {
   public:
    TypeListContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<TypeContext*> type();
    TypeContext* type(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TypeListContext* typeList();

  class GenericSpecializationTypeListContext
      : public antlr4::ParserRuleContext {
   public:
    GenericSpecializationTypeListContext(antlr4::ParserRuleContext* parent,
                                         size_t invokingState);
    size_t getRuleIndex() const override;
    TypeListContext* typeList();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  GenericSpecializationTypeListContext* genericSpecializationTypeList();

  class OptionalGenericTypeListContext : public antlr4::ParserRuleContext {
   public:
    OptionalGenericTypeListContext(antlr4::ParserRuleContext* parent,
                                   size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode*> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  OptionalGenericTypeListContext* optionalGenericTypeList();

  class TypeListMaybeVarArgsContext : public antlr4::ParserRuleContext {
   public:
    TypeListMaybeVarArgsContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<TypeContext*> type();
    TypeContext* type(size_t i);
    antlr4::tree::TerminalNode* VARARGS();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TypeListMaybeVarArgsContext* typeListMaybeVarArgs();

  class LabelParameterContext : public antlr4::ParserRuleContext {
   public:
    LabelParameterContext(antlr4::ParserRuleContext* parent,
                          size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeListContext* typeList();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  LabelParameterContext* labelParameter();

  class OptionalTypeContext : public antlr4::ParserRuleContext {
   public:
    OptionalTypeContext(antlr4::ParserRuleContext* parent,
                        size_t invokingState);
    size_t getRuleIndex() const override;
    TypeContext* type();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  OptionalTypeContext* optionalType();

  class OptionalLabelListContext : public antlr4::ParserRuleContext {
   public:
    OptionalLabelListContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* LABELS();
    std::vector<LabelParameterContext*> labelParameter();
    LabelParameterContext* labelParameter(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  OptionalLabelListContext* optionalLabelList();

  class OptionalOtherwiseContext : public antlr4::ParserRuleContext {
   public:
    OptionalOtherwiseContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* OTHERWISE();
    std::vector<antlr4::tree::TerminalNode*> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  OptionalOtherwiseContext* optionalOtherwise();

  class ParameterContext : public antlr4::ParserRuleContext {
   public:
    ParameterContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ParameterContext* parameter();

  class ParameterListContext : public antlr4::ParserRuleContext {
   public:
    ParameterListContext(antlr4::ParserRuleContext* parent,
                         size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<ParameterContext*> parameter();
    ParameterContext* parameter(size_t i);
    antlr4::tree::TerminalNode* VARARGS();
    antlr4::tree::TerminalNode* IDENTIFIER();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ParameterListContext* parameterList();

  class LabelDeclarationContext : public antlr4::ParserRuleContext {
   public:
    LabelDeclarationContext(antlr4::ParserRuleContext* parent,
                            size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    ParameterListContext* parameterList();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  LabelDeclarationContext* labelDeclaration();

  class ExpressionContext : public antlr4::ParserRuleContext {
   public:
    ExpressionContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    ConditionalExpressionContext* conditionalExpression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExpressionContext* expression();

  class ConditionalExpressionContext : public antlr4::ParserRuleContext {
   public:
    ConditionalExpressionContext(antlr4::ParserRuleContext* parent,
                                 size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<LogicalORExpressionContext*> logicalORExpression();
    LogicalORExpressionContext* logicalORExpression(size_t i);
    ConditionalExpressionContext* conditionalExpression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ConditionalExpressionContext* conditionalExpression();
  ConditionalExpressionContext* conditionalExpression(int precedence);
  class LogicalORExpressionContext : public antlr4::ParserRuleContext {
   public:
    LogicalORExpressionContext(antlr4::ParserRuleContext* parent,
                               size_t invokingState);
    size_t getRuleIndex() const override;
    LogicalANDExpressionContext* logicalANDExpression();
    LogicalORExpressionContext* logicalORExpression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  LogicalORExpressionContext* logicalORExpression();
  LogicalORExpressionContext* logicalORExpression(int precedence);
  class LogicalANDExpressionContext : public antlr4::ParserRuleContext {
   public:
    LogicalANDExpressionContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    BitwiseExpressionContext* bitwiseExpression();
    LogicalANDExpressionContext* logicalANDExpression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  LogicalANDExpressionContext* logicalANDExpression();
  LogicalANDExpressionContext* logicalANDExpression(int precedence);
  class BitwiseExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    BitwiseExpressionContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    EqualityExpressionContext* equalityExpression();
    BitwiseExpressionContext* bitwiseExpression();
    antlr4::tree::TerminalNode* BIT_AND();
    antlr4::tree::TerminalNode* BIT_OR();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  BitwiseExpressionContext* bitwiseExpression();
  BitwiseExpressionContext* bitwiseExpression(int precedence);
  class EqualityExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    EqualityExpressionContext(antlr4::ParserRuleContext* parent,
                              size_t invokingState);
    size_t getRuleIndex() const override;
    RelationalExpressionContext* relationalExpression();
    EqualityExpressionContext* equalityExpression();
    antlr4::tree::TerminalNode* EQUAL();
    antlr4::tree::TerminalNode* NOT_EQUAL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  EqualityExpressionContext* equalityExpression();
  EqualityExpressionContext* equalityExpression(int precedence);
  class RelationalExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    RelationalExpressionContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    ShiftExpressionContext* shiftExpression();
    RelationalExpressionContext* relationalExpression();
    antlr4::tree::TerminalNode* LESS_THAN();
    antlr4::tree::TerminalNode* LESS_THAN_EQUAL();
    antlr4::tree::TerminalNode* GREATER_THAN();
    antlr4::tree::TerminalNode* GREATER_THAN_EQUAL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  RelationalExpressionContext* relationalExpression();
  RelationalExpressionContext* relationalExpression(int precedence);
  class ShiftExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    ShiftExpressionContext(antlr4::ParserRuleContext* parent,
                           size_t invokingState);
    size_t getRuleIndex() const override;
    AdditiveExpressionContext* additiveExpression();
    ShiftExpressionContext* shiftExpression();
    antlr4::tree::TerminalNode* SHIFT_RIGHT();
    antlr4::tree::TerminalNode* SHIFT_LEFT();
    antlr4::tree::TerminalNode* SHIFT_RIGHT_ARITHMETIC();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ShiftExpressionContext* shiftExpression();
  ShiftExpressionContext* shiftExpression(int precedence);
  class AdditiveExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    AdditiveExpressionContext(antlr4::ParserRuleContext* parent,
                              size_t invokingState);
    size_t getRuleIndex() const override;
    MultiplicativeExpressionContext* multiplicativeExpression();
    AdditiveExpressionContext* additiveExpression();
    antlr4::tree::TerminalNode* PLUS();
    antlr4::tree::TerminalNode* MINUS();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  AdditiveExpressionContext* additiveExpression();
  AdditiveExpressionContext* additiveExpression(int precedence);
  class MultiplicativeExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    MultiplicativeExpressionContext(antlr4::ParserRuleContext* parent,
                                    size_t invokingState);
    size_t getRuleIndex() const override;
    UnaryExpressionContext* unaryExpression();
    MultiplicativeExpressionContext* multiplicativeExpression();
    antlr4::tree::TerminalNode* MULTIPLY();
    antlr4::tree::TerminalNode* DIVIDE();
    antlr4::tree::TerminalNode* MODULO();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  MultiplicativeExpressionContext* multiplicativeExpression();
  MultiplicativeExpressionContext* multiplicativeExpression(int precedence);
  class UnaryExpressionContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    UnaryExpressionContext(antlr4::ParserRuleContext* parent,
                           size_t invokingState);
    size_t getRuleIndex() const override;
    AssignmentExpressionContext* assignmentExpression();
    UnaryExpressionContext* unaryExpression();
    antlr4::tree::TerminalNode* PLUS();
    antlr4::tree::TerminalNode* MINUS();
    antlr4::tree::TerminalNode* BIT_NOT();
    antlr4::tree::TerminalNode* NOT();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  UnaryExpressionContext* unaryExpression();

  class LocationExpressionContext : public antlr4::ParserRuleContext {
   public:
    LocationExpressionContext(antlr4::ParserRuleContext* parent,
                              size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    GenericSpecializationTypeListContext* genericSpecializationTypeList();
    LocationExpressionContext* locationExpression();
    ExpressionContext* expression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  LocationExpressionContext* locationExpression();
  LocationExpressionContext* locationExpression(int precedence);
  class IncrementDecrementContext : public antlr4::ParserRuleContext {
   public:
    antlr4::Token* op = nullptr;
    IncrementDecrementContext(antlr4::ParserRuleContext* parent,
                              size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* INCREMENT();
    LocationExpressionContext* locationExpression();
    antlr4::tree::TerminalNode* DECREMENT();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  IncrementDecrementContext* incrementDecrement();

  class AssignmentContext : public antlr4::ParserRuleContext {
   public:
    AssignmentContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    IncrementDecrementContext* incrementDecrement();
    LocationExpressionContext* locationExpression();
    ExpressionContext* expression();
    antlr4::tree::TerminalNode* ASSIGNMENT();
    antlr4::tree::TerminalNode* ASSIGNMENT_OPERATOR();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  AssignmentContext* assignment();

  class AssignmentExpressionContext : public antlr4::ParserRuleContext {
   public:
    AssignmentExpressionContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    PrimaryExpressionContext* primaryExpression();
    AssignmentContext* assignment();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  AssignmentExpressionContext* assignmentExpression();

  class PrimaryExpressionContext : public antlr4::ParserRuleContext {
   public:
    PrimaryExpressionContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    HelperCallContext* helperCall();
    antlr4::tree::TerminalNode* DECIMAL_LITERAL();
    antlr4::tree::TerminalNode* STRING_LITERAL();
    antlr4::tree::TerminalNode* CAST_KEYWORD();
    TypeContext* type();
    ExpressionContext* expression();
    antlr4::tree::TerminalNode* OTHERWISE();
    antlr4::tree::TerminalNode* IDENTIFIER();
    antlr4::tree::TerminalNode* CONVERT_KEYWORD();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  PrimaryExpressionContext* primaryExpression();

  class ForInitializationContext : public antlr4::ParserRuleContext {
   public:
    ForInitializationContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    VariableDeclarationWithInitializationContext*
    variableDeclarationWithInitialization();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ForInitializationContext* forInitialization();

  class ForLoopContext : public antlr4::ParserRuleContext {
   public:
    ForLoopContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* FOR();
    ForInitializationContext* forInitialization();
    ExpressionContext* expression();
    AssignmentContext* assignment();
    StatementBlockContext* statementBlock();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ForLoopContext* forLoop();

  class RangeSpecifierContext : public antlr4::ParserRuleContext {
   public:
    TorqueParser::ExpressionContext* begin = nullptr;
    TorqueParser::ExpressionContext* end = nullptr;
    RangeSpecifierContext(antlr4::ParserRuleContext* parent,
                          size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<ExpressionContext*> expression();
    ExpressionContext* expression(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  RangeSpecifierContext* rangeSpecifier();

  class ForOfRangeContext : public antlr4::ParserRuleContext {
   public:
    ForOfRangeContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    RangeSpecifierContext* rangeSpecifier();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ForOfRangeContext* forOfRange();

  class ForOfLoopContext : public antlr4::ParserRuleContext {
   public:
    ForOfLoopContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* FOR();
    VariableDeclarationContext* variableDeclaration();
    ExpressionContext* expression();
    ForOfRangeContext* forOfRange();
    StatementBlockContext* statementBlock();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ForOfLoopContext* forOfLoop();

  class ArgumentContext : public antlr4::ParserRuleContext {
   public:
    ArgumentContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    ExpressionContext* expression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ArgumentContext* argument();

  class ArgumentListContext : public antlr4::ParserRuleContext {
   public:
    ArgumentListContext(antlr4::ParserRuleContext* parent,
                        size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<ArgumentContext*> argument();
    ArgumentContext* argument(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ArgumentListContext* argumentList();

  class HelperCallContext : public antlr4::ParserRuleContext {
   public:
    HelperCallContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    ArgumentListContext* argumentList();
    OptionalOtherwiseContext* optionalOtherwise();
    antlr4::tree::TerminalNode* MIN();
    antlr4::tree::TerminalNode* MAX();
    antlr4::tree::TerminalNode* IDENTIFIER();
    GenericSpecializationTypeListContext* genericSpecializationTypeList();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  HelperCallContext* helperCall();

  class LabelReferenceContext : public antlr4::ParserRuleContext {
   public:
    LabelReferenceContext(antlr4::ParserRuleContext* parent,
                          size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  LabelReferenceContext* labelReference();

  class VariableDeclarationContext : public antlr4::ParserRuleContext {
   public:
    VariableDeclarationContext(antlr4::ParserRuleContext* parent,
                               size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* LET();
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  VariableDeclarationContext* variableDeclaration();

  class VariableDeclarationWithInitializationContext
      : public antlr4::ParserRuleContext {
   public:
    VariableDeclarationWithInitializationContext(
        antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    VariableDeclarationContext* variableDeclaration();
    antlr4::tree::TerminalNode* ASSIGNMENT();
    ExpressionContext* expression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  VariableDeclarationWithInitializationContext*
  variableDeclarationWithInitialization();

  class HelperCallStatementContext : public antlr4::ParserRuleContext {
   public:
    HelperCallStatementContext(antlr4::ParserRuleContext* parent,
                               size_t invokingState);
    size_t getRuleIndex() const override;
    HelperCallContext* helperCall();
    antlr4::tree::TerminalNode* TAIL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  HelperCallStatementContext* helperCallStatement();

  class ExpressionStatementContext : public antlr4::ParserRuleContext {
   public:
    ExpressionStatementContext(antlr4::ParserRuleContext* parent,
                               size_t invokingState);
    size_t getRuleIndex() const override;
    AssignmentContext* assignment();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExpressionStatementContext* expressionStatement();

  class IfStatementContext : public antlr4::ParserRuleContext {
   public:
    IfStatementContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IF();
    ExpressionContext* expression();
    std::vector<StatementBlockContext*> statementBlock();
    StatementBlockContext* statementBlock(size_t i);
    antlr4::tree::TerminalNode* CONSTEXPR();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  IfStatementContext* ifStatement();

  class WhileLoopContext : public antlr4::ParserRuleContext {
   public:
    WhileLoopContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* WHILE();
    ExpressionContext* expression();
    StatementBlockContext* statementBlock();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  WhileLoopContext* whileLoop();

  class ReturnStatementContext : public antlr4::ParserRuleContext {
   public:
    ReturnStatementContext(antlr4::ParserRuleContext* parent,
                           size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* RETURN();
    ExpressionContext* expression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ReturnStatementContext* returnStatement();

  class BreakStatementContext : public antlr4::ParserRuleContext {
   public:
    BreakStatementContext(antlr4::ParserRuleContext* parent,
                          size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* BREAK();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  BreakStatementContext* breakStatement();

  class ContinueStatementContext : public antlr4::ParserRuleContext {
   public:
    ContinueStatementContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* CONTINUE();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ContinueStatementContext* continueStatement();

  class GotoStatementContext : public antlr4::ParserRuleContext {
   public:
    GotoStatementContext(antlr4::ParserRuleContext* parent,
                         size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* GOTO();
    LabelReferenceContext* labelReference();
    ArgumentListContext* argumentList();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  GotoStatementContext* gotoStatement();

  class HandlerWithStatementContext : public antlr4::ParserRuleContext {
   public:
    HandlerWithStatementContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    StatementBlockContext* statementBlock();
    antlr4::tree::TerminalNode* CATCH();
    antlr4::tree::TerminalNode* IDENTIFIER();
    antlr4::tree::TerminalNode* LABEL();
    LabelDeclarationContext* labelDeclaration();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  HandlerWithStatementContext* handlerWithStatement();

  class TryCatchContext : public antlr4::ParserRuleContext {
   public:
    TryCatchContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* TRY();
    StatementBlockContext* statementBlock();
    std::vector<HandlerWithStatementContext*> handlerWithStatement();
    HandlerWithStatementContext* handlerWithStatement(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TryCatchContext* tryCatch();

  class DiagnosticStatementContext : public antlr4::ParserRuleContext {
   public:
    DiagnosticStatementContext(antlr4::ParserRuleContext* parent,
                               size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* ASSERT();
    ExpressionContext* expression();
    antlr4::tree::TerminalNode* UNREACHABLE_TOKEN();
    antlr4::tree::TerminalNode* DEBUG_TOKEN();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  DiagnosticStatementContext* diagnosticStatement();

  class StatementContext : public antlr4::ParserRuleContext {
   public:
    StatementContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    VariableDeclarationWithInitializationContext*
    variableDeclarationWithInitialization();
    HelperCallStatementContext* helperCallStatement();
    ExpressionStatementContext* expressionStatement();
    ReturnStatementContext* returnStatement();
    BreakStatementContext* breakStatement();
    ContinueStatementContext* continueStatement();
    GotoStatementContext* gotoStatement();
    IfStatementContext* ifStatement();
    DiagnosticStatementContext* diagnosticStatement();
    WhileLoopContext* whileLoop();
    ForOfLoopContext* forOfLoop();
    ForLoopContext* forLoop();
    TryCatchContext* tryCatch();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  StatementContext* statement();

  class StatementListContext : public antlr4::ParserRuleContext {
   public:
    StatementListContext(antlr4::ParserRuleContext* parent,
                         size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<StatementContext*> statement();
    StatementContext* statement(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  StatementListContext* statementList();

  class StatementScopeContext : public antlr4::ParserRuleContext {
   public:
    StatementScopeContext(antlr4::ParserRuleContext* parent,
                          size_t invokingState);
    size_t getRuleIndex() const override;
    StatementListContext* statementList();
    antlr4::tree::TerminalNode* DEFERRED();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  StatementScopeContext* statementScope();

  class StatementBlockContext : public antlr4::ParserRuleContext {
   public:
    StatementBlockContext(antlr4::ParserRuleContext* parent,
                          size_t invokingState);
    size_t getRuleIndex() const override;
    StatementContext* statement();
    StatementScopeContext* statementScope();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  StatementBlockContext* statementBlock();

  class HelperBodyContext : public antlr4::ParserRuleContext {
   public:
    HelperBodyContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    StatementScopeContext* statementScope();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  HelperBodyContext* helperBody();

  class ExtendsDeclarationContext : public antlr4::ParserRuleContext {
   public:
    ExtendsDeclarationContext(antlr4::ParserRuleContext* parent,
                              size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExtendsDeclarationContext* extendsDeclaration();

  class GeneratesDeclarationContext : public antlr4::ParserRuleContext {
   public:
    GeneratesDeclarationContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* STRING_LITERAL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  GeneratesDeclarationContext* generatesDeclaration();

  class ConstexprDeclarationContext : public antlr4::ParserRuleContext {
   public:
    ConstexprDeclarationContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* STRING_LITERAL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ConstexprDeclarationContext* constexprDeclaration();

  class TypeDeclarationContext : public antlr4::ParserRuleContext {
   public:
    TypeDeclarationContext(antlr4::ParserRuleContext* parent,
                           size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    ExtendsDeclarationContext* extendsDeclaration();
    GeneratesDeclarationContext* generatesDeclaration();
    ConstexprDeclarationContext* constexprDeclaration();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TypeDeclarationContext* typeDeclaration();

  class ExternalBuiltinContext : public antlr4::ParserRuleContext {
   public:
    ExternalBuiltinContext(antlr4::ParserRuleContext* parent,
                           size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* EXTERN();
    antlr4::tree::TerminalNode* BUILTIN();
    antlr4::tree::TerminalNode* IDENTIFIER();
    OptionalGenericTypeListContext* optionalGenericTypeList();
    TypeListContext* typeList();
    OptionalTypeContext* optionalType();
    antlr4::tree::TerminalNode* JAVASCRIPT();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExternalBuiltinContext* externalBuiltin();

  class ExternalMacroContext : public antlr4::ParserRuleContext {
   public:
    ExternalMacroContext(antlr4::ParserRuleContext* parent,
                         size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* EXTERN();
    antlr4::tree::TerminalNode* MACRO();
    antlr4::tree::TerminalNode* IDENTIFIER();
    OptionalGenericTypeListContext* optionalGenericTypeList();
    TypeListMaybeVarArgsContext* typeListMaybeVarArgs();
    OptionalTypeContext* optionalType();
    OptionalLabelListContext* optionalLabelList();
    antlr4::tree::TerminalNode* STRING_LITERAL();
    antlr4::tree::TerminalNode* IMPLICIT();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExternalMacroContext* externalMacro();

  class ExternalRuntimeContext : public antlr4::ParserRuleContext {
   public:
    ExternalRuntimeContext(antlr4::ParserRuleContext* parent,
                           size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* EXTERN();
    antlr4::tree::TerminalNode* RUNTIME();
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeListMaybeVarArgsContext* typeListMaybeVarArgs();
    OptionalTypeContext* optionalType();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExternalRuntimeContext* externalRuntime();

  class BuiltinDeclarationContext : public antlr4::ParserRuleContext {
   public:
    BuiltinDeclarationContext(antlr4::ParserRuleContext* parent,
                              size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* BUILTIN();
    antlr4::tree::TerminalNode* IDENTIFIER();
    OptionalGenericTypeListContext* optionalGenericTypeList();
    ParameterListContext* parameterList();
    OptionalTypeContext* optionalType();
    HelperBodyContext* helperBody();
    antlr4::tree::TerminalNode* JAVASCRIPT();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  BuiltinDeclarationContext* builtinDeclaration();

  class GenericSpecializationContext : public antlr4::ParserRuleContext {
   public:
    GenericSpecializationContext(antlr4::ParserRuleContext* parent,
                                 size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    GenericSpecializationTypeListContext* genericSpecializationTypeList();
    ParameterListContext* parameterList();
    OptionalTypeContext* optionalType();
    OptionalLabelListContext* optionalLabelList();
    HelperBodyContext* helperBody();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  GenericSpecializationContext* genericSpecialization();

  class MacroDeclarationContext : public antlr4::ParserRuleContext {
   public:
    MacroDeclarationContext(antlr4::ParserRuleContext* parent,
                            size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* MACRO();
    antlr4::tree::TerminalNode* IDENTIFIER();
    OptionalGenericTypeListContext* optionalGenericTypeList();
    ParameterListContext* parameterList();
    OptionalTypeContext* optionalType();
    OptionalLabelListContext* optionalLabelList();
    HelperBodyContext* helperBody();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  MacroDeclarationContext* macroDeclaration();

  class ConstDeclarationContext : public antlr4::ParserRuleContext {
   public:
    ConstDeclarationContext(antlr4::ParserRuleContext* parent,
                            size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();
    antlr4::tree::TerminalNode* STRING_LITERAL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ConstDeclarationContext* constDeclaration();

  class DeclarationContext : public antlr4::ParserRuleContext {
   public:
    DeclarationContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    TypeDeclarationContext* typeDeclaration();
    BuiltinDeclarationContext* builtinDeclaration();
    GenericSpecializationContext* genericSpecialization();
    MacroDeclarationContext* macroDeclaration();
    ExternalMacroContext* externalMacro();
    ExternalBuiltinContext* externalBuiltin();
    ExternalRuntimeContext* externalRuntime();
    ConstDeclarationContext* constDeclaration();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  DeclarationContext* declaration();

  class ModuleDeclarationContext : public antlr4::ParserRuleContext {
   public:
    ModuleDeclarationContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* MODULE();
    antlr4::tree::TerminalNode* IDENTIFIER();
    std::vector<DeclarationContext*> declaration();
    DeclarationContext* declaration(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ModuleDeclarationContext* moduleDeclaration();

  class FileContext : public antlr4::ParserRuleContext {
   public:
    FileContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<ModuleDeclarationContext*> moduleDeclaration();
    ModuleDeclarationContext* moduleDeclaration(size_t i);
    std::vector<DeclarationContext*> declaration();
    DeclarationContext* declaration(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  FileContext* file();

  bool sempred(antlr4::RuleContext* _localctx, size_t ruleIndex,
               size_t predicateIndex) override;
  bool conditionalExpressionSempred(ConditionalExpressionContext* _localctx,
                                    size_t predicateIndex);
  bool logicalORExpressionSempred(LogicalORExpressionContext* _localctx,
                                  size_t predicateIndex);
  bool logicalANDExpressionSempred(LogicalANDExpressionContext* _localctx,
                                   size_t predicateIndex);
  bool bitwiseExpressionSempred(BitwiseExpressionContext* _localctx,
                                size_t predicateIndex);
  bool equalityExpressionSempred(EqualityExpressionContext* _localctx,
                                 size_t predicateIndex);
  bool relationalExpressionSempred(RelationalExpressionContext* _localctx,
                                   size_t predicateIndex);
  bool shiftExpressionSempred(ShiftExpressionContext* _localctx,
                              size_t predicateIndex);
  bool additiveExpressionSempred(AdditiveExpressionContext* _localctx,
                                 size_t predicateIndex);
  bool multiplicativeExpressionSempred(
      MultiplicativeExpressionContext* _localctx, size_t predicateIndex);
  bool locationExpressionSempred(LocationExpressionContext* _localctx,
                                 size_t predicateIndex);

 private:
  static std::vector<antlr4::dfa::DFA> _decisionToDFA;
  static antlr4::atn::PredictionContextCache _sharedContextCache;
  static std::vector<std::string> _ruleNames;
  static std::vector<std::string> _tokenNames;

  static std::vector<std::string> _literalNames;
  static std::vector<std::string> _symbolicNames;
  static antlr4::dfa::Vocabulary _vocabulary;
  static antlr4::atn::ATN _atn;
  static std::vector<uint16_t> _serializedATN;

  struct Initializer {
    Initializer();
  };
  static Initializer _init;
};

#endif  // V8_TORQUE_TORQUEPARSER_H_
