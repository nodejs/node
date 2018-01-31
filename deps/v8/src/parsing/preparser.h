// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSER_H
#define V8_PARSING_PREPARSER_H

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/preparse-data.h"
#include "src/pending-compilation-error-handler.h"

namespace v8 {
namespace internal {

// Whereas the Parser generates AST during the recursive descent,
// the PreParser doesn't create a tree. Instead, it passes around minimal
// data objects (PreParserExpression, PreParserIdentifier etc.) which contain
// just enough data for the upper layer functions. PreParserFactory is
// responsible for creating these dummy objects. It provides a similar kind of
// interface as AstNodeFactory, so ParserBase doesn't need to care which one is
// used.

class ProducedPreParsedScopeData;

class PreParserIdentifier {
 public:
  PreParserIdentifier() : type_(kUnknownIdentifier) {}
  static PreParserIdentifier Default() {
    return PreParserIdentifier(kUnknownIdentifier);
  }
  static PreParserIdentifier Null() {
    return PreParserIdentifier(kNullIdentifier);
  }
  static PreParserIdentifier Eval() {
    return PreParserIdentifier(kEvalIdentifier);
  }
  static PreParserIdentifier Arguments() {
    return PreParserIdentifier(kArgumentsIdentifier);
  }
  static PreParserIdentifier Constructor() {
    return PreParserIdentifier(kConstructorIdentifier);
  }
  static PreParserIdentifier Await() {
    return PreParserIdentifier(kAwaitIdentifier);
  }
  static PreParserIdentifier Async() {
    return PreParserIdentifier(kAsyncIdentifier);
  }
  static PreParserIdentifier Name() {
    return PreParserIdentifier(kNameIdentifier);
  }
  bool IsNull() const { return type_ == kNullIdentifier; }
  bool IsEval() const { return type_ == kEvalIdentifier; }
  bool IsArguments() const { return type_ == kArgumentsIdentifier; }
  bool IsEvalOrArguments() const { return IsEval() || IsArguments(); }
  bool IsConstructor() const { return type_ == kConstructorIdentifier; }
  bool IsAwait() const { return type_ == kAwaitIdentifier; }
  bool IsName() const { return type_ == kNameIdentifier; }

 private:
  enum Type {
    kNullIdentifier,
    kUnknownIdentifier,
    kEvalIdentifier,
    kArgumentsIdentifier,
    kConstructorIdentifier,
    kAwaitIdentifier,
    kAsyncIdentifier,
    kNameIdentifier
  };

  explicit PreParserIdentifier(Type type) : type_(type), string_(nullptr) {}
  Type type_;
  // Only non-nullptr when PreParser.track_unresolved_variables_ is true.
  const AstRawString* string_;
  friend class PreParserExpression;
  friend class PreParser;
  friend class PreParserFactory;
};


class PreParserExpression {
 public:
  PreParserExpression()
      : code_(TypeField::encode(kNull)), variables_(nullptr) {}

  static PreParserExpression Null() { return PreParserExpression(); }

  static PreParserExpression Default(
      ZoneList<VariableProxy*>* variables = nullptr) {
    return PreParserExpression(TypeField::encode(kExpression), variables);
  }

  static PreParserExpression Spread(const PreParserExpression& expression) {
    return PreParserExpression(TypeField::encode(kSpreadExpression),
                               expression.variables_);
  }

  static PreParserExpression FromIdentifier(const PreParserIdentifier& id,
                                            VariableProxy* variable,
                                            Zone* zone) {
    PreParserExpression expression(TypeField::encode(kIdentifierExpression) |
                                   IdentifierTypeField::encode(id.type_));
    expression.AddVariable(variable, zone);
    return expression;
  }

  static PreParserExpression BinaryOperation(const PreParserExpression& left,
                                             Token::Value op,
                                             const PreParserExpression& right,
                                             Zone* zone) {
    if (op == Token::COMMA) {
      // Possibly an arrow function parameter list.
      if (left.variables_ == nullptr) {
        return PreParserExpression(TypeField::encode(kExpression),
                                   right.variables_);
      }
      if (right.variables_ != nullptr) {
        for (auto variable : *right.variables_) {
          left.variables_->Add(variable, zone);
        }
      }
      return PreParserExpression(TypeField::encode(kExpression),
                                 left.variables_);
    }
    return PreParserExpression(TypeField::encode(kExpression));
  }

  static PreParserExpression Assignment(ZoneList<VariableProxy*>* variables) {
    return PreParserExpression(TypeField::encode(kExpression) |
                                   ExpressionTypeField::encode(kAssignment),
                               variables);
  }

  static PreParserExpression NewTargetExpression() {
    return PreParserExpression::Default();
  }

  static PreParserExpression ObjectLiteral(
      ZoneList<VariableProxy*>* variables) {
    return PreParserExpression(TypeField::encode(kObjectLiteralExpression),
                               variables);
  }

  static PreParserExpression ArrayLiteral(ZoneList<VariableProxy*>* variables) {
    return PreParserExpression(TypeField::encode(kArrayLiteralExpression),
                               variables);
  }

  static PreParserExpression StringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression));
  }

  static PreParserExpression UseStrictStringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression) |
                               IsUseStrictField::encode(true));
  }

  static PreParserExpression UseAsmStringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression) |
                               IsUseAsmField::encode(true));
  }

  static PreParserExpression This(ZoneList<VariableProxy*>* variables) {
    return PreParserExpression(TypeField::encode(kExpression) |
                                   ExpressionTypeField::encode(kThisExpression),
                               variables);
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

  static PreParserExpression CallEval() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kCallEvalExpression));
  }

  static PreParserExpression CallTaggedTemplate() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kCallTaggedTemplateExpression));
  }

  bool is_tagged_template() const {
    DCHECK(IsCall());
    return ExpressionTypeField::decode(code_) == kCallTaggedTemplateExpression;
  }

  static PreParserExpression SuperCallReference() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kSuperCallReference));
  }

  bool IsNull() const { return TypeField::decode(code_) == kNull; }

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

  bool IsUseAsmLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression &&
           IsUseAsmField::decode(code_);
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
           (ExpressionTypeField::decode(code_) == kCallExpression ||
            ExpressionTypeField::decode(code_) == kCallEvalExpression ||
            ExpressionTypeField::decode(code_) ==
                kCallTaggedTemplateExpression);
  }
  PreParserExpression* AsCall() {
    if (IsCall()) return this;
    return nullptr;
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

  bool IsSpread() const {
    return TypeField::decode(code_) == kSpreadExpression;
  }

  PreParserExpression AsFunctionLiteral() { return *this; }

  // Dummy implementation for making expression->somefunc() work in both Parser
  // and PreParser.
  PreParserExpression* operator->() { return this; }

  // More dummy implementations of things PreParser doesn't need to track:
  void SetShouldEagerCompile() {}

  int position() const { return kNoSourcePosition; }
  void set_function_token_position(int position) {}
  void set_scope(Scope* scope) {}

 private:
  enum Type {
    kNull,
    kExpression,
    kIdentifierExpression,
    kStringLiteralExpression,
    kSpreadExpression,
    kObjectLiteralExpression,
    kArrayLiteralExpression
  };

  enum ExpressionType {
    kThisExpression,
    kThisPropertyExpression,
    kPropertyExpression,
    kCallExpression,
    kCallEvalExpression,
    kCallTaggedTemplateExpression,
    kSuperCallReference,
    kAssignment
  };

  explicit PreParserExpression(uint32_t expression_code,
                               ZoneList<VariableProxy*>* variables = nullptr)
      : code_(expression_code), variables_(variables) {}

  void AddVariable(VariableProxy* variable, Zone* zone) {
    if (variable == nullptr) {
      return;
    }
    if (variables_ == nullptr) {
      variables_ = new (zone) ZoneList<VariableProxy*>(1, zone);
    }
    variables_->Add(variable, zone);
  }

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
  typedef BitField<ExpressionType, TypeField::kNext, 4> ExpressionTypeField;
  typedef BitField<bool, TypeField::kNext, 1> IsUseStrictField;
  typedef BitField<bool, IsUseStrictField::kNext, 1> IsUseAsmField;
  typedef BitField<PreParserIdentifier::Type, TypeField::kNext, 10>
      IdentifierTypeField;
  typedef BitField<bool, TypeField::kNext, 1> HasCoverInitializedNameField;

  uint32_t code_;
  // If the PreParser is used in the variable tracking mode, PreParserExpression
  // accumulates variables in that expression.
  ZoneList<VariableProxy*>* variables_;

  friend class PreParser;
  friend class PreParserFactory;
  template <typename T>
  friend class PreParserList;
};


// The pre-parser doesn't need to build lists of expressions, identifiers, or
// the like. If the PreParser is used in variable tracking mode, it needs to
// build lists of variables though.
template <typename T>
class PreParserList {
 public:
  // These functions make list->Add(some_expression) work (and do nothing).
  PreParserList() : length_(0), variables_(nullptr) {}
  PreParserList* operator->() { return this; }
  void Add(const T& element, Zone* zone);
  int length() const { return length_; }
  static PreParserList Null() { return PreParserList(-1); }
  bool IsNull() const { return length_ == -1; }
  void Set(int index, const T& element) {}

 private:
  explicit PreParserList(int n) : length_(n), variables_(nullptr) {}
  int length_;
  ZoneList<VariableProxy*>* variables_;

  friend class PreParser;
  friend class PreParserFactory;
};

template <>
inline void PreParserList<PreParserExpression>::Add(
    const PreParserExpression& expression, Zone* zone) {
  if (expression.variables_ != nullptr) {
    DCHECK(FLAG_lazy_inner_functions);
    DCHECK_NOT_NULL(zone);
    if (variables_ == nullptr) {
      variables_ = new (zone) ZoneList<VariableProxy*>(1, zone);
    }
    for (auto identifier : (*expression.variables_)) {
      variables_->Add(identifier, zone);
    }
  }
  ++length_;
}

template <typename T>
void PreParserList<T>::Add(const T& element, Zone* zone) {
  ++length_;
}

typedef PreParserList<PreParserExpression> PreParserExpressionList;

class PreParserStatement;
typedef PreParserList<PreParserStatement> PreParserStatementList;

class PreParserStatement {
 public:
  static PreParserStatement Default() {
    return PreParserStatement(kUnknownStatement);
  }

  static PreParserStatement Null() {
    return PreParserStatement(kNullStatement);
  }

  static PreParserStatement Empty() {
    return PreParserStatement(kEmptyStatement);
  }

  static PreParserStatement Jump() {
    return PreParserStatement(kJumpStatement);
  }

  // Creates expression statement from expression.
  // Preserves being an unparenthesized string literal, possibly
  // "use strict".
  static PreParserStatement ExpressionStatement(
      const PreParserExpression& expression) {
    if (expression.IsUseStrictLiteral()) {
      return PreParserStatement(kUseStrictExpressionStatement);
    }
    if (expression.IsUseAsmLiteral()) {
      return PreParserStatement(kUseAsmExpressionStatement);
    }
    if (expression.IsStringLiteral()) {
      return PreParserStatement(kStringLiteralExpressionStatement);
    }
    return Default();
  }

  bool IsStringLiteral() {
    return code_ == kStringLiteralExpressionStatement || IsUseStrictLiteral() ||
           IsUseAsmLiteral();
  }

  bool IsUseStrictLiteral() {
    return code_ == kUseStrictExpressionStatement;
  }

  bool IsUseAsmLiteral() { return code_ == kUseAsmExpressionStatement; }

  bool IsJumpStatement() {
    return code_ == kJumpStatement;
  }

  bool IsNull() { return code_ == kNullStatement; }

  bool IsEmptyStatement() {
    DCHECK(!IsNull());
    return code_ == kEmptyStatement;
  }

  // Dummy implementation for making statement->somefunc() work in both Parser
  // and PreParser.
  PreParserStatement* operator->() { return this; }

  // TODO(adamk): These should return something even lighter-weight than
  // PreParserStatementList.
  PreParserStatementList statements() { return PreParserStatementList(); }
  PreParserStatementList cases() { return PreParserStatementList(); }

  void set_scope(Scope* scope) {}
  void Initialize(const PreParserExpression& cond, PreParserStatement body,
                  const SourceRange& body_range = {}) {}
  void Initialize(PreParserStatement init, const PreParserExpression& cond,
                  PreParserStatement next, PreParserStatement body,
                  const SourceRange& body_range = {}) {}

 private:
  enum Type {
    kNullStatement,
    kEmptyStatement,
    kUnknownStatement,
    kJumpStatement,
    kStringLiteralExpressionStatement,
    kUseStrictExpressionStatement,
    kUseAsmExpressionStatement,
  };

  explicit PreParserStatement(Type code) : code_(code) {}
  Type code_;
};


class PreParserFactory {
 public:
  explicit PreParserFactory(AstValueFactory* ast_value_factory, Zone* zone)
      : ast_node_factory_(ast_value_factory, zone), zone_(zone) {}

  void set_zone(Zone* zone) {
    ast_node_factory_.set_zone(zone);
    zone_ = zone;
  }

  AstNodeFactory* ast_node_factory() { return &ast_node_factory_; }

  PreParserExpression NewStringLiteral(const PreParserIdentifier& identifier,
                                       int pos) {
    // This is needed for object literal property names. Property names are
    // normalized to string literals during object literal parsing.
    PreParserExpression expression = PreParserExpression::Default();
    if (identifier.string_ != nullptr) {
      DCHECK(FLAG_lazy_inner_functions);
      VariableProxy* variable = ast_node_factory_.NewVariableProxy(
          identifier.string_, NORMAL_VARIABLE);
      expression.AddVariable(variable, zone_);
    }
    return expression;
  }
  PreParserExpression NewNumberLiteral(double number,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewUndefinedLiteral(int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewTheHoleLiteral() {
    return PreParserExpression::Default();
  }
  PreParserExpression NewRegExpLiteral(const PreParserIdentifier& js_pattern,
                                       int js_flags, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewArrayLiteral(const PreParserExpressionList& values,
                                      int first_spread_index, int pos) {
    return PreParserExpression::ArrayLiteral(values.variables_);
  }
  PreParserExpression NewClassLiteralProperty(const PreParserExpression& key,
                                              const PreParserExpression& value,
                                              ClassLiteralProperty::Kind kind,
                                              bool is_static,
                                              bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(const PreParserExpression& key,
                                               const PreParserExpression& value,
                                               ObjectLiteralProperty::Kind kind,
                                               bool is_computed_name) {
    return PreParserExpression::Default(value.variables_);
  }
  PreParserExpression NewObjectLiteralProperty(const PreParserExpression& key,
                                               const PreParserExpression& value,
                                               bool is_computed_name) {
    return PreParserExpression::Default(value.variables_);
  }
  PreParserExpression NewObjectLiteral(
      const PreParserExpressionList& properties, int boilerplate_properties,
      int pos, bool has_rest_property) {
    return PreParserExpression::ObjectLiteral(properties.variables_);
  }
  PreParserExpression NewVariableProxy(void* variable) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewProperty(const PreParserExpression& obj,
                                  const PreParserExpression& key, int pos) {
    if (obj.IsThis()) {
      return PreParserExpression::ThisProperty();
    }
    return PreParserExpression::Property();
  }
  PreParserExpression NewUnaryOperation(Token::Value op,
                                        const PreParserExpression& expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewBinaryOperation(Token::Value op,
                                         const PreParserExpression& left,
                                         const PreParserExpression& right,
                                         int pos) {
    return PreParserExpression::BinaryOperation(left, op, right, zone_);
  }
  PreParserExpression NewCompareOperation(Token::Value op,
                                          const PreParserExpression& left,
                                          const PreParserExpression& right,
                                          int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewRewritableExpression(
      const PreParserExpression& expression, Scope* scope) {
    return expression;
  }
  PreParserExpression NewAssignment(Token::Value op,
                                    const PreParserExpression& left,
                                    const PreParserExpression& right, int pos) {
    // Identifiers need to be tracked since this might be a parameter with a
    // default value inside an arrow function parameter list.
    return PreParserExpression::Assignment(left.variables_);
  }
  PreParserExpression NewYield(const PreParserExpression& expression, int pos,
                               Suspend::OnAbruptResume on_abrupt_resume) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewAwait(const PreParserExpression& expression, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewYieldStar(const PreParserExpression& iterable,
                                   int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewConditional(const PreParserExpression& condition,
                                     const PreParserExpression& then_expression,
                                     const PreParserExpression& else_expression,
                                     int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCountOperation(Token::Value op, bool is_prefix,
                                        const PreParserExpression& expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCall(
      PreParserExpression expression, const PreParserExpressionList& arguments,
      int pos, Call::PossiblyEval possibly_eval = Call::NOT_EVAL) {
    if (possibly_eval == Call::IS_POSSIBLY_EVAL) {
      DCHECK(expression.IsIdentifier() && expression.AsIdentifier().IsEval());
      return PreParserExpression::CallEval();
    }
    return PreParserExpression::Call();
  }
  PreParserExpression NewTaggedTemplate(
      PreParserExpression expression, const PreParserExpressionList& arguments,
      int pos) {
    return PreParserExpression::CallTaggedTemplate();
  }
  PreParserExpression NewCallNew(const PreParserExpression& expression,
                                 const PreParserExpressionList& arguments,
                                 int pos) {
    return PreParserExpression::Default();
  }
  PreParserStatement NewReturnStatement(
      const PreParserExpression& expression, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }
  PreParserStatement NewAsyncReturnStatement(
      const PreParserExpression& expression, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }
  PreParserExpression NewFunctionLiteral(
      const PreParserIdentifier& name, Scope* scope,
      PreParserStatementList body, int expected_property_count,
      int parameter_count, int function_length,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::EagerCompileHint eager_compile_hint, int position,
      bool has_braces, int function_literal_id,
      ProducedPreParsedScopeData* produced_preparsed_scope_data = nullptr) {
    DCHECK_NULL(produced_preparsed_scope_data);
    return PreParserExpression::Default();
  }

  PreParserExpression NewSpread(const PreParserExpression& expression, int pos,
                                int expr_pos) {
    return PreParserExpression::Spread(expression);
  }

  PreParserExpression NewEmptyParentheses(int pos) {
    return PreParserExpression::Default();
  }

  PreParserStatement NewEmptyStatement(int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewBlock(int capacity, bool ignore_completion_value,
                              ZoneList<const AstRawString*>* labels = nullptr) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewDebuggerStatement(int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewExpressionStatement(const PreParserExpression& expr,
                                            int pos) {
    return PreParserStatement::ExpressionStatement(expr);
  }

  PreParserStatement NewIfStatement(const PreParserExpression& condition,
                                    PreParserStatement then_statement,
                                    PreParserStatement else_statement, int pos,
                                    SourceRange then_range = {},
                                    SourceRange else_range = {}) {
    // This must return a jump statement iff both clauses are jump statements.
    return else_statement.IsJumpStatement() ? then_statement : else_statement;
  }

  PreParserStatement NewBreakStatement(
      PreParserStatement target, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }

  PreParserStatement NewContinueStatement(
      PreParserStatement target, int pos,
      int continuation_pos = kNoSourcePosition) {
    return PreParserStatement::Jump();
  }

  PreParserStatement NewWithStatement(Scope* scope,
                                      const PreParserExpression& expression,
                                      PreParserStatement statement, int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewDoWhileStatement(ZoneList<const AstRawString*>* labels,
                                         int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewWhileStatement(ZoneList<const AstRawString*>* labels,
                                       int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewSwitchStatement(ZoneList<const AstRawString*>* labels,
                                        const PreParserExpression& tag,
                                        int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewCaseClause(const PreParserExpression& label,
                                   PreParserStatementList statements) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewForStatement(ZoneList<const AstRawString*>* labels,
                                     int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewForEachStatement(ForEachStatement::VisitMode visit_mode,
                                         ZoneList<const AstRawString*>* labels,
                                         int pos) {
    return PreParserStatement::Default();
  }

  PreParserStatement NewForOfStatement(ZoneList<const AstRawString*>* labels,
                                       int pos) {
    return PreParserStatement::Default();
  }

  PreParserExpression NewCallRuntime(Runtime::FunctionId id,
                                     ZoneList<PreParserExpression>* arguments,
                                     int pos) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewImportCallExpression(const PreParserExpression& args,
                                              int pos) {
    return PreParserExpression::Default();
  }

 private:
  // For creating VariableProxy objects (if
  // PreParser::track_unresolved_variables_ is used).
  AstNodeFactory ast_node_factory_;
  Zone* zone_;
};


struct PreParserFormalParameters : FormalParametersBase {
  struct Parameter : public ZoneObject {
    Parameter(ZoneList<VariableProxy*>* variables, bool is_rest)
        : variables_(variables), is_rest(is_rest) {}
    Parameter** next() { return &next_parameter; }
    Parameter* const* next() const { return &next_parameter; }

    ZoneList<VariableProxy*>* variables_;
    Parameter* next_parameter = nullptr;
    bool is_rest : 1;
  };
  explicit PreParserFormalParameters(DeclarationScope* scope)
      : FormalParametersBase(scope) {}

  ThreadedList<Parameter> params;
};


class PreParser;

class PreParserTarget {
 public:
  PreParserTarget(ParserBase<PreParser>* preparser,
                  PreParserStatement statement) {}
};

class PreParserTargetScope {
 public:
  explicit PreParserTargetScope(ParserBase<PreParser>* preparser) {}
};

template <>
struct ParserTypes<PreParser> {
  typedef ParserBase<PreParser> Base;
  typedef PreParser Impl;

  // Return types for traversing functions.
  typedef PreParserIdentifier Identifier;
  typedef PreParserExpression Expression;
  typedef PreParserExpression FunctionLiteral;
  typedef PreParserExpression ObjectLiteralProperty;
  typedef PreParserExpression ClassLiteralProperty;
  typedef PreParserExpression Suspend;
  typedef PreParserExpression RewritableExpression;
  typedef PreParserExpressionList ExpressionList;
  typedef PreParserExpressionList ObjectPropertyList;
  typedef PreParserExpressionList ClassPropertyList;
  typedef PreParserFormalParameters FormalParameters;
  typedef PreParserStatement Statement;
  typedef PreParserStatementList StatementList;
  typedef PreParserStatement Block;
  typedef PreParserStatement BreakableStatement;
  typedef PreParserStatement IterationStatement;
  typedef PreParserStatement ForStatement;

  // For constructing objects returned by the traversing functions.
  typedef PreParserFactory Factory;

  typedef PreParserTarget Target;
  typedef PreParserTargetScope TargetScope;
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
class PreParser : public ParserBase<PreParser> {
  friend class ParserBase<PreParser>;
  friend class v8::internal::ExpressionClassifier<ParserTypes<PreParser>>;

 public:
  typedef PreParserIdentifier Identifier;
  typedef PreParserExpression Expression;
  typedef PreParserStatement Statement;

  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseAbort,
    kPreParseSuccess
  };

  PreParser(Zone* zone, Scanner* scanner, uintptr_t stack_limit,
            AstValueFactory* ast_value_factory,
            PendingCompilationErrorHandler* pending_error_handler,
            RuntimeCallStats* runtime_call_stats, Logger* logger,
            int script_id = -1, bool parsing_module = false,
            bool parsing_on_main_thread = true)
      : ParserBase<PreParser>(zone, scanner, stack_limit, nullptr,
                              ast_value_factory, pending_error_handler,
                              runtime_call_stats, logger, script_id,
                              parsing_module, parsing_on_main_thread),
        use_counts_(nullptr),
        track_unresolved_variables_(false),
        produced_preparsed_scope_data_(nullptr) {}

  static bool IsPreParser() { return true; }

  PreParserLogger* logger() { return &log_; }

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram();

  // Parses a single function literal, from the opening parentheses before
  // parameters to the closing brace after the body.
  // Returns a FunctionEntry describing the body of the function in enough
  // detail that it can be lazily compiled.
  // The scanner is expected to have matched the "function" or "function*"
  // keyword and parameters, and have consumed the initial '{'.
  // At return, unless an error occurred, the scanner is positioned before the
  // the final '}'.
  PreParseResult PreParseFunction(
      const AstRawString* function_name, FunctionKind kind,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope, bool track_unresolved_variables,
      bool may_abort, int* use_counts,
      ProducedPreParsedScopeData** produced_preparser_scope_data,
      int script_id);

  ProducedPreParsedScopeData* produced_preparsed_scope_data() const {
    return produced_preparsed_scope_data_;
  }

  void set_produced_preparsed_scope_data(
      ProducedPreParsedScopeData* produced_preparsed_scope_data) {
    produced_preparsed_scope_data_ = produced_preparsed_scope_data;
  }

 private:
  // These types form an algebra over syntactic categories that is just
  // rich enough to let us recognize and propagate the constructs that
  // are either being counted in the preparser data, or is important
  // to throw the correct syntax error exceptions.

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.

  // Indicates that we won't switch from the preparser to the preparser; we'll
  // just stay where we are.
  bool AllowsLazyParsingWithoutUnresolvedVariables() const { return false; }
  bool parse_lazily() const { return false; }

  PendingCompilationErrorHandler* pending_error_handler() {
    return pending_error_handler_;
  }

  V8_INLINE LazyParsingResult
  SkipFunction(const AstRawString* name, FunctionKind kind,
               FunctionLiteral::FunctionType function_type,
               DeclarationScope* function_scope, int* num_parameters,
               ProducedPreParsedScopeData** produced_preparsed_scope_data,
               bool is_inner_function, bool may_abort, bool* ok) {
    UNREACHABLE();
  }
  Expression ParseFunctionLiteral(Identifier name,
                                  Scanner::Location function_name_location,
                                  FunctionNameValidity function_name_validity,
                                  FunctionKind kind, int function_token_pos,
                                  FunctionLiteral::FunctionType function_type,
                                  LanguageMode language_mode, bool* ok);
  LazyParsingResult ParseStatementListAndLogFunction(
      PreParserFormalParameters* formals, bool maybe_abort, bool* ok);

  struct TemplateLiteralState {};

  V8_INLINE TemplateLiteralState OpenTemplateLiteral(int pos) {
    return TemplateLiteralState();
  }
  V8_INLINE void AddTemplateExpression(TemplateLiteralState* state,
                                       const PreParserExpression& expression) {}
  V8_INLINE void AddTemplateSpan(TemplateLiteralState* state, bool should_cook,
                                 bool tail) {}
  V8_INLINE PreParserExpression CloseTemplateLiteral(
      TemplateLiteralState* state, int start, const PreParserExpression& tag) {
    return PreParserExpression::Default();
  }
  V8_INLINE void CheckConflictingVarDeclarations(Scope* scope, bool* ok) {}

  V8_INLINE void SetLanguageMode(Scope* scope, LanguageMode mode) {
    scope->SetLanguageMode(mode);
  }
  V8_INLINE void SetAsmModule() {}

  V8_INLINE PreParserExpression SpreadCall(const PreParserExpression& function,
                                           const PreParserExpressionList& args,
                                           int pos,
                                           Call::PossiblyEval possibly_eval);
  V8_INLINE PreParserExpression
  SpreadCallNew(const PreParserExpression& function,
                const PreParserExpressionList& args, int pos);

  V8_INLINE void RewriteDestructuringAssignments() {}

  V8_INLINE void PrepareGeneratorVariables() {}
  V8_INLINE void RewriteAsyncFunctionBody(
      PreParserStatementList body, PreParserStatement block,
      const PreParserExpression& return_value, bool* ok) {}
  V8_INLINE void RewriteNonPattern(bool* ok) { ValidateExpression(ok); }

  void DeclareAndInitializeVariables(
      PreParserStatement block,
      const DeclarationDescriptor* declaration_descriptor,
      const DeclarationParsingResult::Declaration* declaration,
      ZoneList<const AstRawString*>* names, bool* ok);

  V8_INLINE ZoneList<const AstRawString*>* DeclareLabel(
      ZoneList<const AstRawString*>* labels, const PreParserExpression& expr,
      bool* ok) {
    DCHECK(!parsing_module_ || !expr.AsIdentifier().IsAwait());
    DCHECK(IsIdentifier(expr));
    return labels;
  }

  // TODO(nikolaos): The preparser currently does not keep track of labels.
  V8_INLINE bool ContainsLabel(ZoneList<const AstRawString*>* labels,
                               const PreParserIdentifier& label) {
    return false;
  }

  V8_INLINE PreParserExpression
  RewriteReturn(const PreParserExpression& return_value, int pos) {
    return return_value;
  }
  V8_INLINE PreParserStatement
  RewriteSwitchStatement(PreParserStatement switch_statement, Scope* scope) {
    return PreParserStatement::Default();
  }

  V8_INLINE void RewriteCatchPattern(CatchInfo* catch_info, bool* ok) {
    if (track_unresolved_variables_) {
      const AstRawString* catch_name = catch_info->name.string_;
      if (catch_name == nullptr) {
        catch_name = ast_value_factory()->dot_catch_string();
      }
      catch_info->scope->DeclareCatchVariableName(catch_name);

      if (catch_info->pattern.variables_ != nullptr) {
        for (auto variable : *catch_info->pattern.variables_) {
          scope()->DeclareVariableName(variable->raw_name(), LET);
        }
      }
    }
  }

  V8_INLINE void ValidateCatchBlock(const CatchInfo& catch_info, bool* ok) {}
  V8_INLINE PreParserStatement RewriteTryStatement(
      PreParserStatement try_block, PreParserStatement catch_block,
      const SourceRange& catch_range, PreParserStatement finally_block,
      const SourceRange& finally_range, const CatchInfo& catch_info, int pos) {
    return PreParserStatement::Default();
  }

  V8_INLINE void ParseAndRewriteGeneratorFunctionBody(
      int pos, FunctionKind kind, PreParserStatementList body, bool* ok) {
    ParseStatementList(body, Token::RBRACE, ok);
  }
  V8_INLINE void ParseAndRewriteAsyncGeneratorFunctionBody(
      int pos, FunctionKind kind, PreParserStatementList body, bool* ok) {
    ParseStatementList(body, Token::RBRACE, ok);
  }
  V8_INLINE void DeclareFunctionNameVar(
      const AstRawString* function_name,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope) {
    if (track_unresolved_variables_ &&
        function_type == FunctionLiteral::kNamedExpression &&
        function_scope->LookupLocal(function_name) == nullptr) {
      DCHECK_EQ(function_scope, scope());
      function_scope->DeclareFunctionVar(function_name);
    }
  }

  V8_INLINE void DeclareFunctionNameVar(
      const PreParserIdentifier& function_name,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope) {
    DeclareFunctionNameVar(function_name.string_, function_type,
                           function_scope);
  }

  V8_INLINE PreParserExpression RewriteDoExpression(PreParserStatement body,
                                                    int pos, bool* ok) {
    return PreParserExpression::Default();
  }

  // TODO(nikolaos): The preparser currently does not keep track of labels
  // and targets.
  V8_INLINE PreParserStatement
  LookupBreakTarget(const PreParserIdentifier& label, bool* ok) {
    return PreParserStatement::Default();
  }
  V8_INLINE PreParserStatement
  LookupContinueTarget(const PreParserIdentifier& label, bool* ok) {
    return PreParserStatement::Default();
  }

  V8_INLINE PreParserStatement
  DeclareFunction(const PreParserIdentifier& variable_name,
                  const PreParserExpression& function, VariableMode mode,
                  int pos, bool is_sloppy_block_function,
                  ZoneList<const AstRawString*>* names, bool* ok) {
    DCHECK_NULL(names);
    if (variable_name.string_ != nullptr) {
      DCHECK(track_unresolved_variables_);
      scope()->DeclareVariableName(variable_name.string_, mode);
      if (is_sloppy_block_function) {
        GetDeclarationScope()->DeclareSloppyBlockFunction(variable_name.string_,
                                                          scope());
      }
    }
    return Statement::Default();
  }

  V8_INLINE PreParserStatement DeclareClass(
      const PreParserIdentifier& variable_name,
      const PreParserExpression& value, ZoneList<const AstRawString*>* names,
      int class_token_pos, int end_pos, bool* ok) {
    // Preparser shouldn't be used in contexts where we need to track the names.
    DCHECK_NULL(names);
    if (variable_name.string_ != nullptr) {
      DCHECK(track_unresolved_variables_);
      scope()->DeclareVariableName(variable_name.string_, LET);
    }
    return PreParserStatement::Default();
  }
  V8_INLINE void DeclareClassVariable(const PreParserIdentifier& name,
                                      ClassInfo* class_info,
                                      int class_token_pos, bool* ok) {
    if (name.string_ != nullptr) {
      DCHECK(track_unresolved_variables_);
      scope()->DeclareVariableName(name.string_, CONST);
    }
  }
  V8_INLINE void DeclareClassProperty(const PreParserIdentifier& class_name,
                                      const PreParserExpression& property,
                                      ClassLiteralProperty::Kind kind,
                                      bool is_static, bool is_constructor,
                                      bool is_computed_name,
                                      ClassInfo* class_info, bool* ok) {
    if (kind == ClassLiteralProperty::FIELD && is_computed_name) {
      scope()->DeclareVariableName(
          ClassFieldVariableName(ast_value_factory(),
                                 class_info->computed_field_count),
          CONST);
    }
  }

  V8_INLINE PreParserExpression
  RewriteClassLiteral(Scope* scope, const PreParserIdentifier& name,
                      ClassInfo* class_info, int pos, int end_pos, bool* ok) {
    bool has_default_constructor = !class_info->has_seen_constructor;
    // Account for the default constructor.
    if (has_default_constructor) {
      // Creating and disposing of a FunctionState makes tracking of
      // next_function_is_likely_called match what Parser does. TODO(marja):
      // Make the lazy function + next_function_is_likely_called + default ctor
      // logic less surprising. Default ctors shouldn't affect the laziness of
      // functions.
      bool has_extends = class_info->extends.IsNull();
      FunctionKind kind = has_extends ? FunctionKind::kDefaultDerivedConstructor
                                      : FunctionKind::kDefaultBaseConstructor;
      DeclarationScope* function_scope = NewFunctionScope(kind);
      SetLanguageMode(function_scope, LanguageMode::kStrict);
      function_scope->set_start_position(pos);
      function_scope->set_end_position(pos);
      FunctionState function_state(&function_state_, &scope_, function_scope);
      GetNextFunctionLiteralId();
    }
    if (class_info->has_static_class_fields) {
      GetNextFunctionLiteralId();
    }
    if (class_info->has_instance_class_fields) {
      GetNextFunctionLiteralId();
    }
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserStatement DeclareNative(const PreParserIdentifier& name,
                                             int pos, bool* ok) {
    return PreParserStatement::Default();
  }

  V8_INLINE void QueueDestructuringAssignmentForRewriting(
      PreParserExpression assignment) {}
  V8_INLINE void QueueNonPatternForRewriting(const PreParserExpression& expr,
                                             bool* ok) {}

  // Helper functions for recursive descent.
  V8_INLINE bool IsEval(const PreParserIdentifier& identifier) const {
    return identifier.IsEval();
  }

  V8_INLINE bool IsArguments(const PreParserIdentifier& identifier) const {
    return identifier.IsArguments();
  }

  V8_INLINE bool IsEvalOrArguments(
      const PreParserIdentifier& identifier) const {
    return identifier.IsEvalOrArguments();
  }

  V8_INLINE bool IsAwait(const PreParserIdentifier& identifier) const {
    return identifier.IsAwait();
  }

  // Returns true if the expression is of type "this.foo".
  V8_INLINE static bool IsThisProperty(const PreParserExpression& expression) {
    return expression.IsThisProperty();
  }

  V8_INLINE static bool IsIdentifier(const PreParserExpression& expression) {
    return expression.IsIdentifier();
  }

  V8_INLINE static PreParserIdentifier AsIdentifier(
      const PreParserExpression& expression) {
    return expression.AsIdentifier();
  }

  V8_INLINE static PreParserExpression AsIdentifierExpression(
      const PreParserExpression& expression) {
    return expression;
  }

  V8_INLINE bool IsConstructor(const PreParserIdentifier& identifier) const {
    return identifier.IsConstructor();
  }

  V8_INLINE bool IsName(const PreParserIdentifier& identifier) const {
    return identifier.IsName();
  }

  V8_INLINE static bool IsBoilerplateProperty(
      const PreParserExpression& property) {
    // PreParser doesn't count boilerplate properties.
    return false;
  }

  V8_INLINE bool IsNative(const PreParserExpression& expr) const {
    // Preparsing is disabled for extensions (because the extension
    // details aren't passed to lazily compiled functions), so we
    // don't accept "native function" in the preparser and there is
    // no need to keep track of "native".
    return false;
  }

  V8_INLINE static bool IsArrayIndex(const PreParserIdentifier& string,
                                     uint32_t* index) {
    return false;
  }

  V8_INLINE bool IsUseStrictDirective(PreParserStatement statement) const {
    return statement.IsUseStrictLiteral();
  }

  V8_INLINE bool IsUseAsmDirective(PreParserStatement statement) const {
    return statement.IsUseAsmLiteral();
  }

  V8_INLINE bool IsStringLiteral(PreParserStatement statement) const {
    return statement.IsStringLiteral();
  }

  V8_INLINE static void GetDefaultStrings(
      PreParserIdentifier* default_string,
      PreParserIdentifier* star_default_star_string) {}

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  V8_INLINE static void PushLiteralName(const PreParserIdentifier& id) {}
  V8_INLINE static void PushVariableName(const PreParserIdentifier& id) {}
  V8_INLINE void PushPropertyName(const PreParserExpression& expression) {}
  V8_INLINE void PushEnclosingName(const PreParserIdentifier& name) {}
  V8_INLINE static void AddFunctionForNameInference(
      const PreParserExpression& expression) {}
  V8_INLINE static void InferFunctionName() {}

  V8_INLINE static void CheckAssigningFunctionLiteralToProperty(
      const PreParserExpression& left, const PreParserExpression& right) {}

  V8_INLINE void MarkExpressionAsAssigned(
      const PreParserExpression& expression) {
    // TODO(marja): To be able to produce the same errors, the preparser needs
    // to start tracking which expressions are variables and which are assigned.
    if (expression.variables_ != nullptr) {
      DCHECK(FLAG_lazy_inner_functions);
      DCHECK(track_unresolved_variables_);
      for (auto variable : *expression.variables_) {
        variable->set_is_assigned();
      }
    }
  }

  V8_INLINE bool ShortcutNumericLiteralBinaryExpression(
      PreParserExpression* x, const PreParserExpression& y, Token::Value op,
      int pos) {
    return false;
  }

  V8_INLINE NaryOperation* CollapseNaryExpression(PreParserExpression* x,
                                                  PreParserExpression y,
                                                  Token::Value op, int pos,
                                                  const SourceRange& range) {
    return nullptr;
  }

  V8_INLINE PreParserExpression BuildUnaryExpression(
      const PreParserExpression& expression, Token::Value op, int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserStatement
  BuildInitializationBlock(DeclarationParsingResult* parsing_result,
                           ZoneList<const AstRawString*>* names, bool* ok) {
    for (auto declaration : parsing_result->declarations) {
      DeclareAndInitializeVariables(PreParserStatement::Default(),
                                    &(parsing_result->descriptor), &declaration,
                                    names, ok);
    }
    return PreParserStatement::Default();
  }

  V8_INLINE PreParserStatement InitializeForEachStatement(
      PreParserStatement stmt, const PreParserExpression& each,
      const PreParserExpression& subject, PreParserStatement body) {
    MarkExpressionAsAssigned(each);
    return stmt;
  }

  V8_INLINE PreParserStatement InitializeForOfStatement(
      PreParserStatement stmt, const PreParserExpression& each,
      const PreParserExpression& iterable, PreParserStatement body,
      bool finalize, IteratorType type,
      int next_result_pos = kNoSourcePosition) {
    MarkExpressionAsAssigned(each);
    return stmt;
  }

  V8_INLINE PreParserStatement RewriteForVarInLegacy(const ForInfo& for_info) {
    return PreParserStatement::Null();
  }

  V8_INLINE void DesugarBindingInForEachStatement(
      ForInfo* for_info, PreParserStatement* body_block,
      PreParserExpression* each_variable, bool* ok) {
    if (track_unresolved_variables_) {
      DCHECK_EQ(1, for_info->parsing_result.declarations.size());
      bool is_for_var_of =
          for_info->mode == ForEachStatement::ITERATE &&
          for_info->parsing_result.descriptor.mode == VariableMode::VAR;
      bool collect_names =
          IsLexicalVariableMode(for_info->parsing_result.descriptor.mode) ||
          is_for_var_of;

      DeclareAndInitializeVariables(
          PreParserStatement::Default(), &for_info->parsing_result.descriptor,
          &for_info->parsing_result.declarations[0],
          collect_names ? &for_info->bound_names : nullptr, ok);
    }
  }

  V8_INLINE PreParserStatement CreateForEachStatementTDZ(
      PreParserStatement init_block, const ForInfo& for_info, bool* ok) {
    if (track_unresolved_variables_) {
      if (IsLexicalVariableMode(for_info.parsing_result.descriptor.mode)) {
        for (auto name : for_info.bound_names) {
          scope()->DeclareVariableName(name, LET);
        }
        return PreParserStatement::Default();
      }
    }
    return init_block;
  }

  V8_INLINE StatementT DesugarLexicalBindingsInForStatement(
      PreParserStatement loop, PreParserStatement init,
      const PreParserExpression& cond, PreParserStatement next,
      PreParserStatement body, Scope* inner_scope, const ForInfo& for_info,
      bool* ok) {
    // See Parser::DesugarLexicalBindingsInForStatement.
    if (track_unresolved_variables_) {
      for (auto name : for_info.bound_names) {
        inner_scope->DeclareVariableName(
            name, for_info.parsing_result.descriptor.mode);
      }
    }
    return loop;
  }

  PreParserStatement BuildParameterInitializationBlock(
      const PreParserFormalParameters& parameters, bool* ok);

  V8_INLINE PreParserStatement
  BuildRejectPromiseOnException(PreParserStatement init_block) {
    return PreParserStatement::Default();
  }

  V8_INLINE void InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope) {
    scope->HoistSloppyBlockFunctions(nullptr);
  }

  V8_INLINE void InsertShadowingVarBindingInitializers(
      PreParserStatement block) {}

  V8_INLINE PreParserExpression
  NewThrowReferenceError(MessageTemplate::Template message, int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression
  NewThrowSyntaxError(MessageTemplate::Template message,
                      const PreParserIdentifier& arg, int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression
  NewThrowTypeError(MessageTemplate::Template message,
                    const PreParserIdentifier& arg, int pos) {
    return PreParserExpression::Default();
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const char* arg = nullptr,
                       ParseErrorType error_type = kSyntaxError) {
    pending_error_handler()->ReportMessageAt(source_location.beg_pos,
                                             source_location.end_pos, message,
                                             arg, error_type);
  }

  V8_INLINE void ReportMessageAt(Scanner::Location source_location,
                                 MessageTemplate::Template message,
                                 const PreParserIdentifier& arg,
                                 ParseErrorType error_type = kSyntaxError) {
    UNREACHABLE();
  }

  // "null" return type creators.
  V8_INLINE static PreParserIdentifier NullIdentifier() {
    return PreParserIdentifier::Null();
  }
  V8_INLINE static PreParserExpression NullExpression() {
    return PreParserExpression::Null();
  }
  V8_INLINE static PreParserExpression NullLiteralProperty() {
    return PreParserExpression::Null();
  }
  V8_INLINE static PreParserExpressionList NullExpressionList() {
    return PreParserExpressionList::Null();
  }

  V8_INLINE static PreParserStatementList NullStatementList() {
    return PreParserStatementList::Null();
  }
  V8_INLINE static PreParserStatement NullStatement() {
    return PreParserStatement::Null();
  }

  template <typename T>
  V8_INLINE static bool IsNull(T subject) {
    return subject.IsNull();
  }

  V8_INLINE PreParserIdentifier EmptyIdentifierString() const {
    return PreParserIdentifier::Default();
  }

  // Producing data during the recursive descent.
  PreParserIdentifier GetSymbol() const;

  V8_INLINE PreParserIdentifier GetNextSymbol() const {
    return PreParserIdentifier::Default();
  }

  V8_INLINE PreParserIdentifier GetNumberAsSymbol() const {
    return PreParserIdentifier::Default();
  }

  V8_INLINE PreParserExpression ThisExpression(int pos = kNoSourcePosition) {
    ZoneList<VariableProxy*>* variables = nullptr;
    if (track_unresolved_variables_) {
      VariableProxy* proxy = scope()->NewUnresolved(
          factory()->ast_node_factory(), ast_value_factory()->this_string(),
          pos, THIS_VARIABLE);

      variables = new (zone()) ZoneList<VariableProxy*>(1, zone());
      variables->Add(proxy, zone());
    }
    return PreParserExpression::This(variables);
  }

  V8_INLINE PreParserExpression NewSuperPropertyReference(int pos) {
    if (track_unresolved_variables_) {
      scope()->NewUnresolved(factory()->ast_node_factory(),
                             ast_value_factory()->this_function_string(), pos,
                             NORMAL_VARIABLE);
      scope()->NewUnresolved(factory()->ast_node_factory(),
                             ast_value_factory()->this_string(), pos,
                             THIS_VARIABLE);
    }
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression NewSuperCallReference(int pos) {
    if (track_unresolved_variables_) {
      scope()->NewUnresolved(factory()->ast_node_factory(),
                             ast_value_factory()->this_function_string(), pos,
                             NORMAL_VARIABLE);
      scope()->NewUnresolved(factory()->ast_node_factory(),
                             ast_value_factory()->new_target_string(), pos,
                             NORMAL_VARIABLE);
      scope()->NewUnresolved(factory()->ast_node_factory(),
                             ast_value_factory()->this_string(), pos,
                             THIS_VARIABLE);
    }
    return PreParserExpression::SuperCallReference();
  }

  V8_INLINE PreParserExpression NewTargetExpression(int pos) {
    return PreParserExpression::NewTargetExpression();
  }

  V8_INLINE PreParserExpression FunctionSentExpression(int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression ImportMetaExpression(int pos) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserExpression ExpressionFromLiteral(Token::Value token,
                                                      int pos) {
    return PreParserExpression::Default();
  }

  PreParserExpression ExpressionFromIdentifier(
      const PreParserIdentifier& name, int start_position,
      InferName infer = InferName::kYes);

  V8_INLINE PreParserExpression ExpressionFromString(int pos) {
    if (scanner()->IsUseStrict()) {
      return PreParserExpression::UseStrictStringLiteral();
    }
    return PreParserExpression::StringLiteral();
  }

  V8_INLINE PreParserExpressionList NewExpressionList(int size) const {
    return PreParserExpressionList();
  }

  V8_INLINE PreParserExpressionList NewObjectPropertyList(int size) const {
    return PreParserExpressionList();
  }

  V8_INLINE PreParserExpressionList NewClassPropertyList(int size) const {
    return PreParserExpressionList();
  }

  V8_INLINE PreParserStatementList NewStatementList(int size) const {
    return PreParserStatementList();
  }

  V8_INLINE PreParserExpression
  NewV8Intrinsic(const PreParserIdentifier& name,
                 const PreParserExpressionList& arguments, int pos, bool* ok) {
    return PreParserExpression::Default();
  }

  V8_INLINE PreParserStatement
  NewThrowStatement(const PreParserExpression& exception, int pos) {
    return PreParserStatement::Jump();
  }

  V8_INLINE void AddParameterInitializationBlock(
      const PreParserFormalParameters& parameters, PreParserStatementList body,
      bool is_async, bool* ok) {
    if (!parameters.is_simple) {
      BuildParameterInitializationBlock(parameters, ok);
    }
  }

  V8_INLINE void AddFormalParameter(PreParserFormalParameters* parameters,
                                    const PreParserExpression& pattern,
                                    const PreParserExpression& initializer,
                                    int initializer_end_position,
                                    bool is_rest) {
    if (track_unresolved_variables_) {
      DCHECK(FLAG_lazy_inner_functions);
      parameters->params.Add(new (zone()) PreParserFormalParameters::Parameter(
          pattern.variables_, is_rest));
    }
    parameters->UpdateArityAndFunctionLength(!initializer.IsNull(), is_rest);
  }

  V8_INLINE void DeclareFormalParameters(
      DeclarationScope* scope,
      const ThreadedList<PreParserFormalParameters::Parameter>& parameters,
      bool is_simple) {
    if (!is_simple) scope->SetHasNonSimpleParameters();
    if (track_unresolved_variables_) {
      DCHECK(FLAG_lazy_inner_functions);
      for (auto parameter : parameters) {
        DCHECK_IMPLIES(is_simple, parameter->variables_ != nullptr);
        DCHECK_IMPLIES(is_simple, parameter->variables_->length() == 1);
        // Make sure each parameter is added only once even if it's a
        // destructuring parameter which contains multiple names.
        bool add_parameter = true;
        if (parameter->variables_ != nullptr) {
          for (auto variable : (*parameter->variables_)) {
            // We declare the parameter name for all names, but only create a
            // parameter entry for the first one.
            scope->DeclareParameterName(variable->raw_name(),
                                        parameter->is_rest, ast_value_factory(),
                                        true, add_parameter);
            add_parameter = false;
          }
        }
        if (add_parameter) {
          // No names were declared; declare a dummy one here to up the
          // parameter count.
          DCHECK(!is_simple);
          scope->DeclareParameterName(ast_value_factory()->empty_string(),
                                      parameter->is_rest, ast_value_factory(),
                                      false, add_parameter);
        }
      }
    }
  }

  V8_INLINE void DeclareArrowFunctionFormalParameters(
      PreParserFormalParameters* parameters, const PreParserExpression& params,
      const Scanner::Location& params_loc, Scanner::Location* duplicate_loc,
      bool* ok) {
    // TODO(wingo): Detect duplicated identifiers in paramlists.  Detect
    // parameter lists that are too long.
    if (track_unresolved_variables_) {
      DCHECK(FLAG_lazy_inner_functions);
      if (params.variables_ != nullptr) {
        for (auto variable : *params.variables_) {
          parameters->scope->DeclareVariableName(variable->raw_name(), VAR);
        }
      }
    }
  }

  V8_INLINE PreParserExpression
  ExpressionListToExpression(const PreParserExpressionList& args) {
    return PreParserExpression::Default(args.variables_);
  }

  V8_INLINE void SetFunctionNameFromPropertyName(
      const PreParserExpression& property, const PreParserIdentifier& name,
      const AstRawString* prefix = nullptr) {}
  V8_INLINE void SetFunctionNameFromIdentifierRef(
      const PreParserExpression& value, const PreParserExpression& identifier) {
  }

  V8_INLINE ZoneList<typename ExpressionClassifier::Error>*
  GetReportedErrorList() const {
    return function_state_->GetReportedErrorList();
  }

  V8_INLINE ZoneList<PreParserExpression>* GetNonPatternList() const {
    return function_state_->non_patterns_to_rewrite();
  }

  V8_INLINE void CountUsage(v8::Isolate::UseCounterFeature feature) {
    if (use_counts_ != nullptr) ++use_counts_[feature];
  }

  V8_INLINE bool ParsingDynamicFunctionDeclaration() const { return false; }

// Generate empty functions here as the preparser does not collect source
// ranges for block coverage.
#define DEFINE_RECORD_SOURCE_RANGE(Name) \
  template <typename... Ts>              \
  V8_INLINE void Record##Name##SourceRange(Ts... args) {}
  AST_SOURCE_RANGE_LIST(DEFINE_RECORD_SOURCE_RANGE)
#undef DEFINE_RECORD_SOURCE_RANGE

  // Preparser's private field members.

  int* use_counts_;
  bool track_unresolved_variables_;
  PreParserLogger log_;

  ProducedPreParsedScopeData* produced_preparsed_scope_data_;
};

PreParserExpression PreParser::SpreadCall(const PreParserExpression& function,
                                          const PreParserExpressionList& args,
                                          int pos,
                                          Call::PossiblyEval possibly_eval) {
  return factory()->NewCall(function, args, pos, possibly_eval);
}

PreParserExpression PreParser::SpreadCallNew(
    const PreParserExpression& function, const PreParserExpressionList& args,
    int pos) {
  return factory()->NewCallNew(function, args, pos);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSER_H
