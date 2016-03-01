// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSER_H
#define V8_PARSING_PREPARSER_H

#include "src/ast/scopes.h"
#include "src/bailout-reason.h"
#include "src/hashmap.h"
#include "src/messages.h"
#include "src/parsing/expression-classifier.h"
#include "src/parsing/func-name-inferrer.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/scanner.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {


class PreParserIdentifier {
 public:
  PreParserIdentifier() : type_(kUnknownIdentifier) {}
  static PreParserIdentifier Default() {
    return PreParserIdentifier(kUnknownIdentifier);
  }
  static PreParserIdentifier Eval() {
    return PreParserIdentifier(kEvalIdentifier);
  }
  static PreParserIdentifier Arguments() {
    return PreParserIdentifier(kArgumentsIdentifier);
  }
  static PreParserIdentifier Undefined() {
    return PreParserIdentifier(kUndefinedIdentifier);
  }
  static PreParserIdentifier FutureReserved() {
    return PreParserIdentifier(kFutureReservedIdentifier);
  }
  static PreParserIdentifier FutureStrictReserved() {
    return PreParserIdentifier(kFutureStrictReservedIdentifier);
  }
  static PreParserIdentifier Let() {
    return PreParserIdentifier(kLetIdentifier);
  }
  static PreParserIdentifier Static() {
    return PreParserIdentifier(kStaticIdentifier);
  }
  static PreParserIdentifier Yield() {
    return PreParserIdentifier(kYieldIdentifier);
  }
  static PreParserIdentifier Prototype() {
    return PreParserIdentifier(kPrototypeIdentifier);
  }
  static PreParserIdentifier Constructor() {
    return PreParserIdentifier(kConstructorIdentifier);
  }
  bool IsEval() const { return type_ == kEvalIdentifier; }
  bool IsArguments() const { return type_ == kArgumentsIdentifier; }
  bool IsEvalOrArguments() const { return IsEval() || IsArguments(); }
  bool IsUndefined() const { return type_ == kUndefinedIdentifier; }
  bool IsLet() const { return type_ == kLetIdentifier; }
  bool IsStatic() const { return type_ == kStaticIdentifier; }
  bool IsYield() const { return type_ == kYieldIdentifier; }
  bool IsPrototype() const { return type_ == kPrototypeIdentifier; }
  bool IsConstructor() const { return type_ == kConstructorIdentifier; }
  bool IsFutureReserved() const { return type_ == kFutureReservedIdentifier; }
  bool IsFutureStrictReserved() const {
    return type_ == kFutureStrictReservedIdentifier ||
           type_ == kLetIdentifier || type_ == kStaticIdentifier ||
           type_ == kYieldIdentifier;
  }

  // Allow identifier->name()[->length()] to work. The preparser
  // does not need the actual positions/lengths of the identifiers.
  const PreParserIdentifier* operator->() const { return this; }
  const PreParserIdentifier raw_name() const { return *this; }

  int position() const { return 0; }
  int length() const { return 0; }

 private:
  enum Type {
    kUnknownIdentifier,
    kFutureReservedIdentifier,
    kFutureStrictReservedIdentifier,
    kLetIdentifier,
    kStaticIdentifier,
    kYieldIdentifier,
    kEvalIdentifier,
    kArgumentsIdentifier,
    kUndefinedIdentifier,
    kPrototypeIdentifier,
    kConstructorIdentifier
  };

  explicit PreParserIdentifier(Type type) : type_(type) {}
  Type type_;

  friend class PreParserExpression;
};


class PreParserExpression {
 public:
  static PreParserExpression Default() {
    return PreParserExpression(TypeField::encode(kExpression));
  }

  static PreParserExpression Spread(PreParserExpression expression) {
    return PreParserExpression(TypeField::encode(kSpreadExpression));
  }

  static PreParserExpression FromIdentifier(PreParserIdentifier id) {
    return PreParserExpression(TypeField::encode(kIdentifierExpression) |
                               IdentifierTypeField::encode(id.type_));
  }

  static PreParserExpression BinaryOperation(PreParserExpression left,
                                             Token::Value op,
                                             PreParserExpression right) {
    return PreParserExpression(TypeField::encode(kBinaryOperationExpression));
  }

  static PreParserExpression Assignment() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kAssignment));
  }

  static PreParserExpression ObjectLiteral() {
    return PreParserExpression(TypeField::encode(kObjectLiteralExpression));
  }

  static PreParserExpression ArrayLiteral() {
    return PreParserExpression(TypeField::encode(kArrayLiteralExpression));
  }

  static PreParserExpression StringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression));
  }

  static PreParserExpression UseStrictStringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression) |
                               IsUseStrictField::encode(true));
  }

  static PreParserExpression UseStrongStringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression) |
                               IsUseStrongField::encode(true));
  }

  static PreParserExpression This() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kThisExpression));
  }

  static PreParserExpression ThisProperty() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kThisPropertyExpression));
  }

  static PreParserExpression Property() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kPropertyExpression));
  }

  static PreParserExpression Call() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kCallExpression));
  }

  static PreParserExpression SuperCallReference() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kSuperCallReference));
  }

  static PreParserExpression NoTemplateTag() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kNoTemplateTagExpression));
  }

  bool IsIdentifier() const {
    return TypeField::decode(code_) == kIdentifierExpression;
  }

  PreParserIdentifier AsIdentifier() const {
    DCHECK(IsIdentifier());
    return PreParserIdentifier(IdentifierTypeField::decode(code_));
  }

  bool IsAssignment() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kAssignment;
  }

  bool IsObjectLiteral() const {
    return TypeField::decode(code_) == kObjectLiteralExpression;
  }

  bool IsArrayLiteral() const {
    return TypeField::decode(code_) == kArrayLiteralExpression;
  }

  bool IsStringLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression;
  }

  bool IsUseStrictLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression &&
           IsUseStrictField::decode(code_);
  }

  bool IsUseStrongLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression &&
           IsUseStrongField::decode(code_);
  }

  bool IsThis() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kThisExpression;
  }

  bool IsThisProperty() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kThisPropertyExpression;
  }

  bool IsProperty() const {
    return TypeField::decode(code_) == kExpression &&
           (ExpressionTypeField::decode(code_) == kPropertyExpression ||
            ExpressionTypeField::decode(code_) == kThisPropertyExpression);
  }

  bool IsCall() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kCallExpression;
  }

  bool IsSuperCallReference() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kSuperCallReference;
  }

  bool IsValidReferenceExpression() const {
    return IsIdentifier() || IsProperty();
  }

  // At the moment PreParser doesn't track these expression types.
  bool IsFunctionLiteral() const { return false; }
  bool IsCallNew() const { return false; }

  bool IsNoTemplateTag() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kNoTemplateTagExpression;
  }

  bool IsSpreadExpression() const {
    return TypeField::decode(code_) == kSpreadExpression;
  }

  PreParserExpression AsFunctionLiteral() { return *this; }

  bool IsBinaryOperation() const {
    return TypeField::decode(code_) == kBinaryOperationExpression;
  }

  // Dummy implementation for making expression->somefunc() work in both Parser
  // and PreParser.
  PreParserExpression* operator->() { return this; }

  // More dummy implementations of things PreParser doesn't need to track:
  void set_index(int index) {}  // For YieldExpressions
  void set_should_eager_compile() {}

  int position() const { return RelocInfo::kNoPosition; }
  void set_function_token_position(int position) {}

  // Parenthesized expressions in the form `( Expression )`.
  void set_is_parenthesized() {
    code_ = ParenthesizedField::update(code_, true);
  }
  bool is_parenthesized() const { return ParenthesizedField::decode(code_); }

 private:
  enum Type {
    kExpression,
    kIdentifierExpression,
    kStringLiteralExpression,
    kBinaryOperationExpression,
    kSpreadExpression,
    kObjectLiteralExpression,
    kArrayLiteralExpression
  };

  enum ExpressionType {
    kThisExpression,
    kThisPropertyExpression,
    kPropertyExpression,
    kCallExpression,
    kSuperCallReference,
    kNoTemplateTagExpression,
    kAssignment
  };

  explicit PreParserExpression(uint32_t expression_code)
      : code_(expression_code) {}

  // The first three bits are for the Type.
  typedef BitField<Type, 0, 3> TypeField;

  // The high order bit applies only to nodes which would inherit from the
  // Expression ASTNode --- This is by necessity, due to the fact that
  // Expression nodes may be represented as multiple Types, not exclusively
  // through kExpression.
  // TODO(caitp, adamk): clean up PreParserExpression bitfields.
  typedef BitField<bool, 31, 1> ParenthesizedField;

  // The rest of the bits are interpreted depending on the value
  // of the Type field, so they can share the storage.
  typedef BitField<ExpressionType, TypeField::kNext, 3> ExpressionTypeField;
  typedef BitField<bool, TypeField::kNext, 1> IsUseStrictField;
  typedef BitField<bool, IsUseStrictField::kNext, 1> IsUseStrongField;
  typedef BitField<PreParserIdentifier::Type, TypeField::kNext, 10>
      IdentifierTypeField;
  typedef BitField<bool, TypeField::kNext, 1> HasCoverInitializedNameField;

  uint32_t code_;
};


// The pre-parser doesn't need to build lists of expressions, identifiers, or
// the like.
template <typename T>
class PreParserList {
 public:
  // These functions make list->Add(some_expression) work (and do nothing).
  PreParserList() : length_(0) {}
  PreParserList* operator->() { return this; }
  void Add(T, void*) { ++length_; }
  int length() const { return length_; }
 private:
  int length_;
};


typedef PreParserList<PreParserExpression> PreParserExpressionList;


class PreParserStatement {
 public:
  static PreParserStatement Default() {
    return PreParserStatement(kUnknownStatement);
  }

  static PreParserStatement Jump() {
    return PreParserStatement(kJumpStatement);
  }

  static PreParserStatement FunctionDeclaration() {
    return PreParserStatement(kFunctionDeclaration);
  }

  // Creates expression statement from expression.
  // Preserves being an unparenthesized string literal, possibly
  // "use strict".
  static PreParserStatement ExpressionStatement(
      PreParserExpression expression) {
    if (expression.IsUseStrictLiteral()) {
      return PreParserStatement(kUseStrictExpressionStatement);
    }
    if (expression.IsUseStrongLiteral()) {
      return PreParserStatement(kUseStrongExpressionStatement);
    }
    if (expression.IsStringLiteral()) {
      return PreParserStatement(kStringLiteralExpressionStatement);
    }
    return Default();
  }

  bool IsStringLiteral() {
    return code_ == kStringLiteralExpressionStatement;
  }

  bool IsUseStrictLiteral() {
    return code_ == kUseStrictExpressionStatement;
  }

  bool IsUseStrongLiteral() { return code_ == kUseStrongExpressionStatement; }

  bool IsFunctionDeclaration() {
    return code_ == kFunctionDeclaration;
  }

  bool IsJumpStatement() {
    return code_ == kJumpStatement;
  }

 private:
  enum Type {
    kUnknownStatement,
    kJumpStatement,
    kStringLiteralExpressionStatement,
    kUseStrictExpressionStatement,
    kUseStrongExpressionStatement,
    kFunctionDeclaration
  };

  explicit PreParserStatement(Type code) : code_(code) {}
  Type code_;
};


typedef PreParserList<PreParserStatement> PreParserStatementList;


class PreParserFactory {
 public:
  explicit PreParserFactory(void* unused_value_factory) {}
  PreParserExpression NewStringLiteral(PreParserIdentifier identifier,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewNumberLiteral(double number,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewRegExpLiteral(PreParserIdentifier js_pattern,
                                       int js_flags, int literal_index,
                                       bool is_strong, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewArrayLiteral(PreParserExpressionList values,
                                      int literal_index,
                                      bool is_strong,
                                      int pos) {
    return PreParserExpression::ArrayLiteral();
  }
  PreParserExpression NewArrayLiteral(PreParserExpressionList values,
                                      int first_spread_index, int literal_index,
                                      bool is_strong, int pos) {
    return PreParserExpression::ArrayLiteral();
  }
  PreParserExpression NewObjectLiteralProperty(PreParserExpression key,
                                               PreParserExpression value,
                                               ObjectLiteralProperty::Kind kind,
                                               bool is_static,
                                               bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(PreParserExpression key,
                                               PreParserExpression value,
                                               bool is_static,
                                               bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteral(PreParserExpressionList properties,
                                       int literal_index,
                                       int boilerplate_properties,
                                       bool has_function,
                                       bool is_strong,
                                       int pos) {
    return PreParserExpression::ObjectLiteral();
  }
  PreParserExpression NewVariableProxy(void* variable) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewProperty(PreParserExpression obj,
                                  PreParserExpression key,
                                  int pos) {
    if (obj.IsThis()) {
      return PreParserExpression::ThisProperty();
    }
    return PreParserExpression::Property();
  }
  PreParserExpression NewUnaryOperation(Token::Value op,
                                        PreParserExpression expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewBinaryOperation(Token::Value op,
                                         PreParserExpression left,
                                         PreParserExpression right, int pos) {
    return PreParserExpression::BinaryOperation(left, op, right);
  }
  PreParserExpression NewCompareOperation(Token::Value op,
                                          PreParserExpression left,
                                          PreParserExpression right, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewRewritableAssignmentExpression(
      PreParserExpression expression) {
    return expression;
  }
  PreParserExpression NewAssignment(Token::Value op,
                                    PreParserExpression left,
                                    PreParserExpression right,
                                    int pos) {
    return PreParserExpression::Assignment();
  }
  PreParserExpression NewYield(PreParserExpression generator_object,
                               PreParserExpression expression,
                               Yield::Kind yield_kind,
                               int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewConditional(PreParserExpression condition,
                                     PreParserExpression then_expression,
                                     PreParserExpression else_expression,
                                     int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCountOperation(Token::Value op,
                                        bool is_prefix,
                                        PreParserExpression expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCall(PreParserExpression expression,
                              PreParserExpressionList arguments,
                              int pos) {
    return PreParserExpression::Call();
  }
  PreParserExpression NewCallNew(PreParserExpression expression,
                                 PreParserExpressionList arguments,
                                 int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCallRuntime(const AstRawString* name,
                                     const Runtime::Function* function,
                                     PreParserExpressionList arguments,
                                     int pos) {
    return PreParserExpression::Default();
  }
  PreParserStatement NewReturnStatement(PreParserExpression expression,
                                        int pos) {
    return PreParserStatement::Default();
  }
  PreParserExpression NewFunctionLiteral(
      PreParserIdentifier name, Scope* scope, PreParserStatementList body,
      int materialized_literal_count, int expected_property_count,
      int parameter_count,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::EagerCompileHint eager_compile_hint, FunctionKind kind,
      int position) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewSpread(PreParserExpression expression, int pos) {
    return PreParserExpression::Spread(expression);
  }

  PreParserExpression NewEmptyParentheses(int pos) {
    return PreParserExpression::Default();
  }

  // Return the object itself as AstVisitor and implement the needed
  // dummy method right in this class.
  PreParserFactory* visitor() { return this; }
  int* ast_properties() {
    static int dummy = 42;
    return &dummy;
  }
};


struct PreParserFormalParameters : FormalParametersBase {
  explicit PreParserFormalParameters(Scope* scope)
      : FormalParametersBase(scope) {}
  int arity = 0;

  int Arity() const { return arity; }
  PreParserIdentifier at(int i) { return PreParserIdentifier(); }  // Dummy
};


class PreParser;

class PreParserTraits {
 public:
  struct Type {
    // TODO(marja): To be removed. The Traits object should contain all the data
    // it needs.
    typedef PreParser* Parser;

    // PreParser doesn't need to store generator variables.
    typedef void GeneratorVariable;

    typedef int AstProperties;

    // Return types for traversing functions.
    typedef PreParserIdentifier Identifier;
    typedef PreParserExpression Expression;
    typedef PreParserExpression YieldExpression;
    typedef PreParserExpression FunctionLiteral;
    typedef PreParserExpression ClassLiteral;
    typedef PreParserExpression ObjectLiteralProperty;
    typedef PreParserExpression Literal;
    typedef PreParserExpressionList ExpressionList;
    typedef PreParserExpressionList PropertyList;
    typedef PreParserIdentifier FormalParameter;
    typedef PreParserFormalParameters FormalParameters;
    typedef PreParserStatementList StatementList;

    // For constructing objects returned by the traversing functions.
    typedef PreParserFactory Factory;
  };

  explicit PreParserTraits(PreParser* pre_parser) : pre_parser_(pre_parser) {}

  // Helper functions for recursive descent.
  static bool IsEval(PreParserIdentifier identifier) {
    return identifier.IsEval();
  }

  static bool IsArguments(PreParserIdentifier identifier) {
    return identifier.IsArguments();
  }

  static bool IsEvalOrArguments(PreParserIdentifier identifier) {
    return identifier.IsEvalOrArguments();
  }

  static bool IsUndefined(PreParserIdentifier identifier) {
    return identifier.IsUndefined();
  }

  static bool IsPrototype(PreParserIdentifier identifier) {
    return identifier.IsPrototype();
  }

  static bool IsConstructor(PreParserIdentifier identifier) {
    return identifier.IsConstructor();
  }

  // Returns true if the expression is of type "this.foo".
  static bool IsThisProperty(PreParserExpression expression) {
    return expression.IsThisProperty();
  }

  static bool IsIdentifier(PreParserExpression expression) {
    return expression.IsIdentifier();
  }

  static PreParserIdentifier AsIdentifier(PreParserExpression expression) {
    return expression.AsIdentifier();
  }

  static bool IsFutureStrictReserved(PreParserIdentifier identifier) {
    return identifier.IsFutureStrictReserved();
  }

  static bool IsBoilerplateProperty(PreParserExpression property) {
    // PreParser doesn't count boilerplate properties.
    return false;
  }

  static bool IsArrayIndex(PreParserIdentifier string, uint32_t* index) {
    return false;
  }

  static PreParserExpression GetPropertyValue(PreParserExpression property) {
    return PreParserExpression::Default();
  }

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  static void PushLiteralName(FuncNameInferrer* fni, PreParserIdentifier id) {
    // PreParser should not use FuncNameInferrer.
    UNREACHABLE();
  }

  static void PushPropertyName(FuncNameInferrer* fni,
                               PreParserExpression expression) {
    // PreParser should not use FuncNameInferrer.
    UNREACHABLE();
  }

  static void InferFunctionName(FuncNameInferrer* fni,
                                PreParserExpression expression) {
    // PreParser should not use FuncNameInferrer.
    UNREACHABLE();
  }

  static void CheckFunctionLiteralInsideTopLevelObjectLiteral(
      Scope* scope, PreParserExpression property, bool* has_function) {}

  static void CheckAssigningFunctionLiteralToProperty(
      PreParserExpression left, PreParserExpression right) {}

  static PreParserExpression MarkExpressionAsAssigned(
      PreParserExpression expression) {
    // TODO(marja): To be able to produce the same errors, the preparser needs
    // to start tracking which expressions are variables and which are assigned.
    return expression;
  }

  bool ShortcutNumericLiteralBinaryExpression(PreParserExpression* x,
                                              PreParserExpression y,
                                              Token::Value op,
                                              int pos,
                                              PreParserFactory* factory) {
    return false;
  }

  PreParserExpression BuildUnaryExpression(PreParserExpression expression,
                                           Token::Value op, int pos,
                                           PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewThrowReferenceError(MessageTemplate::Template message,
                                             int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewThrowSyntaxError(MessageTemplate::Template message,
                                          Handle<Object> arg, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewThrowTypeError(MessageTemplate::Template message,
                                        Handle<Object> arg, int pos) {
    return PreParserExpression::Default();
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location location,
                       MessageTemplate::Template message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);
  void ReportMessageAt(int start_pos, int end_pos,
                       MessageTemplate::Template message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);

  // "null" return type creators.
  static PreParserIdentifier EmptyIdentifier() {
    return PreParserIdentifier::Default();
  }
  static PreParserIdentifier EmptyIdentifierString() {
    return PreParserIdentifier::Default();
  }
  static PreParserExpression EmptyExpression() {
    return PreParserExpression::Default();
  }
  static PreParserExpression EmptyLiteral() {
    return PreParserExpression::Default();
  }
  static PreParserExpression EmptyObjectLiteralProperty() {
    return PreParserExpression::Default();
  }
  static PreParserExpression EmptyFunctionLiteral() {
    return PreParserExpression::Default();
  }
  static PreParserExpressionList NullExpressionList() {
    return PreParserExpressionList();
  }

  // Odd-ball literal creators.
  static PreParserExpression GetLiteralTheHole(int position,
                                               PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  // Producing data during the recursive descent.
  PreParserIdentifier GetSymbol(Scanner* scanner);
  PreParserIdentifier GetNumberAsSymbol(Scanner* scanner);

  static PreParserIdentifier GetNextSymbol(Scanner* scanner) {
    return PreParserIdentifier::Default();
  }

  static PreParserExpression ThisExpression(Scope* scope,
                                            PreParserFactory* factory,
                                            int pos) {
    return PreParserExpression::This();
  }

  static PreParserExpression SuperPropertyReference(Scope* scope,
                                                    PreParserFactory* factory,
                                                    int pos) {
    return PreParserExpression::Default();
  }

  static PreParserExpression SuperCallReference(Scope* scope,
                                                PreParserFactory* factory,
                                                int pos) {
    return PreParserExpression::SuperCallReference();
  }

  static PreParserExpression NewTargetExpression(Scope* scope,
                                                 PreParserFactory* factory,
                                                 int pos) {
    return PreParserExpression::Default();
  }

  static PreParserExpression DefaultConstructor(bool call_super, Scope* scope,
                                                int pos, int end_pos) {
    return PreParserExpression::Default();
  }

  static PreParserExpression ExpressionFromLiteral(
      Token::Value token, int pos, Scanner* scanner,
      PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  static PreParserExpression ExpressionFromIdentifier(
      PreParserIdentifier name, int start_position, int end_position,
      Scope* scope, PreParserFactory* factory) {
    return PreParserExpression::FromIdentifier(name);
  }

  PreParserExpression ExpressionFromString(int pos,
                                           Scanner* scanner,
                                           PreParserFactory* factory = NULL);

  PreParserExpression GetIterator(PreParserExpression iterable,
                                  PreParserFactory* factory, int pos) {
    return PreParserExpression::Default();
  }

  static PreParserExpressionList NewExpressionList(int size, Zone* zone) {
    return PreParserExpressionList();
  }

  static PreParserStatementList NewStatementList(int size, Zone* zone) {
    return PreParserStatementList();
  }

  static PreParserExpressionList NewPropertyList(int size, Zone* zone) {
    return PreParserExpressionList();
  }

  static void AddParameterInitializationBlock(
      const PreParserFormalParameters& parameters,
      PreParserStatementList list, bool* ok) {}

  V8_INLINE void SkipLazyFunctionBody(int* materialized_literal_count,
                                      int* expected_property_count, bool* ok) {
    UNREACHABLE();
  }

  V8_INLINE PreParserStatementList ParseEagerFunctionBody(
      PreParserIdentifier function_name, int pos,
      const PreParserFormalParameters& parameters, FunctionKind kind,
      FunctionLiteral::FunctionType function_type, bool* ok);

  V8_INLINE void ParseArrowFunctionFormalParameterList(
      PreParserFormalParameters* parameters,
      PreParserExpression expression, const Scanner::Location& params_loc,
      Scanner::Location* duplicate_loc, bool* ok);

  void ReindexLiterals(const PreParserFormalParameters& paramaters) {}

  struct TemplateLiteralState {};

  TemplateLiteralState OpenTemplateLiteral(int pos) {
    return TemplateLiteralState();
  }
  void AddTemplateSpan(TemplateLiteralState*, bool) {}
  void AddTemplateExpression(TemplateLiteralState*, PreParserExpression) {}
  PreParserExpression CloseTemplateLiteral(TemplateLiteralState*, int,
                                           PreParserExpression tag) {
    if (IsTaggedTemplate(tag)) {
      // Emulate generation of array literals for tag callsite
      // 1st is array of cooked strings, second is array of raw strings
      MaterializeTemplateCallsiteLiterals();
    }
    return EmptyExpression();
  }
  inline void MaterializeTemplateCallsiteLiterals();
  PreParserExpression NoTemplateTag() {
    return PreParserExpression::NoTemplateTag();
  }
  static bool IsTaggedTemplate(const PreParserExpression tag) {
    return !tag.IsNoTemplateTag();
  }

  void AddFormalParameter(PreParserFormalParameters* parameters,
                          PreParserExpression pattern,
                          PreParserExpression initializer,
                          int initializer_end_position, bool is_rest) {
    ++parameters->arity;
  }
  void DeclareFormalParameter(Scope* scope, PreParserIdentifier parameter,
                              ExpressionClassifier* classifier) {
    if (!classifier->is_simple_parameter_list()) {
      scope->SetHasNonSimpleParameters();
    }
  }

  void CheckConflictingVarDeclarations(Scope* scope, bool* ok) {}

  // Temporary glue; these functions will move to ParserBase.
  PreParserExpression ParseV8Intrinsic(bool* ok);
  V8_INLINE PreParserExpression ParseDoExpression(bool* ok);
  PreParserExpression ParseFunctionLiteral(
      PreParserIdentifier name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction,
      LanguageMode language_mode, bool* ok);

  PreParserExpression ParseClassLiteral(PreParserIdentifier name,
                                        Scanner::Location class_name_location,
                                        bool name_is_strict_reserved, int pos,
                                        bool* ok);

  PreParserExpressionList PrepareSpreadArguments(PreParserExpressionList list) {
    return list;
  }

  inline void MaterializeUnspreadArgumentsLiterals(int count);

  inline PreParserExpression SpreadCall(PreParserExpression function,
                                        PreParserExpressionList args, int pos);

  inline PreParserExpression SpreadCallNew(PreParserExpression function,
                                           PreParserExpressionList args,
                                           int pos);

  inline void RewriteDestructuringAssignments() {}

  inline void QueueDestructuringAssignmentForRewriting(PreParserExpression) {}

  void SetFunctionNameFromPropertyName(PreParserExpression,
                                       PreParserIdentifier) {}
  void SetFunctionNameFromIdentifierRef(PreParserExpression,
                                        PreParserExpression) {}

  inline PreParserExpression RewriteNonPattern(
      PreParserExpression expr, const ExpressionClassifier* classifier,
      bool* ok);
  inline PreParserExpression RewriteNonPatternArguments(
      PreParserExpression args, const ExpressionClassifier* classifier,
      bool* ok);
  inline PreParserExpression RewriteNonPatternObjectLiteralProperty(
      PreParserExpression property, const ExpressionClassifier* classifier,
      bool* ok);

 private:
  PreParser* pre_parser_;
};


// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparse-data-format.h for the data format.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.
class PreParser : public ParserBase<PreParserTraits> {
 public:
  typedef PreParserIdentifier Identifier;
  typedef PreParserExpression Expression;
  typedef PreParserStatement Statement;

  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseSuccess
  };

  PreParser(Zone* zone, Scanner* scanner, AstValueFactory* ast_value_factory,
            ParserRecorder* log, uintptr_t stack_limit)
      : ParserBase<PreParserTraits>(zone, scanner, stack_limit, NULL,
                                    ast_value_factory, log, this) {}

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram(int* materialized_literals = 0) {
    Scope* scope = NewScope(scope_, SCRIPT_SCOPE);
    PreParserFactory factory(NULL);
    FunctionState top_scope(&function_state_, &scope_, scope, kNormalFunction,
                            &factory);
    bool ok = true;
    int start_position = scanner()->peek_location().beg_pos;
    ParseStatementList(Token::EOS, &ok);
    if (stack_overflow()) return kPreParseStackOverflow;
    if (!ok) {
      ReportUnexpectedToken(scanner()->current_token());
    } else if (is_strict(scope_->language_mode())) {
      CheckStrictOctalLiteral(start_position, scanner()->location().end_pos,
                              &ok);
    }
    if (materialized_literals) {
      *materialized_literals = function_state_->materialized_literal_count();
    }
    return kPreParseSuccess;
  }

  // Parses a single function literal, from the opening parentheses before
  // parameters to the closing brace after the body.
  // Returns a FunctionEntry describing the body of the function in enough
  // detail that it can be lazily compiled.
  // The scanner is expected to have matched the "function" or "function*"
  // keyword and parameters, and have consumed the initial '{'.
  // At return, unless an error occurred, the scanner is positioned before the
  // the final '}'.
  PreParseResult PreParseLazyFunction(
      LanguageMode language_mode, FunctionKind kind, bool has_simple_parameters,
      ParserRecorder* log, Scanner::BookmarkScope* bookmark = nullptr);

 private:
  friend class PreParserTraits;

  static const int kLazyParseTrialLimit = 200;

  // These types form an algebra over syntactic categories that is just
  // rich enough to let us recognize and propagate the constructs that
  // are either being counted in the preparser data, or is important
  // to throw the correct syntax error exceptions.

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  Statement ParseStatementListItem(bool* ok);
  void ParseStatementList(int end_token, bool* ok,
                          Scanner::BookmarkScope* bookmark = nullptr);
  Statement ParseStatement(bool* ok);
  Statement ParseSubStatement(bool* ok);
  Statement ParseFunctionDeclaration(bool* ok);
  Statement ParseClassDeclaration(bool* ok);
  Statement ParseBlock(bool* ok);
  Statement ParseVariableStatement(VariableDeclarationContext var_context,
                                   bool* ok);
  Statement ParseVariableDeclarations(VariableDeclarationContext var_context,
                                      int* num_decl, bool* is_lexical,
                                      bool* is_binding_pattern,
                                      Scanner::Location* first_initializer_loc,
                                      Scanner::Location* bindings_loc,
                                      bool* ok);
  Statement ParseExpressionOrLabelledStatement(bool* ok);
  Statement ParseIfStatement(bool* ok);
  Statement ParseContinueStatement(bool* ok);
  Statement ParseBreakStatement(bool* ok);
  Statement ParseReturnStatement(bool* ok);
  Statement ParseWithStatement(bool* ok);
  Statement ParseSwitchStatement(bool* ok);
  Statement ParseDoWhileStatement(bool* ok);
  Statement ParseWhileStatement(bool* ok);
  Statement ParseForStatement(bool* ok);
  Statement ParseThrowStatement(bool* ok);
  Statement ParseTryStatement(bool* ok);
  Statement ParseDebuggerStatement(bool* ok);
  Expression ParseConditionalExpression(bool accept_IN, bool* ok);
  Expression ParseObjectLiteral(bool* ok);
  Expression ParseV8Intrinsic(bool* ok);
  Expression ParseDoExpression(bool* ok);

  V8_INLINE void SkipLazyFunctionBody(int* materialized_literal_count,
                                      int* expected_property_count, bool* ok);
  V8_INLINE PreParserStatementList ParseEagerFunctionBody(
      PreParserIdentifier function_name, int pos,
      const PreParserFormalParameters& parameters, FunctionKind kind,
      FunctionLiteral::FunctionType function_type, bool* ok);

  Expression ParseFunctionLiteral(
      Identifier name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_pos, FunctionLiteral::FunctionType function_type,
      FunctionLiteral::ArityRestriction arity_restriction,
      LanguageMode language_mode, bool* ok);
  void ParseLazyFunctionLiteralBody(bool* ok,
                                    Scanner::BookmarkScope* bookmark = nullptr);

  PreParserExpression ParseClassLiteral(PreParserIdentifier name,
                                        Scanner::Location class_name_location,
                                        bool name_is_strict_reserved, int pos,
                                        bool* ok);
};


void PreParserTraits::MaterializeTemplateCallsiteLiterals() {
  pre_parser_->function_state_->NextMaterializedLiteralIndex();
  pre_parser_->function_state_->NextMaterializedLiteralIndex();
}


void PreParserTraits::MaterializeUnspreadArgumentsLiterals(int count) {
  for (int i = 0; i < count; ++i) {
    pre_parser_->function_state_->NextMaterializedLiteralIndex();
  }
}


PreParserExpression PreParserTraits::SpreadCall(PreParserExpression function,
                                                PreParserExpressionList args,
                                                int pos) {
  return pre_parser_->factory()->NewCall(function, args, pos);
}

PreParserExpression PreParserTraits::SpreadCallNew(PreParserExpression function,
                                                   PreParserExpressionList args,
                                                   int pos) {
  return pre_parser_->factory()->NewCallNew(function, args, pos);
}


void PreParserTraits::ParseArrowFunctionFormalParameterList(
    PreParserFormalParameters* parameters,
    PreParserExpression params, const Scanner::Location& params_loc,
    Scanner::Location* duplicate_loc, bool* ok) {
  // TODO(wingo): Detect duplicated identifiers in paramlists.  Detect parameter
  // lists that are too long.
}


PreParserExpression PreParserTraits::ParseDoExpression(bool* ok) {
  return pre_parser_->ParseDoExpression(ok);
}


PreParserExpression PreParserTraits::RewriteNonPattern(
    PreParserExpression expr, const ExpressionClassifier* classifier,
    bool* ok) {
  pre_parser_->ValidateExpression(classifier, ok);
  return expr;
}


PreParserExpression PreParserTraits::RewriteNonPatternArguments(
    PreParserExpression args, const ExpressionClassifier* classifier,
    bool* ok) {
  pre_parser_->ValidateExpression(classifier, ok);
  return args;
}


PreParserExpression PreParserTraits::RewriteNonPatternObjectLiteralProperty(
    PreParserExpression property, const ExpressionClassifier* classifier,
    bool* ok) {
  pre_parser_->ValidateExpression(classifier, ok);
  return property;
}


PreParserStatementList PreParser::ParseEagerFunctionBody(
    PreParserIdentifier function_name, int pos,
    const PreParserFormalParameters& parameters, FunctionKind kind,
    FunctionLiteral::FunctionType function_type, bool* ok) {
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  ParseStatementList(Token::RBRACE, ok);
  if (!*ok) return PreParserStatementList();

  Expect(Token::RBRACE, ok);
  return PreParserStatementList();
}


PreParserStatementList PreParserTraits::ParseEagerFunctionBody(
    PreParserIdentifier function_name, int pos,
    const PreParserFormalParameters& parameters, FunctionKind kind,
    FunctionLiteral::FunctionType function_type, bool* ok) {
  return pre_parser_->ParseEagerFunctionBody(function_name, pos, parameters,
                                             kind, function_type, ok);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSER_H
