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
    DEFERRED = 27,
    IF = 28,
    FOR = 29,
    WHILE = 30,
    RETURN = 31,
    CONSTEXPR = 32,
    CONTINUE = 33,
    BREAK = 34,
    GOTO = 35,
    OTHERWISE = 36,
    TRY = 37,
    LABEL = 38,
    LABELS = 39,
    TAIL = 40,
    ISNT = 41,
    IS = 42,
    LET = 43,
    CONST = 44,
    EXTERN = 45,
    ASSERT_TOKEN = 46,
    CHECK_TOKEN = 47,
    UNREACHABLE_TOKEN = 48,
    DEBUG_TOKEN = 49,
    ASSIGNMENT = 50,
    ASSIGNMENT_OPERATOR = 51,
    EQUAL = 52,
    PLUS = 53,
    MINUS = 54,
    MULTIPLY = 55,
    DIVIDE = 56,
    MODULO = 57,
    BIT_OR = 58,
    BIT_AND = 59,
    BIT_NOT = 60,
    MAX = 61,
    MIN = 62,
    NOT_EQUAL = 63,
    LESS_THAN = 64,
    LESS_THAN_EQUAL = 65,
    GREATER_THAN = 66,
    GREATER_THAN_EQUAL = 67,
    SHIFT_LEFT = 68,
    SHIFT_RIGHT = 69,
    SHIFT_RIGHT_ARITHMETIC = 70,
    VARARGS = 71,
    EQUALITY_OPERATOR = 72,
    INCREMENT = 73,
    DECREMENT = 74,
    NOT = 75,
    STRING_LITERAL = 76,
    IDENTIFIER = 77,
    WS = 78,
    BLOCK_COMMENT = 79,
    LINE_COMMENT = 80,
    DECIMAL_LITERAL = 81
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
    RuleStructExpression = 27,
    RuleFunctionPointerExpression = 28,
    RulePrimaryExpression = 29,
    RuleForInitialization = 30,
    RuleForLoop = 31,
    RuleRangeSpecifier = 32,
    RuleForOfRange = 33,
    RuleForOfLoop = 34,
    RuleArgument = 35,
    RuleArgumentList = 36,
    RuleHelperCall = 37,
    RuleLabelReference = 38,
    RuleVariableDeclaration = 39,
    RuleVariableDeclarationWithInitialization = 40,
    RuleHelperCallStatement = 41,
    RuleExpressionStatement = 42,
    RuleIfStatement = 43,
    RuleWhileLoop = 44,
    RuleReturnStatement = 45,
    RuleBreakStatement = 46,
    RuleContinueStatement = 47,
    RuleGotoStatement = 48,
    RuleHandlerWithStatement = 49,
    RuleTryLabelStatement = 50,
    RuleDiagnosticStatement = 51,
    RuleStatement = 52,
    RuleStatementList = 53,
    RuleStatementScope = 54,
    RuleStatementBlock = 55,
    RuleHelperBody = 56,
    RuleFieldDeclaration = 57,
    RuleFieldListDeclaration = 58,
    RuleExtendsDeclaration = 59,
    RuleGeneratesDeclaration = 60,
    RuleConstexprDeclaration = 61,
    RuleTypeDeclaration = 62,
    RuleTypeAliasDeclaration = 63,
    RuleExternalBuiltin = 64,
    RuleExternalMacro = 65,
    RuleExternalRuntime = 66,
    RuleBuiltinDeclaration = 67,
    RuleGenericSpecialization = 68,
    RuleMacroDeclaration = 69,
    RuleExternConstDeclaration = 70,
    RuleConstDeclaration = 71,
    RuleStructDeclaration = 72,
    RuleDeclaration = 73,
    RuleModuleDeclaration = 74,
    RuleFile = 75
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
  class StructExpressionContext;
  class FunctionPointerExpressionContext;
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
  class TryLabelStatementContext;
  class DiagnosticStatementContext;
  class StatementContext;
  class StatementListContext;
  class StatementScopeContext;
  class StatementBlockContext;
  class HelperBodyContext;
  class FieldDeclarationContext;
  class FieldListDeclarationContext;
  class ExtendsDeclarationContext;
  class GeneratesDeclarationContext;
  class ConstexprDeclarationContext;
  class TypeDeclarationContext;
  class TypeAliasDeclarationContext;
  class ExternalBuiltinContext;
  class ExternalMacroContext;
  class ExternalRuntimeContext;
  class BuiltinDeclarationContext;
  class GenericSpecializationContext;
  class MacroDeclarationContext;
  class ExternConstDeclarationContext;
  class ConstDeclarationContext;
  class StructDeclarationContext;
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
    std::vector<TypeContext*> type();
    TypeContext* type(size_t i);
    antlr4::tree::TerminalNode* BIT_OR();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TypeContext* type();
  TypeContext* type(int precedence);
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
    PrimaryExpressionContext* primaryExpression();
    ExpressionContext* expression();
    LocationExpressionContext* locationExpression();

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
    FunctionPointerExpressionContext* functionPointerExpression();
    AssignmentContext* assignment();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  AssignmentExpressionContext* assignmentExpression();

  class StructExpressionContext : public antlr4::ParserRuleContext {
   public:
    StructExpressionContext(antlr4::ParserRuleContext* parent,
                            size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    std::vector<ExpressionContext*> expression();
    ExpressionContext* expression(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  StructExpressionContext* structExpression();

  class FunctionPointerExpressionContext : public antlr4::ParserRuleContext {
   public:
    FunctionPointerExpressionContext(antlr4::ParserRuleContext* parent,
                                     size_t invokingState);
    size_t getRuleIndex() const override;
    PrimaryExpressionContext* primaryExpression();
    antlr4::tree::TerminalNode* IDENTIFIER();
    GenericSpecializationTypeListContext* genericSpecializationTypeList();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  FunctionPointerExpressionContext* functionPointerExpression();

  class PrimaryExpressionContext : public antlr4::ParserRuleContext {
   public:
    PrimaryExpressionContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    HelperCallContext* helperCall();
    StructExpressionContext* structExpression();
    antlr4::tree::TerminalNode* DECIMAL_LITERAL();
    antlr4::tree::TerminalNode* STRING_LITERAL();
    ExpressionContext* expression();

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
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();
    antlr4::tree::TerminalNode* LET();
    antlr4::tree::TerminalNode* CONST();

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
    antlr4::tree::TerminalNode* LABEL();
    LabelDeclarationContext* labelDeclaration();
    StatementBlockContext* statementBlock();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  HandlerWithStatementContext* handlerWithStatement();

  class TryLabelStatementContext : public antlr4::ParserRuleContext {
   public:
    TryLabelStatementContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* TRY();
    StatementBlockContext* statementBlock();
    std::vector<HandlerWithStatementContext*> handlerWithStatement();
    HandlerWithStatementContext* handlerWithStatement(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TryLabelStatementContext* tryLabelStatement();

  class DiagnosticStatementContext : public antlr4::ParserRuleContext {
   public:
    DiagnosticStatementContext(antlr4::ParserRuleContext* parent,
                               size_t invokingState);
    size_t getRuleIndex() const override;
    ExpressionContext* expression();
    antlr4::tree::TerminalNode* ASSERT_TOKEN();
    antlr4::tree::TerminalNode* CHECK_TOKEN();
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
    TryLabelStatementContext* tryLabelStatement();

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

  class FieldDeclarationContext : public antlr4::ParserRuleContext {
   public:
    FieldDeclarationContext(antlr4::ParserRuleContext* parent,
                            size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  FieldDeclarationContext* fieldDeclaration();

  class FieldListDeclarationContext : public antlr4::ParserRuleContext {
   public:
    FieldListDeclarationContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    std::vector<FieldDeclarationContext*> fieldDeclaration();
    FieldDeclarationContext* fieldDeclaration(size_t i);

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  FieldListDeclarationContext* fieldListDeclaration();

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

  class TypeAliasDeclarationContext : public antlr4::ParserRuleContext {
   public:
    TypeAliasDeclarationContext(antlr4::ParserRuleContext* parent,
                                size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  TypeAliasDeclarationContext* typeAliasDeclaration();

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
    antlr4::tree::TerminalNode* STRING_LITERAL();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  MacroDeclarationContext* macroDeclaration();

  class ExternConstDeclarationContext : public antlr4::ParserRuleContext {
   public:
    ExternConstDeclarationContext(antlr4::ParserRuleContext* parent,
                                  size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* CONST();
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();
    GeneratesDeclarationContext* generatesDeclaration();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ExternConstDeclarationContext* externConstDeclaration();

  class ConstDeclarationContext : public antlr4::ParserRuleContext {
   public:
    ConstDeclarationContext(antlr4::ParserRuleContext* parent,
                            size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* CONST();
    antlr4::tree::TerminalNode* IDENTIFIER();
    TypeContext* type();
    antlr4::tree::TerminalNode* ASSIGNMENT();
    ExpressionContext* expression();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  ConstDeclarationContext* constDeclaration();

  class StructDeclarationContext : public antlr4::ParserRuleContext {
   public:
    StructDeclarationContext(antlr4::ParserRuleContext* parent,
                             size_t invokingState);
    size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode* IDENTIFIER();
    FieldListDeclarationContext* fieldListDeclaration();

    void enterRule(antlr4::tree::ParseTreeListener* listener) override;
    void exitRule(antlr4::tree::ParseTreeListener* listener) override;

    antlrcpp::Any accept(antlr4::tree::ParseTreeVisitor* visitor) override;
  };

  StructDeclarationContext* structDeclaration();

  class DeclarationContext : public antlr4::ParserRuleContext {
   public:
    DeclarationContext(antlr4::ParserRuleContext* parent, size_t invokingState);
    size_t getRuleIndex() const override;
    StructDeclarationContext* structDeclaration();
    TypeDeclarationContext* typeDeclaration();
    TypeAliasDeclarationContext* typeAliasDeclaration();
    BuiltinDeclarationContext* builtinDeclaration();
    GenericSpecializationContext* genericSpecialization();
    MacroDeclarationContext* macroDeclaration();
    ExternalMacroContext* externalMacro();
    ExternalBuiltinContext* externalBuiltin();
    ExternalRuntimeContext* externalRuntime();
    ExternConstDeclarationContext* externConstDeclaration();
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
  bool typeSempred(TypeContext* _localctx, size_t predicateIndex);
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
