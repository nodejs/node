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
#include "src/globals.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/parsing.h"
#include "src/parsing/preparse-data-format.h"
#include "src/parsing/preparse-data.h"
#include "src/parsing/preparser.h"
#include "src/utils.h"

namespace v8 {

class ScriptCompiler;

namespace internal {

class ConsumedPreParsedScopeData;
class ParseInfo;
class ScriptData;
class ParserTarget;
class ParserTargetScope;
class PendingCompilationErrorHandler;
class PreParsedScopeData;

class FunctionEntry BASE_EMBEDDED {
 public:
  enum {
    kStartPositionIndex,
    kEndPositionIndex,
    kNumParametersIndex,
    kFlagsIndex,
    kNumInnerFunctionsIndex,
    kSize
  };

  explicit FunctionEntry(Vector<unsigned> backing)
    : backing_(backing) { }

  FunctionEntry() : backing_() { }

  class LanguageModeField : public BitField<LanguageMode, 0, 1> {};
  class UsesSuperPropertyField
      : public BitField<bool, LanguageModeField::kNext, 1> {};

  static uint32_t EncodeFlags(LanguageMode language_mode,
                              bool uses_super_property) {
    return LanguageModeField::encode(language_mode) |
           UsesSuperPropertyField::encode(uses_super_property);
  }

  int start_pos() const { return backing_[kStartPositionIndex]; }
  int end_pos() const { return backing_[kEndPositionIndex]; }
  int num_parameters() const { return backing_[kNumParametersIndex]; }
  LanguageMode language_mode() const {
    return LanguageModeField::decode(backing_[kFlagsIndex]);
  }
  bool uses_super_property() const {
    return UsesSuperPropertyField::decode(backing_[kFlagsIndex]);
  }
  int num_inner_functions() const { return backing_[kNumInnerFunctionsIndex]; }

  bool is_valid() const { return !backing_.is_empty(); }

 private:
  Vector<unsigned> backing_;
};


// Wrapper around ScriptData to provide parser-specific functionality.
class ParseData {
 public:
  static ParseData* FromCachedData(ScriptData* cached_data) {
    ParseData* pd = new ParseData(cached_data);
    if (pd->IsSane()) return pd;
    cached_data->Reject();
    delete pd;
    return nullptr;
  }

  void Initialize();
  FunctionEntry GetFunctionEntry(int start);
  int FunctionCount();

  unsigned* Data() {  // Writable data as unsigned int array.
    return reinterpret_cast<unsigned*>(const_cast<byte*>(script_data_->data()));
  }

  void Reject() { script_data_->Reject(); }

  bool rejected() const { return script_data_->rejected(); }

 private:
  explicit ParseData(ScriptData* script_data) : script_data_(script_data) {}

  bool IsSane();
  unsigned Magic();
  unsigned Version();
  int FunctionsSize();
  int Length() const {
    // Script data length is already checked to be a multiple of unsigned size.
    return script_data_->length() / sizeof(unsigned);
  }

  ScriptData* script_data_;
  int function_index_;

  DISALLOW_COPY_AND_ASSIGN(ParseData);
};

// ----------------------------------------------------------------------------
// JAVASCRIPT PARSING

class Parser;


struct ParserFormalParameters : FormalParametersBase {
  struct Parameter : public ZoneObject {
    Parameter(const AstRawString* name, Expression* pattern,
              Expression* initializer, int position,
              int initializer_end_position, bool is_rest)
        : name(name),
          pattern(pattern),
          initializer(initializer),
          position(position),
          initializer_end_position(initializer_end_position),
          is_rest(is_rest) {}
    const AstRawString* name;
    Expression* pattern;
    Expression* initializer;
    int position;
    int initializer_end_position;
    bool is_rest;
    Parameter* next_parameter = nullptr;
    bool is_simple() const {
      return pattern->IsVariableProxy() && initializer == nullptr && !is_rest;
    }

    Parameter** next() { return &next_parameter; }
    Parameter* const* next() const { return &next_parameter; }
  };

  explicit ParserFormalParameters(DeclarationScope* scope)
      : FormalParametersBase(scope) {}
  ThreadedList<Parameter> params;
};

template <>
struct ParserTypes<Parser> {
  typedef ParserBase<Parser> Base;
  typedef Parser Impl;

  // Return types for traversing functions.
  typedef const AstRawString* Identifier;
  typedef v8::internal::Expression* Expression;
  typedef v8::internal::FunctionLiteral* FunctionLiteral;
  typedef ObjectLiteral::Property* ObjectLiteralProperty;
  typedef ClassLiteral::Property* ClassLiteralProperty;
  typedef v8::internal::Suspend* Suspend;
  typedef v8::internal::RewritableExpression* RewritableExpression;
  typedef ZoneList<v8::internal::Expression*>* ExpressionList;
  typedef ZoneList<ObjectLiteral::Property*>* ObjectPropertyList;
  typedef ZoneList<ClassLiteral::Property*>* ClassPropertyList;
  typedef ParserFormalParameters FormalParameters;
  typedef v8::internal::Statement* Statement;
  typedef ZoneList<v8::internal::Statement*>* StatementList;
  typedef v8::internal::Block* Block;
  typedef v8::internal::BreakableStatement* BreakableStatement;
  typedef v8::internal::ForStatement* ForStatement;
  typedef v8::internal::IterationStatement* IterationStatement;

  // For constructing objects returned by the traversing functions.
  typedef AstNodeFactory Factory;

  typedef ParserTarget Target;
  typedef ParserTargetScope TargetScope;
};

class V8_EXPORT_PRIVATE Parser : public NON_EXPORTED_BASE(ParserBase<Parser>) {
 public:
  explicit Parser(ParseInfo* info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = nullptr;
    delete cached_parse_data_;
    cached_parse_data_ = nullptr;
  }

  static bool IsPreParser() { return false; }

  void ParseOnBackground(ParseInfo* info);

  // Deserialize the scope chain prior to parsing in which the script is going
  // to be executed. If the script is a top-level script, or the scope chain
  // consists of only a native context, maybe_outer_scope_info should be an
  // empty handle.
  //
  // This only deserializes the scope chain, but doesn't connect the scopes to
  // their corresponding scope infos. Therefore, looking up variables in the
  // deserialized scopes is not possible.
  void DeserializeScopeChain(ParseInfo* info,
                             MaybeHandle<ScopeInfo> maybe_outer_scope_info);

  // Move statistics to Isolate
  void UpdateStatistics(Isolate* isolate, Handle<Script> script);
  void HandleSourceURLComments(Isolate* isolate, Handle<Script> script);

 private:
  friend class ParserBase<Parser>;
  friend class v8::internal::ExpressionClassifier<ParserTypes<Parser>>;
  friend bool v8::internal::parsing::ParseProgram(ParseInfo*, Isolate*);
  friend bool v8::internal::parsing::ParseFunction(
      ParseInfo*, Handle<SharedFunctionInfo> shared_info, Isolate*);

  bool AllowsLazyParsingWithoutUnresolvedVariables() const {
    return scope()->AllowsLazyParsingWithoutUnresolvedVariables(
        original_scope_);
  }

  bool parse_lazily() const { return mode_ == PARSE_LAZILY; }
  enum Mode { PARSE_LAZILY, PARSE_EAGERLY };

  class ParsingModeScope BASE_EMBEDDED {
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
  FunctionLiteral* DoParseFunction(ParseInfo* info,
                                   const AstRawString* raw_name);

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(ParseInfo* info);

  void SetCachedData(ParseInfo* info);

  void StitchAst(ParseInfo* top_level_parse_info, Isolate* isolate);

  ScriptCompiler::CompileOptions compile_options() const {
    return compile_options_;
  }
  bool consume_cached_parse_data() const {
    return compile_options_ == ScriptCompiler::kConsumeParserCache;
  }
  bool produce_cached_parse_data() const {
    return compile_options_ == ScriptCompiler::kProduceParserCache;
  }

  PreParser* reusable_preparser() {
    if (reusable_preparser_ == nullptr) {
      reusable_preparser_ =
          new PreParser(zone(), &scanner_, stack_limit_, ast_value_factory(),
                        pending_error_handler(), runtime_call_stats_, logger_,
                        -1, parsing_module_, parsing_on_main_thread_);
#define SET_ALLOW(name) reusable_preparser_->set_allow_##name(allow_##name());
      SET_ALLOW(natives);
      SET_ALLOW(harmony_do_expressions);
      SET_ALLOW(harmony_function_sent);
      SET_ALLOW(harmony_public_fields);
      SET_ALLOW(harmony_dynamic_import);
      SET_ALLOW(harmony_import_meta);
      SET_ALLOW(harmony_async_iteration);
      SET_ALLOW(harmony_bigint);
#undef SET_ALLOW
    }
    return reusable_preparser_;
  }

  void ParseModuleItemList(ZoneList<Statement*>* body, bool* ok);
  Statement* ParseModuleItem(bool* ok);
  const AstRawString* ParseModuleSpecifier(bool* ok);
  void ParseImportDeclaration(bool* ok);
  Statement* ParseExportDeclaration(bool* ok);
  Statement* ParseExportDefault(bool* ok);
  void ParseExportClause(ZoneList<const AstRawString*>* export_names,
                         ZoneList<Scanner::Location>* export_locations,
                         ZoneList<const AstRawString*>* local_names,
                         Scanner::Location* reserved_loc, bool* ok);
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
  ZoneList<const NamedImport*>* ParseNamedImports(int pos, bool* ok);
  Block* BuildInitializationBlock(DeclarationParsingResult* parsing_result,
                                  ZoneList<const AstRawString*>* names,
                                  bool* ok);
  ZoneList<const AstRawString*>* DeclareLabel(
      ZoneList<const AstRawString*>* labels, VariableProxy* expr, bool* ok);
  bool ContainsLabel(ZoneList<const AstRawString*>* labels,
                     const AstRawString* label);
  Expression* RewriteReturn(Expression* return_value, int pos);
  Statement* RewriteSwitchStatement(SwitchStatement* switch_statement,
                                    Scope* scope);
  void RewriteCatchPattern(CatchInfo* catch_info, bool* ok);
  void ValidateCatchBlock(const CatchInfo& catch_info, bool* ok);
  Statement* RewriteTryStatement(Block* try_block, Block* catch_block,
                                 const SourceRange& catch_range,
                                 Block* finally_block,
                                 const SourceRange& finally_range,
                                 const CatchInfo& catch_info, int pos);
  void ParseAndRewriteGeneratorFunctionBody(int pos, FunctionKind kind,
                                            ZoneList<Statement*>* body,
                                            bool* ok);
  void ParseAndRewriteAsyncGeneratorFunctionBody(int pos, FunctionKind kind,
                                                 ZoneList<Statement*>* body,
                                                 bool* ok);
  void DeclareFunctionNameVar(const AstRawString* function_name,
                              FunctionLiteral::FunctionType function_type,
                              DeclarationScope* function_scope);

  Statement* DeclareFunction(const AstRawString* variable_name,
                             FunctionLiteral* function, VariableMode mode,
                             int pos, bool is_sloppy_block_function,
                             ZoneList<const AstRawString*>* names, bool* ok);
  Variable* CreateSyntheticContextVariable(const AstRawString* synthetic_name,
                                           bool* ok);
  FunctionLiteral* CreateInitializerFunction(
      DeclarationScope* scope, ZoneList<ClassLiteral::Property*>* fields);
  V8_INLINE Statement* DeclareClass(const AstRawString* variable_name,
                                    Expression* value,
                                    ZoneList<const AstRawString*>* names,
                                    int class_token_pos, int end_pos, bool* ok);
  V8_INLINE void DeclareClassVariable(const AstRawString* name,
                                      ClassInfo* class_info,
                                      int class_token_pos, bool* ok);
  V8_INLINE void DeclareClassProperty(const AstRawString* class_name,
                                      ClassLiteralProperty* property,
                                      ClassLiteralProperty::Kind kind,
                                      bool is_static, bool is_constructor,
                                      bool is_computed_name,
                                      ClassInfo* class_info, bool* ok);
  V8_INLINE Expression* RewriteClassLiteral(Scope* block_scope,
                                            const AstRawString* name,
                                            ClassInfo* class_info, int pos,
                                            int end_pos, bool* ok);
  V8_INLINE Statement* DeclareNative(const AstRawString* name, int pos,
                                     bool* ok);

  V8_INLINE Block* IgnoreCompletion(Statement* statement);

  V8_INLINE Scope* NewHiddenCatchScope();

  // PatternRewriter and associated methods defined in pattern-rewriter.cc.
  friend class PatternRewriter;
  void DeclareAndInitializeVariables(
      Block* block, const DeclarationDescriptor* declaration_descriptor,
      const DeclarationParsingResult::Declaration* declaration,
      ZoneList<const AstRawString*>* names, bool* ok);
  void RewriteDestructuringAssignment(RewritableExpression* expr);
  Expression* RewriteDestructuringAssignment(Assignment* assignment);

  // [if (IteratorType == kAsync)]
  //     !%_IsJSReceiver(result = Await(iterator.next()) &&
  //         %ThrowIteratorResultNotAnObject(result)
  // [else]
  //     !%_IsJSReceiver(result = iterator.next()) &&
  //         %ThrowIteratorResultNotAnObject(result)
  // [endif]
  Expression* BuildIteratorNextResult(Expression* iterator, Variable* result,
                                      IteratorType type, int pos);

  // Initialize the components of a for-in / for-of statement.
  Statement* InitializeForEachStatement(ForEachStatement* stmt,
                                        Expression* each, Expression* subject,
                                        Statement* body);
  Statement* InitializeForOfStatement(ForOfStatement* stmt, Expression* each,
                                      Expression* iterable, Statement* body,
                                      bool finalize, IteratorType type,
                                      int next_result_pos = kNoSourcePosition);

  Block* RewriteForVarInLegacy(const ForInfo& for_info);
  void DesugarBindingInForEachStatement(ForInfo* for_info, Block** body_block,
                                        Expression** each_variable, bool* ok);
  Block* CreateForEachStatementTDZ(Block* init_block, const ForInfo& for_info,
                                   bool* ok);

  Statement* DesugarLexicalBindingsInForStatement(
      ForStatement* loop, Statement* init, Expression* cond, Statement* next,
      Statement* body, Scope* inner_scope, const ForInfo& for_info, bool* ok);

  Expression* RewriteDoExpression(Block* body, int pos, bool* ok);

  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      LanguageMode language_mode, bool* ok);

  // Check if the scope has conflicting var/let declarations from different
  // scopes. This covers for example
  //
  // function f() { { { var x; } let x; } }
  // function g() { { var x; let x; } }
  //
  // The var declarations are hoisted to the function scope, but originate from
  // a scope where the name has also been let bound or the var declaration is
  // hoisted over such a scope.
  void CheckConflictingVarDeclarations(Scope* scope, bool* ok);

  // Insert initializer statements for var-bindings shadowing parameter bindings
  // from a non-simple parameter list.
  void InsertShadowingVarBindingInitializers(Block* block);

  // Implement sloppy block-scoped functions, ES2015 Annex B 3.3
  void InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope);

  VariableProxy* NewUnresolved(const AstRawString* name, int begin_pos,
                               VariableKind kind = NORMAL_VARIABLE);
  VariableProxy* NewUnresolved(const AstRawString* name);
  Variable* Declare(Declaration* declaration,
                    DeclarationDescriptor::Kind declaration_kind,
                    VariableMode mode, InitializationFlag init, bool* ok,
                    Scope* declaration_scope = nullptr,
                    int var_end_pos = kNoSourcePosition);
  Declaration* DeclareVariable(const AstRawString* name, VariableMode mode,
                               int pos, bool* ok);
  Declaration* DeclareVariable(const AstRawString* name, VariableMode mode,
                               InitializationFlag init, int pos, bool* ok);

  bool TargetStackContainsLabel(const AstRawString* label);
  BreakableStatement* LookupBreakTarget(const AstRawString* label, bool* ok);
  IterationStatement* LookupContinueTarget(const AstRawString* label, bool* ok);

  Statement* BuildAssertIsCoercible(Variable* var, ObjectLiteral* pattern);

  // Factory methods.
  FunctionLiteral* DefaultConstructor(const AstRawString* name, bool call_super,
                                      int pos, int end_pos);

  // Skip over a lazy function, either using cached data if we have it, or
  // by parsing the function with PreParser. Consumes the ending }.
  // If may_abort == true, the (pre-)parser may decide to abort skipping
  // in order to force the function to be eagerly parsed, after all.
  LazyParsingResult SkipFunction(
      const AstRawString* function_name, FunctionKind kind,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope, int* num_parameters,
      ProducedPreParsedScopeData** produced_preparsed_scope_data,
      bool is_inner_function, bool may_abort, bool* ok);

  Block* BuildParameterInitializationBlock(
      const ParserFormalParameters& parameters, bool* ok);
  Block* BuildRejectPromiseOnException(Block* block);

  ZoneList<Statement*>* ParseFunction(
      const AstRawString* function_name, int pos, FunctionKind kind,
      FunctionLiteral::FunctionType function_type,
      DeclarationScope* function_scope, int* num_parameters,
      int* function_length, bool* has_duplicate_parameters,
      int* expected_property_count, bool* ok);

  void ThrowPendingError(Isolate* isolate, Handle<Script> script);

  class TemplateLiteral : public ZoneObject {
   public:
    TemplateLiteral(Zone* zone, int pos)
        : cooked_(8, zone), raw_(8, zone), expressions_(8, zone), pos_(pos) {}

    const ZoneList<const AstRawString*>* cooked() const { return &cooked_; }
    const ZoneList<const AstRawString*>* raw() const { return &raw_; }
    const ZoneList<Expression*>* expressions() const { return &expressions_; }
    int position() const { return pos_; }

    void AddTemplateSpan(const AstRawString* cooked, const AstRawString* raw,
                         int end, Zone* zone) {
      DCHECK_NOT_NULL(raw);
      cooked_.Add(cooked, zone);
      raw_.Add(raw, zone);
    }

    void AddExpression(Expression* expression, Zone* zone) {
      DCHECK_NOT_NULL(expression);
      expressions_.Add(expression, zone);
    }

   private:
    ZoneList<const AstRawString*> cooked_;
    ZoneList<const AstRawString*> raw_;
    ZoneList<Expression*> expressions_;
    int pos_;
  };

  typedef TemplateLiteral* TemplateLiteralState;

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
  int32_t ComputeTemplateLiteralHash(const TemplateLiteral* lit);

  ZoneList<Expression*>* PrepareSpreadArguments(ZoneList<Expression*>* list);
  Expression* SpreadCall(Expression* function, ZoneList<Expression*>* args,
                         int pos, Call::PossiblyEval is_possibly_eval);
  Expression* SpreadCallNew(Expression* function, ZoneList<Expression*>* args,
                            int pos);
  Expression* RewriteSuperCall(Expression* call_expression);

  void SetLanguageMode(Scope* scope, LanguageMode mode);
  void SetAsmModule();

  // Rewrite all DestructuringAssignments in the current FunctionState.
  V8_INLINE void RewriteDestructuringAssignments();

  Expression* RewriteSpreads(ArrayLiteral* lit);

  // Rewrite expressions that are not used as patterns
  V8_INLINE void RewriteNonPattern(bool* ok);

  V8_INLINE void QueueDestructuringAssignmentForRewriting(
      RewritableExpression* assignment);
  V8_INLINE void QueueNonPatternForRewriting(RewritableExpression* expr,
                                             bool* ok);

  friend class InitializerRewriter;
  void RewriteParameterInitializer(Expression* expr);

  Expression* BuildInitialYield(int pos, FunctionKind kind);
  Assignment* BuildCreateJSGeneratorObject(int pos, FunctionKind kind);
  Expression* BuildResolvePromise(Expression* value, int pos);
  Expression* BuildRejectPromise(Expression* value, int pos);
  Variable* PromiseVariable();
  Variable* AsyncGeneratorAwaitVariable();

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(Runtime::FunctionId function_id,
                            MessageTemplate::Template message,
                            const AstRawString* arg, int pos);

  void FinalizeIteratorUse(Variable* completion, Expression* condition,
                           Variable* iter, Block* iterator_use, Block* result,
                           IteratorType type);

  Statement* FinalizeForOfStatement(ForOfStatement* loop, Variable* completion,
                                    IteratorType type, int pos);
  void BuildIteratorClose(ZoneList<Statement*>* statements, Variable* iterator,
                          Variable* input, Variable* output, IteratorType type);
  void BuildIteratorCloseForCompletion(ZoneList<Statement*>* statements,
                                       Variable* iterator,
                                       Expression* completion,
                                       IteratorType type);
  Statement* CheckCallable(Variable* var, Expression* error, int pos);

  V8_INLINE void RewriteAsyncFunctionBody(ZoneList<Statement*>* body,
                                          Block* block,
                                          Expression* return_value, bool* ok);

  void AddArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                        Expression* params, int end_pos,
                                        bool* ok);
  void SetFunctionName(Expression* value, const AstRawString* name,
                       const AstRawString* prefix = nullptr);

  // Helper functions for recursive descent.
  V8_INLINE bool IsEval(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->eval_string();
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
    return property != nullptr && property->obj()->IsVariableProxy() &&
           property->obj()->AsVariableProxy()->is_this();
  }

  // This returns true if the expression is an indentifier (wrapped
  // inside a variable proxy).  We exclude the case of 'this', which
  // has been converted to a variable proxy.
  V8_INLINE static bool IsIdentifier(Expression* expression) {
    DCHECK_NOT_NULL(expression);
    VariableProxy* operand = expression->AsVariableProxy();
    return operand != nullptr && !operand->is_this() &&
           !operand->is_new_target();
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

  V8_INLINE bool IsUseStrictDirective(Statement* statement) const {
    return IsStringLiteral(statement, ast_value_factory()->use_strict_string());
  }

  V8_INLINE bool IsUseAsmDirective(Statement* statement) const {
    return IsStringLiteral(statement, ast_value_factory()->use_asm_string());
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

  V8_INLINE void GetDefaultStrings(
      const AstRawString** default_string,
      const AstRawString** star_default_star_string) {
    *default_string = ast_value_factory()->default_string();
    *star_default_star_string = ast_value_factory()->star_default_star_string();
  }

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  V8_INLINE void PushLiteralName(const AstRawString* id) {
    DCHECK_NOT_NULL(fni_);
    fni_->PushLiteralName(id);
  }

  V8_INLINE void PushVariableName(const AstRawString* id) {
    DCHECK_NOT_NULL(fni_);
    fni_->PushVariableName(id);
  }

  V8_INLINE void PushPropertyName(Expression* expression) {
    DCHECK_NOT_NULL(fni_);
    if (expression->IsPropertyName()) {
      fni_->PushLiteralName(expression->AsLiteral()->AsRawPropertyName());
    } else {
      fni_->PushLiteralName(ast_value_factory()->anonymous_function_string());
    }
  }

  V8_INLINE void PushEnclosingName(const AstRawString* name) {
    DCHECK_NOT_NULL(fni_);
    fni_->PushEnclosingName(name);
  }

  V8_INLINE void AddFunctionForNameInference(FunctionLiteral* func_to_infer) {
    DCHECK_NOT_NULL(fni_);
    fni_->AddFunction(func_to_infer);
  }

  V8_INLINE void InferFunctionName() {
    DCHECK_NOT_NULL(fni_);
    fni_->Infer();
  }

  // If we assign a function literal to a property we pretenure the
  // literal so it can be added as a constant function property.
  V8_INLINE static void CheckAssigningFunctionLiteralToProperty(
      Expression* left, Expression* right) {
    DCHECK_NOT_NULL(left);
    if (left->IsProperty() && right->IsFunctionLiteral()) {
      right->AsFunctionLiteral()->set_pretenure();
    }
  }

  // Determine if the expression is a variable proxy and mark it as being used
  // in an assignment or with a increment/decrement operator.
  V8_INLINE static void MarkExpressionAsAssigned(Expression* expression) {
    DCHECK_NOT_NULL(expression);
    if (expression->IsVariableProxy()) {
      expression->AsVariableProxy()->set_is_assigned();
    }
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

  // Rewrites the following types of unary expressions:
  // not <literal> -> true / false
  // + <numeric literal> -> <numeric literal>
  // - <numeric literal> -> <numeric literal with value negated>
  // ! <literal> -> true / false
  // The following rewriting rules enable the collection of type feedback
  // without any special stub and the multiplication is removed later in
  // Crankshaft's canonicalization pass.
  // + foo -> foo * 1
  // - foo -> foo * (-1)
  // ~ foo -> foo ^(~0)
  Expression* BuildUnaryExpression(Expression* expression, Token::Value op,
                                   int pos);

  // Generate AST node that throws a ReferenceError with the given type.
  V8_INLINE Expression* NewThrowReferenceError(
      MessageTemplate::Template message, int pos) {
    return NewThrowError(Runtime::kNewReferenceError, message,
                         ast_value_factory()->empty_string(), pos);
  }

  // Generate AST node that throws a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  V8_INLINE Expression* NewThrowSyntaxError(MessageTemplate::Template message,
                                            const AstRawString* arg, int pos) {
    return NewThrowError(Runtime::kNewSyntaxError, message, arg, pos);
  }

  // Generate AST node that throws a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  V8_INLINE Expression* NewThrowTypeError(MessageTemplate::Template message,
                                          const AstRawString* arg, int pos) {
    return NewThrowError(Runtime::kNewTypeError, message, arg, pos);
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const char* arg = nullptr,
                       ParseErrorType error_type = kSyntaxError) {
    if (stack_overflow()) {
      // Suppress the error message (syntax error or such) in the presence of a
      // stack overflow. The isolate allows only one pending exception at at
      // time
      // and we want to report the stack overflow later.
      return;
    }
    pending_error_handler()->ReportMessageAt(source_location.beg_pos,
                                             source_location.end_pos, message,
                                             arg, error_type);
  }

  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const AstRawString* arg,
                       ParseErrorType error_type = kSyntaxError) {
    if (stack_overflow()) {
      // Suppress the error message (syntax error or such) in the presence of a
      // stack overflow. The isolate allows only one pending exception at at
      // time
      // and we want to report the stack overflow later.
      return;
    }
    pending_error_handler()->ReportMessageAt(source_location.beg_pos,
                                             source_location.end_pos, message,
                                             arg, error_type);
  }

  // "null" return type creators.
  V8_INLINE static std::nullptr_t NullIdentifier() { return nullptr; }
  V8_INLINE static std::nullptr_t NullExpression() { return nullptr; }
  V8_INLINE static std::nullptr_t NullLiteralProperty() { return nullptr; }
  V8_INLINE static ZoneList<Expression*>* NullExpressionList() {
    return nullptr;
  }
  V8_INLINE static ZoneList<Statement*>* NullStatementList() { return nullptr; }
  V8_INLINE static std::nullptr_t NullStatement() { return nullptr; }

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

  V8_INLINE const AstRawString* GetNextSymbol() const {
    return scanner()->NextSymbol(ast_value_factory());
  }

  V8_INLINE const AstRawString* GetNumberAsSymbol() const {
    double double_value = scanner()->DoubleValue();
    char array[100];
    const char* string = DoubleToCString(double_value, ArrayVector(array));
    return ast_value_factory()->GetOneByteString(string);
  }

  V8_INLINE Expression* ThisExpression(int pos = kNoSourcePosition) {
    return NewUnresolved(ast_value_factory()->this_string(), pos,
                         THIS_VARIABLE);
  }

  Expression* NewSuperPropertyReference(int pos);
  Expression* NewSuperCallReference(int pos);
  Expression* NewTargetExpression(int pos);
  Expression* FunctionSentExpression(int pos);
  Expression* ImportMetaExpression(int pos);

  Literal* ExpressionFromLiteral(Token::Value token, int pos);

  V8_INLINE Expression* ExpressionFromIdentifier(
      const AstRawString* name, int start_position,
      InferName infer = InferName::kYes) {
    if (infer == InferName::kYes) {
      fni_->PushVariableName(name);
    }
    return NewUnresolved(name, start_position);
  }

  V8_INLINE Expression* ExpressionFromString(int pos) {
    const AstRawString* symbol = GetSymbol();
    fni_->PushLiteralName(symbol);
    return factory()->NewStringLiteral(symbol, pos);
  }

  V8_INLINE ZoneList<Expression*>* NewExpressionList(int size) const {
    return new (zone()) ZoneList<Expression*>(size, zone());
  }
  V8_INLINE ZoneList<ObjectLiteral::Property*>* NewObjectPropertyList(
      int size) const {
    return new (zone()) ZoneList<ObjectLiteral::Property*>(size, zone());
  }
  V8_INLINE ZoneList<ClassLiteral::Property*>* NewClassPropertyList(
      int size) const {
    return new (zone()) ZoneList<ClassLiteral::Property*>(size, zone());
  }
  V8_INLINE ZoneList<Statement*>* NewStatementList(int size) const {
    return new (zone()) ZoneList<Statement*>(size, zone());
  }

  V8_INLINE Expression* NewV8Intrinsic(const AstRawString* name,
                                       ZoneList<Expression*>* args, int pos,
                                       bool* ok);

  V8_INLINE Statement* NewThrowStatement(Expression* exception, int pos) {
    return factory()->NewExpressionStatement(
        factory()->NewThrow(exception, pos), pos);
  }

  V8_INLINE void AddParameterInitializationBlock(
      const ParserFormalParameters& parameters, ZoneList<Statement*>* body,
      bool is_async, bool* ok) {
    if (parameters.is_simple) return;
    auto* init_block = BuildParameterInitializationBlock(parameters, ok);
    if (!*ok) return;
    if (is_async) {
      init_block = BuildRejectPromiseOnException(init_block);
    }
    body->Add(init_block, zone());
  }

  V8_INLINE void AddFormalParameter(ParserFormalParameters* parameters,
                                    Expression* pattern,
                                    Expression* initializer,
                                    int initializer_end_position,
                                    bool is_rest) {
    parameters->UpdateArityAndFunctionLength(initializer != nullptr, is_rest);
    bool has_simple_name = pattern->IsVariableProxy() && initializer == nullptr;
    const AstRawString* name = has_simple_name
                                   ? pattern->AsVariableProxy()->raw_name()
                                   : ast_value_factory()->empty_string();
    auto parameter = new (parameters->scope->zone())
        ParserFormalParameters::Parameter(name, pattern, initializer,
                                          scanner()->location().beg_pos,
                                          initializer_end_position, is_rest);

    parameters->params.Add(parameter);
  }

  V8_INLINE void DeclareFormalParameters(
      DeclarationScope* scope,
      const ThreadedList<ParserFormalParameters::Parameter>& parameters,
      bool is_simple, bool* has_duplicate = nullptr) {
    if (!is_simple) scope->SetHasNonSimpleParameters();
    for (auto parameter : parameters) {
      bool is_optional = parameter->initializer != nullptr;
      // If the parameter list is simple, declare the parameters normally with
      // their names. If the parameter list is not simple, declare a temporary
      // for each parameter - the corresponding named variable is declared by
      // BuildParamerterInitializationBlock.
      scope->DeclareParameter(
          is_simple ? parameter->name : ast_value_factory()->empty_string(),
          is_simple ? VAR : TEMPORARY, is_optional, parameter->is_rest,
          has_duplicate, ast_value_factory(), parameter->position);
    }
  }

  void DeclareArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                            Expression* params,
                                            const Scanner::Location& params_loc,
                                            Scanner::Location* duplicate_loc,
                                            bool* ok);

  Expression* ExpressionListToExpression(ZoneList<Expression*>* args);

  void SetFunctionNameFromPropertyName(LiteralProperty* property,
                                       const AstRawString* name,
                                       const AstRawString* prefix = nullptr);
  void SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                       const AstRawString* name,
                                       const AstRawString* prefix = nullptr);

  void SetFunctionNameFromIdentifierRef(Expression* value,
                                        Expression* identifier);

  V8_INLINE ZoneList<typename ExpressionClassifier::Error>*
  GetReportedErrorList() const {
    return function_state_->GetReportedErrorList();
  }

  V8_INLINE ZoneList<RewritableExpression*>* GetNonPatternList() const {
    return function_state_->non_patterns_to_rewrite();
  }

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

  // Parser's private field members.
  friend class DiscardableZoneScope;  // Uses reusable_preparser_.
  // FIXME(marja): Make reusable_preparser_ always use its own temp Zone (call
  // DeleteAll after each function), so this won't be needed.

  Scanner scanner_;
  PreParser* reusable_preparser_;
  Mode mode_;

  SourceRangeMap* source_range_map_ = nullptr;

  friend class ParserTarget;
  friend class ParserTargetScope;
  ParserTarget* target_stack_;  // for break, continue statements

  ScriptCompiler::CompileOptions compile_options_;
  ParseData* cached_parse_data_;

  // Other information which will be stored in Parser and moved to Isolate after
  // parsing.
  int use_counts_[v8::Isolate::kUseCounterFeatureCount];
  int total_preparse_skipped_;
  bool allow_lazy_;
  bool temp_zoned_;
  ParserLogger* log_;
  ConsumedPreParsedScopeData* consumed_preparsed_scope_data_;

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

class ParserTarget BASE_EMBEDDED {
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

class ParserTargetScope BASE_EMBEDDED {
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
