// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_H_
#define V8_PARSING_PARSER_H_

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/parsing.h"
#include "src/parsing/preparse-data-format.h"
#include "src/parsing/preparse-data.h"
#include "src/parsing/preparser.h"
#include "src/pending-compilation-error-handler.h"
#include "src/utils.h"

namespace v8 {

class ScriptCompiler;

namespace internal {

class ParseInfo;
class ScriptData;
class ParserTarget;
class ParserTargetScope;
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
    return NULL;
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

  typedef v8::internal::Variable Variable;

  // Return types for traversing functions.
  typedef const AstRawString* Identifier;
  typedef v8::internal::Expression* Expression;
  typedef v8::internal::FunctionLiteral* FunctionLiteral;
  typedef ObjectLiteral::Property* ObjectLiteralProperty;
  typedef ClassLiteral::Property* ClassLiteralProperty;
  typedef v8::internal::Suspend* Suspend;
  typedef ZoneList<v8::internal::Expression*>* ExpressionList;
  typedef ZoneList<ObjectLiteral::Property*>* ObjectPropertyList;
  typedef ZoneList<ClassLiteral::Property*>* ClassPropertyList;
  typedef ParserFormalParameters FormalParameters;
  typedef v8::internal::Statement* Statement;
  typedef ZoneList<v8::internal::Statement*>* StatementList;
  typedef v8::internal::Block* Block;
  typedef v8::internal::BreakableStatement* BreakableStatement;
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
    reusable_preparser_ = NULL;
    delete cached_parse_data_;
    cached_parse_data_ = NULL;
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

  // Handle errors detected during parsing
  void ReportErrors(Isolate* isolate, Handle<Script> script);
  // Move statistics to Isolate
  void UpdateStatistics(Isolate* isolate, Handle<Script> script);
  void HandleSourceURLComments(Isolate* isolate, Handle<Script> script);

 private:
  friend class ParserBase<Parser>;
  friend class v8::internal::ExpressionClassifier<ParserTypes<Parser>>;
  friend bool v8::internal::parsing::ParseProgram(ParseInfo*, Isolate*, bool);
  friend bool v8::internal::parsing::ParseFunction(ParseInfo*, Isolate*, bool);

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

  // Limit the allowed number of local variables in a function. The hard limit
  // is that offsets computed by FullCodeGenerator::StackOperand and similar
  // functions are ints, and they should not overflow. In addition, accessing
  // local variables creates user-controlled constants in the generated code,
  // and we don't want too much user-controlled memory inside the code (this was
  // the reason why this limit was introduced in the first place; see
  // https://codereview.chromium.org/7003030/ ).
  static const int kMaxNumFunctionLocals = 4194303;  // 2^22-1

  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram(Isolate* isolate, ParseInfo* info);

  FunctionLiteral* ParseFunction(Isolate* isolate, ParseInfo* info);
  FunctionLiteral* DoParseFunction(ParseInfo* info);

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
    if (reusable_preparser_ == NULL) {
      reusable_preparser_ =
          new PreParser(zone(), &scanner_, stack_limit_, ast_value_factory(),
                        &pending_error_handler_, runtime_call_stats_,
                        preparsed_scope_data_, parsing_on_main_thread_);
#define SET_ALLOW(name) reusable_preparser_->set_allow_##name(allow_##name());
      SET_ALLOW(natives);
      SET_ALLOW(tailcalls);
      SET_ALLOW(harmony_do_expressions);
      SET_ALLOW(harmony_function_sent);
      SET_ALLOW(harmony_trailing_commas);
      SET_ALLOW(harmony_class_fields);
      SET_ALLOW(harmony_object_rest_spread);
      SET_ALLOW(harmony_dynamic_import);
      SET_ALLOW(harmony_async_iteration);
      SET_ALLOW(harmony_template_escapes);
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
  void DeclareAndInitializeVariables(
      Block* block, const DeclarationDescriptor* declaration_descriptor,
      const DeclarationParsingResult::Declaration* declaration,
      ZoneList<const AstRawString*>* names, bool* ok);
  ZoneList<const AstRawString*>* DeclareLabel(
      ZoneList<const AstRawString*>* labels, VariableProxy* expr, bool* ok);
  bool ContainsLabel(ZoneList<const AstRawString*>* labels,
                     const AstRawString* label);
  Expression* RewriteReturn(Expression* return_value, int pos);
  Statement* RewriteSwitchStatement(Expression* tag,
                                    SwitchStatement* switch_statement,
                                    ZoneList<CaseClause*>* cases, Scope* scope);
  void RewriteCatchPattern(CatchInfo* catch_info, bool* ok);
  void ValidateCatchBlock(const CatchInfo& catch_info, bool* ok);
  Statement* RewriteTryStatement(Block* try_block, Block* catch_block,
                                 Block* finally_block,
                                 const CatchInfo& catch_info, int pos);

  void ParseAndRewriteGeneratorFunctionBody(int pos, FunctionKind kind,
                                            ZoneList<Statement*>* body,
                                            bool* ok);
  void CreateFunctionNameAssignment(const AstRawString* function_name, int pos,
                                    FunctionLiteral::FunctionType function_type,
                                    DeclarationScope* function_scope,
                                    ZoneList<Statement*>* result, int index);

  Statement* DeclareFunction(const AstRawString* variable_name,
                             FunctionLiteral* function, VariableMode mode,
                             int pos, bool is_sloppy_block_function,
                             ZoneList<const AstRawString*>* names, bool* ok);
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
                                      ClassInfo* class_info, bool* ok);
  V8_INLINE Expression* RewriteClassLiteral(Scope* block_scope,
                                            const AstRawString* name,
                                            ClassInfo* class_info, int pos,
                                            int end_pos, bool* ok);
  V8_INLINE Statement* DeclareNative(const AstRawString* name, int pos,
                                     bool* ok);

  V8_INLINE Block* IgnoreCompletion(Statement* statement);

  V8_INLINE Scope* NewHiddenCatchScopeWithParent(Scope* parent);

  class PatternRewriter final : public AstVisitor<PatternRewriter> {
   public:
    static void DeclareAndInitializeVariables(
        Parser* parser, Block* block,
        const DeclarationDescriptor* declaration_descriptor,
        const DeclarationParsingResult::Declaration* declaration,
        ZoneList<const AstRawString*>* names, bool* ok);

    static void RewriteDestructuringAssignment(Parser* parser,
                                               RewritableExpression* expr,
                                               Scope* Scope);

    static Expression* RewriteDestructuringAssignment(Parser* parser,
                                                      Assignment* assignment,
                                                      Scope* scope);

   private:
    PatternRewriter() {}

#define DECLARE_VISIT(type) void Visit##type(v8::internal::type* node);
    // Visiting functions for AST nodes make this an AstVisitor.
    AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

    enum PatternContext {
      BINDING,
      INITIALIZER,
      ASSIGNMENT,
      ASSIGNMENT_INITIALIZER
    };

    PatternContext context() const { return context_; }
    void set_context(PatternContext context) { context_ = context; }

    void RecurseIntoSubpattern(AstNode* pattern, Expression* value) {
      Expression* old_value = current_value_;
      current_value_ = value;
      recursion_level_++;
      Visit(pattern);
      recursion_level_--;
      current_value_ = old_value;
    }

    void VisitObjectLiteral(ObjectLiteral* node, Variable** temp_var);
    void VisitArrayLiteral(ArrayLiteral* node, Variable** temp_var);

    bool IsBindingContext() const {
      return context_ == BINDING || context_ == INITIALIZER;
    }
    bool IsInitializerContext() const { return context_ != ASSIGNMENT; }
    bool IsAssignmentContext() const {
      return context_ == ASSIGNMENT || context_ == ASSIGNMENT_INITIALIZER;
    }
    bool IsSubPattern() const { return recursion_level_ > 1; }
    PatternContext SetAssignmentContextIfNeeded(Expression* node);
    PatternContext SetInitializerContextIfNeeded(Expression* node);

    bool DeclaresParameterContainingSloppyEval() const;
    void RewriteParameterScopes(Expression* expr);

    Variable* CreateTempVar(Expression* value = nullptr);

    AstNodeFactory* factory() const { return parser_->factory(); }
    AstValueFactory* ast_value_factory() const {
      return parser_->ast_value_factory();
    }
    Zone* zone() const { return parser_->zone(); }
    Scope* scope() const { return scope_; }

    Scope* scope_;
    Parser* parser_;
    PatternContext context_;
    Expression* pattern_;
    int initializer_position_;
    Block* block_;
    const DeclarationDescriptor* descriptor_;
    ZoneList<const AstRawString*>* names_;
    Expression* current_value_;
    int recursion_level_;
    bool* ok_;

    DEFINE_AST_VISITOR_MEMBERS_WITHOUT_STACKOVERFLOW()
  };

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
                                        Statement* body, int each_keyword_pos);
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

  // Get odd-ball literals.
  Literal* GetLiteralUndefined(int position);

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

  Statement* BuildAssertIsCoercible(Variable* var);

  // Factory methods.
  FunctionLiteral* DefaultConstructor(const AstRawString* name, bool call_super,
                                      int pos, int end_pos);

  // Skip over a lazy function, either using cached data if we have it, or
  // by parsing the function with PreParser. Consumes the ending }.
  // If may_abort == true, the (pre-)parser may decide to abort skipping
  // in order to force the function to be eagerly parsed, after all.
  LazyParsingResult SkipFunction(FunctionKind kind,
                                 DeclarationScope* function_scope,
                                 int* num_parameters, bool is_inner_function,
                                 bool may_abort, bool* ok);

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

    const ZoneList<Expression*>* cooked() const { return &cooked_; }
    const ZoneList<Expression*>* raw() const { return &raw_; }
    const ZoneList<Expression*>* expressions() const { return &expressions_; }
    int position() const { return pos_; }

    void AddTemplateSpan(Literal* cooked, Literal* raw, int end, Zone* zone) {
      DCHECK_NOT_NULL(cooked);
      DCHECK_NOT_NULL(raw);
      cooked_.Add(cooked, zone);
      raw_.Add(raw, zone);
    }

    void AddExpression(Expression* expression, Zone* zone) {
      DCHECK_NOT_NULL(expression);
      expressions_.Add(expression, zone);
    }

   private:
    ZoneList<Expression*> cooked_;
    ZoneList<Expression*> raw_;
    ZoneList<Expression*> expressions_;
    int pos_;
  };

  typedef TemplateLiteral* TemplateLiteralState;

  TemplateLiteralState OpenTemplateLiteral(int pos);
  // "should_cook" means that the span can be "cooked": in tagged template
  // literals, both the raw and "cooked" representations are available to user
  // code ("cooked" meaning that escape sequences are converted to their
  // interpreted values). With the --harmony-template-escapes flag, invalid
  // escape sequences cause the cooked span to be represented by undefined,
  // instead of being a syntax error.
  // "tail" indicates that this span is the last in the literal.
  void AddTemplateSpan(TemplateLiteralState* state, bool should_cook,
                       bool tail);
  void AddTemplateExpression(TemplateLiteralState* state,
                             Expression* expression);
  Expression* CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                   Expression* tag);
  uint32_t ComputeTemplateLiteralHash(const TemplateLiteral* lit);

  ZoneList<Expression*>* PrepareSpreadArguments(ZoneList<Expression*>* list);
  Expression* SpreadCall(Expression* function, ZoneList<Expression*>* args,
                         int pos, Call::PossiblyEval is_possibly_eval);
  Expression* SpreadCallNew(Expression* function, ZoneList<Expression*>* args,
                            int pos);
  Expression* RewriteSuperCall(Expression* call_expression);

  void SetLanguageMode(Scope* scope, LanguageMode mode);
  void SetAsmModule();

  V8_INLINE void MarkCollectedTailCallExpressions();
  V8_INLINE void MarkTailPosition(Expression* expression);

  // Rewrite all DestructuringAssignments in the current FunctionState.
  V8_INLINE void RewriteDestructuringAssignments();

  V8_INLINE Expression* RewriteExponentiation(Expression* left,
                                              Expression* right, int pos);
  V8_INLINE Expression* RewriteAssignExponentiation(Expression* left,
                                                    Expression* right, int pos);

  friend class NonPatternRewriter;
  V8_INLINE Expression* RewriteSpreads(ArrayLiteral* lit);

  // Rewrite expressions that are not used as patterns
  V8_INLINE void RewriteNonPattern(bool* ok);

  V8_INLINE void QueueDestructuringAssignmentForRewriting(
      Expression* assignment);
  V8_INLINE void QueueNonPatternForRewriting(Expression* expr, bool* ok);

  friend class InitializerRewriter;
  void RewriteParameterInitializer(Expression* expr, Scope* scope);

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

  void FinalizeIteratorUse(Scope* use_scope, Variable* completion,
                           Expression* condition, Variable* iter,
                           Block* iterator_use, Block* result,
                           IteratorType type);

  Statement* FinalizeForOfStatement(ForOfStatement* loop, Variable* completion,
                                    IteratorType type, int pos);
  void BuildIteratorClose(ZoneList<Statement*>* statements, Variable* iterator,
                          Variable* input, Variable* output, IteratorType type);
  void BuildIteratorCloseForCompletion(Scope* scope,
                                       ZoneList<Statement*>* statements,
                                       Variable* iterator,
                                       Expression* completion,
                                       IteratorType type);
  Statement* CheckCallable(Variable* var, Expression* error, int pos);

  V8_INLINE Expression* RewriteAwaitExpression(Expression* value, int pos);
  V8_INLINE void PrepareAsyncFunctionBody(ZoneList<Statement*>* body,
                                          FunctionKind kind, int pos);
  V8_INLINE void RewriteAsyncFunctionBody(ZoneList<Statement*>* body,
                                          Block* block,
                                          Expression* return_value, bool* ok);

  Expression* RewriteYieldStar(Expression* generator, Expression* expression,
                               int pos);

  void AddArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                        Expression* params, int end_pos,
                                        bool* ok);
  void SetFunctionName(Expression* value, const AstRawString* name);

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

  V8_INLINE bool IsUndefined(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->undefined_string();
  }

  // Returns true if the expression is of type "this.foo".
  V8_INLINE static bool IsThisProperty(Expression* expression) {
    DCHECK(expression != NULL);
    Property* property = expression->AsProperty();
    return property != NULL && property->obj()->IsVariableProxy() &&
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

  V8_INLINE bool IsPrototype(const AstRawString* identifier) const {
    return identifier == ast_value_factory()->prototype_string();
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
    if (literal == nullptr || !literal->raw_value()->IsString()) return false;
    return arg == nullptr || literal->raw_value()->AsString() == arg;
  }

  V8_INLINE static Expression* GetPropertyValue(LiteralProperty* property) {
    return property->value();
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
    DCHECK(left != NULL);
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

  Expression* BuildIteratorResult(Expression* value, bool done);

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
  V8_INLINE void ReportMessageAt(Scanner::Location source_location,
                                 MessageTemplate::Template message,
                                 const char* arg = NULL,
                                 ParseErrorType error_type = kSyntaxError) {
    if (stack_overflow()) {
      // Suppress the error message (syntax error or such) in the presence of a
      // stack overflow. The isolate allows only one pending exception at at
      // time
      // and we want to report the stack overflow later.
      return;
    }
    pending_error_handler_.ReportMessageAt(source_location.beg_pos,
                                           source_location.end_pos, message,
                                           arg, error_type);
  }

  V8_INLINE void ReportMessageAt(Scanner::Location source_location,
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
    pending_error_handler_.ReportMessageAt(source_location.beg_pos,
                                           source_location.end_pos, message,
                                           arg, error_type);
  }

  // "null" return type creators.
  V8_INLINE static const AstRawString* EmptyIdentifier() { return nullptr; }
  V8_INLINE static bool IsEmptyIdentifier(const AstRawString* name) {
    return name == nullptr;
  }
  V8_INLINE static Expression* EmptyExpression() { return nullptr; }
  V8_INLINE static Literal* EmptyLiteral() { return nullptr; }
  V8_INLINE static ObjectLiteralProperty* EmptyObjectLiteralProperty() {
    return nullptr;
  }
  V8_INLINE static ClassLiteralProperty* EmptyClassLiteralProperty() {
    return nullptr;
  }
  V8_INLINE static FunctionLiteral* EmptyFunctionLiteral() { return nullptr; }
  V8_INLINE static Block* NullBlock() { return nullptr; }

  V8_INLINE static bool IsEmptyExpression(Expression* expr) {
    return expr == nullptr;
  }

  // Used in error return values.
  V8_INLINE static ZoneList<Expression*>* NullExpressionList() {
    return nullptr;
  }
  V8_INLINE static bool IsNullExpressionList(ZoneList<Expression*>* exprs) {
    return exprs == nullptr;
  }
  V8_INLINE static ZoneList<Statement*>* NullStatementList() { return nullptr; }
  V8_INLINE static bool IsNullStatementList(ZoneList<Statement*>* stmts) {
    return stmts == nullptr;
  }
  V8_INLINE static Statement* NullStatement() { return nullptr; }
  V8_INLINE bool IsNullStatement(Statement* stmt) { return stmt == nullptr; }
  V8_INLINE bool IsEmptyStatement(Statement* stmt) {
    DCHECK_NOT_NULL(stmt);
    return stmt->IsEmpty();
  }

  // Non-NULL empty string.
  V8_INLINE const AstRawString* EmptyIdentifierString() const {
    return ast_value_factory()->empty_string();
  }

  // Odd-ball literal creators.
  V8_INLINE Literal* GetLiteralTheHole(int position) {
    return factory()->NewTheHoleLiteral(kNoSourcePosition);
  }

  // Producing data during the recursive descent.
  V8_INLINE const AstRawString* GetSymbol() const {
    const AstRawString* result = scanner()->CurrentSymbol(ast_value_factory());
    DCHECK(result != NULL);
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
  V8_INLINE ZoneList<CaseClause*>* NewCaseClauseList(int size) const {
    return new (zone()) ZoneList<CaseClause*>(size, zone());
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
    if (init_block != nullptr) body->Add(init_block, zone());
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
      const ThreadedList<ParserFormalParameters::Parameter>& parameters) {
    bool is_simple = classifier()->is_simple_parameter_list();
    if (!is_simple) scope->SetHasNonSimpleParameters();
    for (auto parameter : parameters) {
      bool is_duplicate = false;
      bool is_optional = parameter->initializer != nullptr;
      // If the parameter list is simple, declare the parameters normally with
      // their names. If the parameter list is not simple, declare a temporary
      // for each parameter - the corresponding named variable is declared by
      // BuildParamerterInitializationBlock.
      scope->DeclareParameter(
          is_simple ? parameter->name : ast_value_factory()->empty_string(),
          is_simple ? VAR : TEMPORARY, is_optional, parameter->is_rest,
          &is_duplicate, ast_value_factory(), parameter->position);
      if (is_duplicate &&
          classifier()->is_valid_formal_parameter_list_without_duplicates()) {
        classifier()->RecordDuplicateFormalParameterError(
            scanner()->location());
      }
    }
  }

  void DeclareArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                            Expression* params,
                                            const Scanner::Location& params_loc,
                                            Scanner::Location* duplicate_loc,
                                            bool* ok);

  V8_INLINE Expression* NoTemplateTag() { return NULL; }
  V8_INLINE static bool IsTaggedTemplate(const Expression* tag) {
    return tag != NULL;
  }

  Expression* ExpressionListToExpression(ZoneList<Expression*>* args);

  void AddAccessorPrefixToFunctionName(bool is_get, FunctionLiteral* function,
                                       const AstRawString* name);

  void SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                       const AstRawString* name);

  void SetFunctionNameFromIdentifierRef(Expression* value,
                                        Expression* identifier);

  V8_INLINE ZoneList<typename ExpressionClassifier::Error>*
  GetReportedErrorList() const {
    return function_state_->GetReportedErrorList();
  }

  V8_INLINE ZoneList<Expression*>* GetNonPatternList() const {
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

  // Parser's private field members.
  friend class DiscardableZoneScope;  // Uses reusable_preparser_.
  // FIXME(marja): Make reusable_preparser_ always use its own temp Zone (call
  // DeleteAll after each function), so this won't be needed.

  Scanner scanner_;
  PreParser* reusable_preparser_;
  Mode mode_;

  std::vector<FunctionLiteral*> literals_to_stitch_;
  Handle<String> source_;
  CompilerDispatcher* compiler_dispatcher_ = nullptr;
  ParseInfo* main_parse_info_ = nullptr;

  friend class ParserTarget;
  friend class ParserTargetScope;
  ParserTarget* target_stack_;  // for break, continue statements

  ScriptCompiler::CompileOptions compile_options_;
  ParseData* cached_parse_data_;

  PendingCompilationErrorHandler pending_error_handler_;

  // Other information which will be stored in Parser and moved to Isolate after
  // parsing.
  int use_counts_[v8::Isolate::kUseCounterFeatureCount];
  int total_preparse_skipped_;
  bool allow_lazy_;
  bool temp_zoned_;
  ParserLogger* log_;

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
