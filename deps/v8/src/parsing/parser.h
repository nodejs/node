// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_H_
#define V8_PARSING_PARSER_H_

#include <cstddef>

#include "src/ast/ast-source-ranges.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/compiler-specific.h"
#include "src/base/threaded-list.h"
#include "src/common/globals.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/parsing.h"
#include "src/parsing/preparser.h"
#include "src/utils/pointer-with-payload.h"
#include "src/zone/zone-chunk-list.h"

namespace v8 {

class ScriptCompiler;

namespace internal {

class ConsumedPreparseData;
class ParseInfo;
class ParserTarget;
class ParserTargetScope;
class PendingCompilationErrorHandler;
class PreparseData;

// ----------------------------------------------------------------------------
// JAVASCRIPT PARSING

class Parser;


struct ParserFormalParameters : FormalParametersBase {
  struct Parameter : public ZoneObject {
    Parameter(Expression* pattern, Expression* initializer, int position,
              int initializer_end_position, bool is_rest)
        : initializer_and_is_rest(initializer, is_rest),
          pattern(pattern),
          position(position),
          initializer_end_position(initializer_end_position) {}

    PointerWithPayload<Expression, bool, 1> initializer_and_is_rest;

    Expression* pattern;
    Expression* initializer() const {
      return initializer_and_is_rest.GetPointer();
    }
    int position;
    int initializer_end_position;
    inline bool is_rest() const { return initializer_and_is_rest.GetPayload(); }

    Parameter* next_parameter = nullptr;
    bool is_simple() const {
      return pattern->IsVariableProxy() && initializer() == nullptr &&
             !is_rest();
    }

    const AstRawString* name() const {
      DCHECK(is_simple());
      return pattern->AsVariableProxy()->raw_name();
    }

    Parameter** next() { return &next_parameter; }
    Parameter* const* next() const { return &next_parameter; }
  };

  void set_strict_parameter_error(const Scanner::Location& loc,
                                  MessageTemplate message) {
    strict_error_loc = loc;
    strict_error_message = message;
  }

  bool has_duplicate() const { return duplicate_loc.IsValid(); }
  void ValidateDuplicate(Parser* parser) const;
  void ValidateStrictMode(Parser* parser) const;

  explicit ParserFormalParameters(DeclarationScope* scope)
      : FormalParametersBase(scope) {}

  base::ThreadedList<Parameter> params;
  Scanner::Location duplicate_loc = Scanner::Location::invalid();
  Scanner::Location strict_error_loc = Scanner::Location::invalid();
  MessageTemplate strict_error_message = MessageTemplate::kNone;
};

template <>
struct ParserTypes<Parser> {
  using Base = ParserBase<Parser>;
  using Impl = Parser;

  // Return types for traversing functions.
  using Block = v8::internal::Block*;
  using BreakableStatement = v8::internal::BreakableStatement*;
  using ClassLiteralProperty = ClassLiteral::Property*;
  using ClassPropertyList = ZonePtrList<ClassLiteral::Property>*;
  using Expression = v8::internal::Expression*;
  using ExpressionList = ScopedPtrList<v8::internal::Expression>;
  using FormalParameters = ParserFormalParameters;
  using ForStatement = v8::internal::ForStatement*;
  using FunctionLiteral = v8::internal::FunctionLiteral*;
  using Identifier = const AstRawString*;
  using IterationStatement = v8::internal::IterationStatement*;
  using ObjectLiteralProperty = ObjectLiteral::Property*;
  using ObjectPropertyList = ScopedPtrList<v8::internal::ObjectLiteralProperty>;
  using Statement = v8::internal::Statement*;
  using StatementList = ScopedPtrList<v8::internal::Statement>;
  using Suspend = v8::internal::Suspend*;

  // For constructing objects returned by the traversing functions.
  using Factory = AstNodeFactory;

  // Other implementation-specific functions.
  using FuncNameInferrer = v8::internal::FuncNameInferrer;
  using SourceRange = v8::internal::SourceRange;
  using SourceRangeScope = v8::internal::SourceRangeScope;
  using Target = ParserTarget;
  using TargetScope = ParserTargetScope;
};

class V8_EXPORT_PRIVATE Parser : public NON_EXPORTED_BASE(ParserBase<Parser>) {
 public:
  explicit Parser(ParseInfo* info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = nullptr;
  }

  static bool IsPreParser() { return false; }

  void ParseOnBackground(ParseInfo* info);

  // Initializes an empty scope chain for top-level scripts, or scopes which
  // consist of only the native context.
  void InitializeEmptyScopeChain(ParseInfo* info);

  // Deserialize the scope chain prior to parsing in which the script is going
  // to be executed. If the script is a top-level script, or the scope chain
  // consists of only a native context, maybe_outer_scope_info should be an
  // empty handle.
  //
  // This only deserializes the scope chain, but doesn't connect the scopes to
  // their corresponding scope infos. Therefore, looking up variables in the
  // deserialized scopes is not possible.
  void DeserializeScopeChain(Isolate* isolate, ParseInfo* info,
                             MaybeHandle<ScopeInfo> maybe_outer_scope_info,
                             Scope::DeserializationMode mode =
                                 Scope::DeserializationMode::kScopesOnly);

  // Move statistics to Isolate
  void UpdateStatistics(Isolate* isolate, Handle<Script> script);
  void HandleSourceURLComments(Isolate* isolate, Handle<Script> script);

 private:
  friend class ParserBase<Parser>;
  friend struct ParserFormalParameters;
  friend class i::ExpressionScope<ParserTypes<Parser>>;
  friend class i::VariableDeclarationParsingScope<ParserTypes<Parser>>;
  friend class i::ParameterDeclarationParsingScope<ParserTypes<Parser>>;
  friend class i::ArrowHeadParsingScope<ParserTypes<Parser>>;
  friend bool v8::internal::parsing::ParseProgram(ParseInfo*, Isolate*);
  friend bool v8::internal::parsing::ParseFunction(
      ParseInfo*, Handle<SharedFunctionInfo> shared_info, Isolate*);

  bool AllowsLazyParsingWithoutUnresolvedVariables() const {
    return scope()->AllowsLazyParsingWithoutUnresolvedVariables(
        original_scope_);
  }

  bool parse_lazily() const { return mode_ == PARSE_LAZILY; }
  enum Mode { PARSE_LAZILY, PARSE_EAGERLY };

  class ParsingModeScope {
   public:
    ParsingModeScope(Parser* parser, Mode mode)
        : parser_(parser), old_mode_(parser->mode_) {
      parser_->mode_ = mode;
    }
    ~ParsingModeScope() { parser_->mode_ = old_mode_; }

   private:
    Parser* parser_;
    Mode old_mode_;
  };

  // Runtime encoding of different completion modes.
  enum CompletionKind {
    kNormalCompletion,
    kThrowCompletion,
    kAbruptCompletion
  };

  Variable* NewTemporary(const AstRawString* name) {
    return scope()->NewTemporary(name);
  }

  void PrepareGeneratorVariables();

  // Returns nullptr if parsing failed.
  FunctionLiteral* ParseProgram(Isolate* isolate, ParseInfo* info);

  FunctionLiteral* ParseFunction(Isolate* isolate, ParseInfo* info,
                                 Handle<SharedFunctionInfo> shared_info);
  FunctionLiteral* DoParseFunction(Isolate* isolate, ParseInfo* info,
                                   const AstRawString* raw_name);

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(Isolate* isolate, ParseInfo* info);

  // Parse with the script as if the source is implicitly wrapped in a function.
  // We manually construct the AST and scopes for a top-level function and the
  // function wrapper.
  void ParseWrapped(Isolate* isolate, ParseInfo* info,
                    ScopedPtrList<Statement>* body, DeclarationScope* scope,
                    Zone* zone);

  ZonePtrList<const AstRawString>* PrepareWrappedArguments(Isolate* isolate,
                                                           ParseInfo* info,
                                                           Zone* zone);

  PreParser* reusable_preparser() {
    if (reusable_preparser_ == nullptr) {
      reusable_preparser_ = new PreParser(
          &preparser_zone_, &scanner_, stack_limit_, ast_value_factory(),
          pending_error_handler(), runtime_call_stats_, logger_, -1,
          parsing_module_, parsing_on_main_thread_);
#define SET_ALLOW(name) reusable_preparser_->set_allow_##name(allow_##name());
      SET_ALLOW(natives);
      SET_ALLOW(harmony_dynamic_import);
      SET_ALLOW(harmony_import_meta);
      SET_ALLOW(harmony_private_methods);
      SET_ALLOW(eval_cache);
#undef SET_ALLOW
      preparse_data_buffer_.reserve(128);
    }
    return reusable_preparser_;
  }

  void ParseModuleItemList(ScopedPtrList<Statement>* body);
  Statement* ParseModuleItem();
  const AstRawString* ParseModuleSpecifier();
  void ParseImportDeclaration();
  Statement* ParseExportDeclaration();
  Statement* ParseExportDefault();
  void ParseExportStar();
  struct ExportClauseData {
    const AstRawString* export_name;
    const AstRawString* local_name;
    Scanner::Location location;
  };
  ZoneChunkList<ExportClauseData>* ParseExportClause(
      Scanner::Location* reserved_loc);
  struct NamedImport : public ZoneObject {
    const AstRawString* import_name;
    const AstRawString* local_name;
    const Scanner::Location location;
    NamedImport(const AstRawString* import_name, const AstRawString* local_name,
                Scanner::Location location)
        : import_name(import_name),
          local_name(local_name),
          location(location) {}
  };
  ZonePtrList<const NamedImport>* ParseNamedImports(int pos);
  Statement* BuildInitializationBlock(DeclarationParsingResult* parsing_result);
  void DeclareLabel(ZonePtrList<const AstRawString>** labels,
                    ZonePtrList<const AstRawString>** own_labels,
                    VariableProxy* expr);
  bool ContainsLabel(ZonePtrList<const AstRawString>* labels,
                     const AstRawString* label);
  Expression* RewriteReturn(Expression* return_value, int pos);
  Statement* RewriteSwitchStatement(SwitchStatement* switch_statement,
                                    Scope* scope);
  Block* RewriteCatchPattern(CatchInfo* catch_info);
  void ReportVarRedeclarationIn(const AstRawString* name, Scope* scope);
  Statement* RewriteTryStatement(Block* try_block, Block* catch_block,
                                 const SourceRange& catch_range,
                                 Block* finally_block,
                                 const SourceRange& finally_range,
                                 const CatchInfo& catch_info, int pos);
  void ParseAndRewriteGeneratorFunctionBody(int pos, FunctionKind kind,
                                            ScopedPtrList<Statement>* body);
  void ParseAndRewriteAsyncGeneratorFunctionBody(
      int pos, FunctionKind kind, ScopedPtrList<Statement>* body);
  void DeclareFunctionNameVar(const AstRawString* function_name,
                              FunctionLiteral::FunctionType function_type,
                              DeclarationScope* function_scope);

  Statement* DeclareFunction(const AstRawString* variable_name,
                             FunctionLiteral* function, VariableMode mode,
                             VariableKind kind, int beg_pos, int end_pos,
                             ZonePtrList<const AstRawString>* names);
  Variable* CreateSyntheticContextVariable(const AstRawString* synthetic_name);
  Variable* CreatePrivateNameVariable(
      ClassScope* scope, RequiresBrandCheckFlag requires_brand_check,
      const AstRawString* name);
  FunctionLiteral* CreateInitializerFunction(
      const char* name, DeclarationScope* scope,
      ZonePtrList<ClassLiteral::Property>* fields);

  bool IdentifierEquals(const AstRawString* identifier,
                        const AstRawString* other) {
    return identifier == other;
  }

  Statement* DeclareClass(const AstRawString* variable_name, Expression* value,
                          ZonePtrList<const AstRawString>* names,
                          int class_token_pos, int end_pos);
  void DeclareClassVariable(const AstRawString* name, ClassInfo* class_info,
                            int class_token_pos);
  void DeclareClassBrandVariable(ClassScope* scope, ClassInfo* class_info,
                                 int class_token_pos);
  void DeclarePrivateClassMember(ClassScope* scope,
                                 const AstRawString* property_name,
                                 ClassLiteralProperty* property,
                                 ClassLiteralProperty::Kind kind,
                                 bool is_static, ClassInfo* class_info);
  void DeclarePublicClassMethod(const AstRawString* class_name,
                                ClassLiteralProperty* property,
                                bool is_constructor, ClassInfo* class_info);
  void DeclarePublicClassField(ClassScope* scope,
                               ClassLiteralProperty* property, bool is_static,
                               bool is_computed_name, ClassInfo* class_info);
  void DeclareClassProperty(ClassScope* scope, const AstRawString* class_name,
                            ClassLiteralProperty* property, bool is_constructor,
                            ClassInfo* class_info);
  void DeclareClassField(ClassScope* scope, ClassLiteralProperty* property,
                         const AstRawString* property_name, bool is_static,
                         bool is_computed_name, bool is_private,
                         ClassInfo* class_info);
  Expression* RewriteClassLiteral(ClassScope* block_scope,
                                  const AstRawString* name,
                                  ClassInfo* class_info, int pos, int end_pos);
  Statement* DeclareNative(const AstRawString* name, int pos);

  Block* IgnoreCompletion(Statement* statement);

  Scope* NewHiddenCatchScope();

  bool HasCheckedSyntax() {
    return scope()->GetDeclarationScope()->has_checked_syntax();
  }

  // PatternRewriter and associated methods defined in pattern-rewriter.cc.
  friend class PatternRewriter;
  void InitializeVariables(
      ScopedPtrList<Statement>* statements, VariableKind kind,
      const DeclarationParsingResult::Declaration* declaration);

  Block* RewriteForVarInLegacy(const ForInfo& for_info);
  void DesugarBindingInForEachStatement(ForInfo* for_info, Block** body_block,
                                        Expression** each_variable);
  Block* CreateForEachStatementTDZ(Block* init_block, const ForInfo& for_info);

  Statement* DesugarLexicalBindingsInForStatement(
      ForStatement* loop, Statement* init, Expression* cond, Statement* next,
      Statement* body, Scope* inner_scope, const ForInfo& for_info);

  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      LanguageMode language_mode,
      ZonePtrList<const AstRawString>* arguments_for_wrapped_function);

  ObjectLiteral* InitializeObjectLiteral(ObjectLiteral* object_literal) {
    object_literal->CalculateEmitStore(main_zone());
    return object_literal;
  }

  // Insert initializer statements for var-bindings shadowing parameter bindings
  // from a non-simple parameter list.
  void InsertShadowingVarBindingInitializers(Block* block);

  // Implement sloppy block-scoped functions, ES2015 Annex B 3.3
  void InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope);

  void DeclareUnboundVariable(const AstRawString* name, VariableMode mode,
                              InitializationFlag init, int pos);
  V8_WARN_UNUSED_RESULT
  VariableProxy* DeclareBoundVariable(const AstRawString* name,
                                      VariableMode mode, int pos);
  void DeclareAndBindVariable(VariableProxy* proxy, VariableKind kind,
                              VariableMode mode, InitializationFlag init,
                              Scope* declaration_scope, bool* was_added,
                              int begin, int end = kNoSourcePosition);
  V8_WARN_UNUSED_RESULT
  Variable* DeclareVariable(const AstRawString* name, VariableKind kind,
                            VariableMode mode, InitializationFlag init,
                            Scope* declaration_scope, bool* was_added,
                            int begin, int end = kNoSourcePosition);
  void Declare(Declaration* declaration, const AstRawString* name,
               VariableKind kind, VariableMode mode, InitializationFlag init,
               Scope* declaration_scope, bool* was_added, int var_begin_pos,
               int var_end_pos = kNoSourcePosition);

  bool TargetStackContainsLabel(const AstRawString* label);
  BreakableStatement* LookupBreakTarget(const AstRawString* label);
  IterationStatement* LookupContinueTarget(const AstRawString* label);

  // Factory methods.
  FunctionLiteral* DefaultConstructor(const AstRawString* name, bool call_super,
                                      int pos, int end_pos);

  // Skip over a lazy function, either using cached data if we have it, or
  // by parsing the function with PreParser. Consumes the ending }.
  // In case the preparser detects an error it cannot identify, it resets the
  // scanner- and preparser state to the initial one, before PreParsing the
  // function.
  // SkipFunction returns true if it correctly parsed the function, including
  // cases where we detect an error. It returns false, if we needed to stop
  // parsing or could not identify an error correctly, meaning the caller needs
  // to fully reparse. In this case it resets the scanner and preparser state.
  bool SkipFunction(const AstRawString* function_name, FunctionKind kind,
                    FunctionLiteral::FunctionType function_type,
                    DeclarationScope* function_scope, int* num_parameters,
                    int* function_length,
                    ProducedPreparseData** produced_preparsed_scope_data);

  Block* BuildParameterInitializationBlock(
      const ParserFormalParameters& parameters);
  Block* BuildRejectPromiseOnException(Block* block);

  void ParseFunction(
      ScopedPtrList<Statement>* body, const AstRawString* function_name,
      int pos, FunctionKind kind, FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope, int* num_parameters,
      int* function_length, bool* has_duplicate_parameters,
      int* expected_property_count, int* suspend_count,
      ZonePtrList<const AstRawString>* arguments_for_wrapped_function);

  void ThrowPendingError(Isolate* isolate, Handle<Script> script);

  class TemplateLiteral : public ZoneObject {
   public:
    TemplateLiteral(Zone* zone, int pos)
        : cooked_(8, zone), raw_(8, zone), expressions_(8, zone), pos_(pos) {}

    const ZonePtrList<const AstRawString>* cooked() const { return &cooked_; }
    const ZonePtrList<const AstRawString>* raw() const { return &raw_; }
    const ZonePtrList<Expression>* expressions() const { return &expressions_; }
    int position() const { return pos_; }

    void AddTemplateSpan(const AstRawString* cooked, const AstRawString* raw,
                         int end, Zone* zone) {
      DCHECK_NOT_NULL(raw);
      cooked_.Add(cooked, zone);
      raw_.Add(raw, zone);
    }

    void AddExpression(Expression* expression, Zone* zone) {
      expressions_.Add(expression, zone);
    }

   private:
    ZonePtrList<const AstRawString> cooked_;
    ZonePtrList<const AstRawString> raw_;
    ZonePtrList<Expression> expressions_;
    int pos_;
  };

  using TemplateLiteralState = TemplateLiteral*;

  TemplateLiteralState OpenTemplateLiteral(int pos);
  // "should_cook" means that the span can be "cooked": in tagged template
  // literals, both the raw and "cooked" representations are available to user
  // code ("cooked" meaning that escape sequences are converted to their
  // interpreted values). Invalid escape sequences cause the cooked span
  // to be represented by undefined, instead of being a syntax error.
  // "tail" indicates that this span is the last in the literal.
  void AddTemplateSpan(TemplateLiteralState* state, bool should_cook,
                       bool tail);
  void AddTemplateExpression(TemplateLiteralState* state,
                             Expression* expression);
  Expression* CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                   Expression* tag);

  ArrayLiteral* ArrayLiteralFromListWithSpread(
      const ScopedPtrList<Expression>& list);
  Expression* SpreadCall(Expression* function,
                         const ScopedPtrList<Expression>& args, int pos,
                         Call::PossiblyEval is_possibly_eval);
  Expression* SpreadCallNew(Expression* function,
                            const ScopedPtrList<Expression>& args, int pos);
  Expression* RewriteSuperCall(Expression* call_expression);

  void SetLanguageMode(Scope* scope, LanguageMode mode);
  void SetAsmModule();

  Expression* RewriteSpreads(ArrayLiteral* lit);

  Expression* BuildInitialYield(int pos, FunctionKind kind);
  Assignment* BuildCreateJSGeneratorObject(int pos, FunctionKind kind);

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(Runtime::FunctionId function_id,
                            MessageTemplate message, const AstRawString* arg,
                            int pos);

  Statement* CheckCallable(Variable* var, Expression* error, int pos);

  void RewriteAsyncFunctionBody(ScopedPtrList<Statement>* body, Block* block,
                                Expression* return_value);

  void AddArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                        Expression* params, int end_pos);
  void SetFunctionName(Expression* value, const AstRawString* name,
                       const AstRawString* prefix = nullptr);

  // Helper functions for recursive descent.
  V8_INLINE bool IsEval(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->eval_string();
  }

  V8_INLINE bool IsAsync(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->async_string();
  }

  V8_INLINE bool IsArguments(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->arguments_string();
  }

  V8_INLINE bool IsEvalOrArguments(const AstRawString* identifier) const {
    return IsEval(identifier) || IsArguments(identifier);
  }

  // Returns true if the expression is of type "this.foo".
  V8_INLINE static bool IsThisProperty(Expression* expression) {
    DCHECK_NOT_NULL(expression);
    Property* property = expression->AsProperty();
    return property != nullptr && property->obj()->IsThisExpression();
  }

  // Returns true if the expression is of type "obj.#foo".
  V8_INLINE static bool IsPrivateReference(Expression* expression) {
    DCHECK_NOT_NULL(expression);
    Property* property = expression->AsProperty();
    return property != nullptr && property->IsPrivateReference();
  }

  // This returns true if the expression is an indentifier (wrapped
  // inside a variable proxy).  We exclude the case of 'this', which
  // has been converted to a variable proxy.
  V8_INLINE static bool IsIdentifier(Expression* expression) {
    VariableProxy* operand = expression->AsVariableProxy();
    return operand != nullptr && !operand->is_new_target();
  }

  V8_INLINE static const AstRawString* AsIdentifier(Expression* expression) {
    DCHECK(IsIdentifier(expression));
    return expression->AsVariableProxy()->raw_name();
  }

  V8_INLINE VariableProxy* AsIdentifierExpression(Expression* expression) {
    return expression->AsVariableProxy();
  }

  V8_INLINE bool IsConstructor(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->constructor_string();
  }

  V8_INLINE bool IsName(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->name_string();
  }

  V8_INLINE static bool IsBoilerplateProperty(
      ObjectLiteral::Property* property) {
    return !property->IsPrototype();
  }

  V8_INLINE bool IsNative(Expression* expr) const {
    DCHECK_NOT_NULL(expr);
    return expr->IsVariableProxy() &&
           expr->AsVariableProxy()->raw_name() ==
               ast_value_factory()->native_string();
  }

  V8_INLINE static bool IsArrayIndex(const AstRawString* string,
                                     uint32_t* index) {
    return string->AsArrayIndex(index);
  }

  // Returns true if the statement is an expression statement containing
  // a single string literal.  If a second argument is given, the literal
  // is also compared with it and the result is true only if they are equal.
  V8_INLINE bool IsStringLiteral(Statement* statement,
                                 const AstRawString* arg = nullptr) const {
    ExpressionStatement* e_stat = statement->AsExpressionStatement();
    if (e_stat == nullptr) return false;
    Literal* literal = e_stat->expression()->AsLiteral();
    if (literal == nullptr || !literal->IsString()) return false;
    return arg == nullptr || literal->AsRawString() == arg;
  }

  V8_INLINE void GetDefaultStrings(const AstRawString** default_string,
                                   const AstRawString** dot_default_string) {
    *default_string = ast_value_factory()->default_string();
    *dot_default_string = ast_value_factory()->dot_default_string();
  }

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  V8_INLINE void PushLiteralName(const AstRawString* id) {
    fni_.PushLiteralName(id);
  }

  V8_INLINE void PushVariableName(const AstRawString* id) {
    fni_.PushVariableName(id);
  }

  V8_INLINE void PushPropertyName(Expression* expression) {
    if (expression->IsPropertyName()) {
      fni_.PushLiteralName(expression->AsLiteral()->AsRawPropertyName());
    } else {
      fni_.PushLiteralName(ast_value_factory()->computed_string());
    }
  }

  V8_INLINE void PushEnclosingName(const AstRawString* name) {
    fni_.PushEnclosingName(name);
  }

  V8_INLINE void AddFunctionForNameInference(FunctionLiteral* func_to_infer) {
    fni_.AddFunction(func_to_infer);
  }

  V8_INLINE void InferFunctionName() { fni_.Infer(); }

  // If we assign a function literal to a property we pretenure the
  // literal so it can be added as a constant function property.
  V8_INLINE static void CheckAssigningFunctionLiteralToProperty(
      Expression* left, Expression* right) {
    DCHECK_NOT_NULL(left);
    if (left->IsProperty() && right->IsFunctionLiteral()) {
      right->AsFunctionLiteral()->set_pretenure();
    }
  }

  // A shortcut for performing a ToString operation
  V8_INLINE Expression* ToString(Expression* expr) {
    if (expr->IsStringLiteral()) return expr;
    ScopedPtrList<Expression> args(pointer_buffer());
    args.Add(expr);
    return factory()->NewCallRuntime(Runtime::kInlineToStringRT, args,
                                     expr->position());
  }

  // Returns true if we have a binary expression between two numeric
  // literals. In that case, *x will be changed to an expression which is the
  // computed value.
  bool ShortcutNumericLiteralBinaryExpression(Expression** x, Expression* y,
                                              Token::Value op, int pos);

  // Returns true if we have a binary operation between a binary/n-ary
  // expression (with the same operation) and a value, which can be collapsed
  // into a single n-ary expression. In that case, *x will be changed to an
  // n-ary expression.
  bool CollapseNaryExpression(Expression** x, Expression* y, Token::Value op,
                              int pos, const SourceRange& range);

  // Returns a UnaryExpression or, in one of the following cases, a Literal.
  // ! <literal> -> true / false
  // + <Number literal> -> <Number literal>
  // - <Number literal> -> <Number literal with value negated>
  // ~ <literal> -> true / false
  Expression* BuildUnaryExpression(Expression* expression, Token::Value op,
                                   int pos);

  // Generate AST node that throws a ReferenceError with the given type.
  V8_INLINE Expression* NewThrowReferenceError(MessageTemplate message,
                                               int pos) {
    return NewThrowError(Runtime::kNewReferenceError, message,
                         ast_value_factory()->empty_string(), pos);
  }

  // Generate AST node that throws a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  V8_INLINE Expression* NewThrowSyntaxError(MessageTemplate message,
                                            const AstRawString* arg, int pos) {
    return NewThrowError(Runtime::kNewSyntaxError, message, arg, pos);
  }

  // Generate AST node that throws a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  V8_INLINE Expression* NewThrowTypeError(MessageTemplate message,
                                          const AstRawString* arg, int pos) {
    return NewThrowError(Runtime::kNewTypeError, message, arg, pos);
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate message, const char* arg = nullptr) {
    pending_error_handler()->ReportMessageAt(
        source_location.beg_pos, source_location.end_pos, message, arg);
    scanner_.set_parser_error();
  }

  // Dummy implementation. The parser should never have a unidentifiable
  // error.
  V8_INLINE void ReportUnidentifiableError() { UNREACHABLE(); }

  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate message, const AstRawString* arg) {
    pending_error_handler()->ReportMessageAt(
        source_location.beg_pos, source_location.end_pos, message, arg);
    scanner_.set_parser_error();
  }

  const AstRawString* GetRawNameFromIdentifier(const AstRawString* arg) {
    return arg;
  }

  void ReportUnexpectedTokenAt(
      Scanner::Location location, Token::Value token,
      MessageTemplate message = MessageTemplate::kUnexpectedToken);

  // "null" return type creators.
  V8_INLINE static std::nullptr_t NullIdentifier() { return nullptr; }
  V8_INLINE static std::nullptr_t NullExpression() { return nullptr; }
  V8_INLINE static std::nullptr_t NullLiteralProperty() { return nullptr; }
  V8_INLINE static ZonePtrList<Expression>* NullExpressionList() {
    return nullptr;
  }
  V8_INLINE static ZonePtrList<Statement>* NullStatementList() {
    return nullptr;
  }
  V8_INLINE static std::nullptr_t NullStatement() { return nullptr; }
  V8_INLINE static std::nullptr_t NullBlock() { return nullptr; }
  Expression* FailureExpression() { return factory()->FailureExpression(); }

  template <typename T>
  V8_INLINE static bool IsNull(T subject) {
    return subject == nullptr;
  }

  // Non-null empty string.
  V8_INLINE const AstRawString* EmptyIdentifierString() const {
    return ast_value_factory()->empty_string();
  }

  // Producing data during the recursive descent.
  V8_INLINE const AstRawString* GetSymbol() const {
    const AstRawString* result = scanner()->CurrentSymbol(ast_value_factory());
    DCHECK_NOT_NULL(result);
    return result;
  }

  V8_INLINE const AstRawString* GetIdentifier() const { return GetSymbol(); }

  V8_INLINE const AstRawString* GetNextSymbol() const {
    return scanner()->NextSymbol(ast_value_factory());
  }

  V8_INLINE const AstRawString* GetNumberAsSymbol() const {
    double double_value = scanner()->DoubleValue();
    char array[100];
    const char* string = DoubleToCString(double_value, ArrayVector(array));
    return ast_value_factory()->GetOneByteString(string);
  }

  class ThisExpression* ThisExpression() {
    UseThis();
    return factory()->ThisExpression();
  }

  Expression* NewSuperPropertyReference(int pos);
  Expression* NewSuperCallReference(int pos);
  Expression* NewTargetExpression(int pos);
  Expression* ImportMetaExpression(int pos);

  Expression* ExpressionFromLiteral(Token::Value token, int pos);

  V8_INLINE VariableProxy* ExpressionFromPrivateName(ClassScope* class_scope,
                                                     const AstRawString* name,
                                                     int start_position) {
    VariableProxy* proxy = factory()->ast_node_factory()->NewVariableProxy(
        name, NORMAL_VARIABLE, start_position);
    class_scope->AddUnresolvedPrivateName(proxy);
    return proxy;
  }

  V8_INLINE VariableProxy* ExpressionFromIdentifier(
      const AstRawString* name, int start_position,
      InferName infer = InferName::kYes) {
    if (infer == InferName::kYes) {
      fni_.PushVariableName(name);
    }
    return expression_scope()->NewVariable(name, start_position);
  }

  V8_INLINE void DeclareIdentifier(const AstRawString* name,
                                   int start_position) {
    expression_scope()->Declare(name, start_position);
  }

  V8_INLINE Variable* DeclareCatchVariableName(Scope* scope,
                                               const AstRawString* name) {
    return scope->DeclareCatchVariableName(name);
  }

  V8_INLINE ZonePtrList<Expression>* NewExpressionList(int size) const {
    return new (zone()) ZonePtrList<Expression>(size, zone());
  }
  V8_INLINE ZonePtrList<ObjectLiteral::Property>* NewObjectPropertyList(
      int size) const {
    return new (zone()) ZonePtrList<ObjectLiteral::Property>(size, zone());
  }
  V8_INLINE ZonePtrList<ClassLiteral::Property>* NewClassPropertyList(
      int size) const {
    return new (zone()) ZonePtrList<ClassLiteral::Property>(size, zone());
  }
  V8_INLINE ZonePtrList<Statement>* NewStatementList(int size) const {
    return new (zone()) ZonePtrList<Statement>(size, zone());
  }

  Expression* NewV8Intrinsic(const AstRawString* name,
                             const ScopedPtrList<Expression>& args, int pos);

  V8_INLINE Statement* NewThrowStatement(Expression* exception, int pos) {
    return factory()->NewExpressionStatement(
        factory()->NewThrow(exception, pos), pos);
  }

  V8_INLINE void AddFormalParameter(ParserFormalParameters* parameters,
                                    Expression* pattern,
                                    Expression* initializer,
                                    int initializer_end_position,
                                    bool is_rest) {
    parameters->UpdateArityAndFunctionLength(initializer != nullptr, is_rest);
    auto parameter = new (parameters->scope->zone())
        ParserFormalParameters::Parameter(pattern, initializer,
                                          scanner()->location().beg_pos,
                                          initializer_end_position, is_rest);

    parameters->params.Add(parameter);
  }

  V8_INLINE void DeclareFormalParameters(ParserFormalParameters* parameters) {
    bool is_simple = parameters->is_simple;
    DeclarationScope* scope = parameters->scope;
    if (!is_simple) scope->MakeParametersNonSimple();
    for (auto parameter : parameters->params) {
      bool is_optional = parameter->initializer() != nullptr;
      // If the parameter list is simple, declare the parameters normally with
      // their names. If the parameter list is not simple, declare a temporary
      // for each parameter - the corresponding named variable is declared by
      // BuildParamerterInitializationBlock.
      scope->DeclareParameter(
          is_simple ? parameter->name() : ast_value_factory()->empty_string(),
          is_simple ? VariableMode::kVar : VariableMode::kTemporary,
          is_optional, parameter->is_rest(), ast_value_factory(),
          parameter->position);
    }
  }

  void DeclareArrowFunctionFormalParameters(
      ParserFormalParameters* parameters, Expression* params,
      const Scanner::Location& params_loc);

  Expression* ExpressionListToExpression(const ScopedPtrList<Expression>& args);

  void SetFunctionNameFromPropertyName(LiteralProperty* property,
                                       const AstRawString* name,
                                       const AstRawString* prefix = nullptr);
  void SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                       const AstRawString* name,
                                       const AstRawString* prefix = nullptr);

  void SetFunctionNameFromIdentifierRef(Expression* value,
                                        Expression* identifier);

  V8_INLINE void CountUsage(v8::Isolate::UseCounterFeature feature) {
    ++use_counts_[feature];
  }

  // Returns true iff we're parsing the first function literal during
  // CreateDynamicFunction().
  V8_INLINE bool ParsingDynamicFunctionDeclaration() const {
    return parameters_end_pos_ != kNoSourcePosition;
  }

  V8_INLINE void ConvertBinaryToNaryOperationSourceRange(
      BinaryOperation* binary_op, NaryOperation* nary_op) {
    if (source_range_map_ == nullptr) return;
    DCHECK_NULL(source_range_map_->Find(nary_op));

    BinaryOperationSourceRanges* ranges =
        static_cast<BinaryOperationSourceRanges*>(
            source_range_map_->Find(binary_op));
    if (ranges == nullptr) return;

    SourceRange range = ranges->GetRange(SourceRangeKind::kRight);
    source_range_map_->Insert(
        nary_op, new (zone()) NaryOperationSourceRanges(zone(), range));
  }

  V8_INLINE void AppendNaryOperationSourceRange(NaryOperation* node,
                                                const SourceRange& range) {
    if (source_range_map_ == nullptr) return;
    NaryOperationSourceRanges* ranges =
        static_cast<NaryOperationSourceRanges*>(source_range_map_->Find(node));
    if (ranges == nullptr) return;

    ranges->AddRange(range);
    DCHECK_EQ(node->subsequent_length(), ranges->RangeCount());
  }

  V8_INLINE void RecordBlockSourceRange(Block* node,
                                        int32_t continuation_position) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node, new (zone()) BlockSourceRanges(continuation_position));
  }

  V8_INLINE void RecordCaseClauseSourceRange(CaseClause* node,
                                             const SourceRange& body_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(node,
                              new (zone()) CaseClauseSourceRanges(body_range));
  }

  V8_INLINE void RecordConditionalSourceRange(Expression* node,
                                              const SourceRange& then_range,
                                              const SourceRange& else_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node->AsConditional(),
        new (zone()) ConditionalSourceRanges(then_range, else_range));
  }

  V8_INLINE void RecordFunctionLiteralSourceRange(FunctionLiteral* node) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(node, new (zone()) FunctionLiteralSourceRanges);
  }

  V8_INLINE void RecordBinaryOperationSourceRange(
      Expression* node, const SourceRange& right_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(node->AsBinaryOperation(),
                              new (zone())
                                  BinaryOperationSourceRanges(right_range));
  }

  V8_INLINE void RecordJumpStatementSourceRange(Statement* node,
                                                int32_t continuation_position) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        static_cast<JumpStatement*>(node),
        new (zone()) JumpStatementSourceRanges(continuation_position));
  }

  V8_INLINE void RecordIfStatementSourceRange(Statement* node,
                                              const SourceRange& then_range,
                                              const SourceRange& else_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node->AsIfStatement(),
        new (zone()) IfStatementSourceRanges(then_range, else_range));
  }

  V8_INLINE void RecordIterationStatementSourceRange(
      IterationStatement* node, const SourceRange& body_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node, new (zone()) IterationStatementSourceRanges(body_range));
  }

  V8_INLINE void RecordSuspendSourceRange(Expression* node,
                                          int32_t continuation_position) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(static_cast<Suspend*>(node),
                              new (zone())
                                  SuspendSourceRanges(continuation_position));
  }

  V8_INLINE void RecordSwitchStatementSourceRange(
      Statement* node, int32_t continuation_position) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node->AsSwitchStatement(),
        new (zone()) SwitchStatementSourceRanges(continuation_position));
  }

  V8_INLINE void RecordThrowSourceRange(Statement* node,
                                        int32_t continuation_position) {
    if (source_range_map_ == nullptr) return;
    ExpressionStatement* expr_stmt = static_cast<ExpressionStatement*>(node);
    Throw* throw_expr = expr_stmt->expression()->AsThrow();
    source_range_map_->Insert(
        throw_expr, new (zone()) ThrowSourceRanges(continuation_position));
  }

  V8_INLINE void RecordTryCatchStatementSourceRange(
      TryCatchStatement* node, const SourceRange& body_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node, new (zone()) TryCatchStatementSourceRanges(body_range));
  }

  V8_INLINE void RecordTryFinallyStatementSourceRange(
      TryFinallyStatement* node, const SourceRange& body_range) {
    if (source_range_map_ == nullptr) return;
    source_range_map_->Insert(
        node, new (zone()) TryFinallyStatementSourceRanges(body_range));
  }

  // Generate the next internal variable name for binding an exported namespace
  // object (used to implement the "export * as" syntax).
  const AstRawString* NextInternalNamespaceExportName();

  ParseInfo* info() const { return info_; }

  std::vector<uint8_t>* preparse_data_buffer() {
    return &preparse_data_buffer_;
  }

  // Parser's private field members.
  friend class PreParserZoneScope;  // Uses reusable_preparser().
  friend class PreparseDataBuilder;  // Uses preparse_data_buffer()

  ParseInfo* info_;
  Scanner scanner_;
  Zone preparser_zone_;
  PreParser* reusable_preparser_;
  Mode mode_;

  SourceRangeMap* source_range_map_ = nullptr;

  friend class ParserTarget;
  friend class ParserTargetScope;
  ParserTarget* target_stack_;  // for break, continue statements

  ScriptCompiler::CompileOptions compile_options_;

  // For NextInternalNamespaceExportName().
  int number_of_named_namespace_exports_ = 0;

  // Other information which will be stored in Parser and moved to Isolate after
  // parsing.
  int use_counts_[v8::Isolate::kUseCounterFeatureCount];
  int total_preparse_skipped_;
  bool allow_lazy_;
  bool temp_zoned_;
  ConsumedPreparseData* consumed_preparse_data_;
  std::vector<uint8_t> preparse_data_buffer_;

  // If not kNoSourcePosition, indicates that the first function literal
  // encountered is a dynamic function, see CreateDynamicFunction(). This field
  // indicates the correct position of the ')' that closes the parameter list.
  // After that ')' is encountered, this field is reset to kNoSourcePosition.
  int parameters_end_pos_;
};

// ----------------------------------------------------------------------------
// Target is a support class to facilitate manipulation of the
// Parser's target_stack_ (the stack of potential 'break' and
// 'continue' statement targets). Upon construction, a new target is
// added; it is removed upon destruction.

class ParserTarget {
 public:
  ParserTarget(ParserBase<Parser>* parser, BreakableStatement* statement)
      : variable_(&parser->impl()->target_stack_),
        statement_(statement),
        previous_(parser->impl()->target_stack_) {
    parser->impl()->target_stack_ = this;
  }

  ~ParserTarget() { *variable_ = previous_; }

  ParserTarget* previous() { return previous_; }
  BreakableStatement* statement() { return statement_; }

 private:
  ParserTarget** variable_;
  BreakableStatement* statement_;
  ParserTarget* previous_;
};

class ParserTargetScope {
 public:
  explicit ParserTargetScope(ParserBase<Parser>* parser)
      : variable_(&parser->impl()->target_stack_),
        previous_(parser->impl()->target_stack_) {
    parser->impl()->target_stack_ = nullptr;
  }

  ~ParserTargetScope() { *variable_ = previous_; }

 private:
  ParserTarget** variable_;
  ParserTarget* previous_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSER_H_
