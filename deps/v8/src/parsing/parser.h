// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_H_
#define V8_PARSING_PARSER_H_

#include "src/allocation.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/compiler.h"  // TODO(titzer): remove this include dependency
#include "src/parsing/parser-base.h"
#include "src/parsing/preparse-data.h"
#include "src/parsing/preparse-data-format.h"
#include "src/parsing/preparser.h"
#include "src/pending-compilation-error-handler.h"

namespace v8 {

class ScriptCompiler;

namespace internal {

class Target;

// A container for the inputs, configuration options, and outputs of parsing.
class ParseInfo {
 public:
  explicit ParseInfo(Zone* zone);
  ParseInfo(Zone* zone, Handle<JSFunction> function);
  ParseInfo(Zone* zone, Handle<Script> script);
  // TODO(all) Only used via Debug::FindSharedFunctionInfoInScript, remove?
  ParseInfo(Zone* zone, Handle<SharedFunctionInfo> shared);

  ~ParseInfo() {
    if (ast_value_factory_owned()) {
      delete ast_value_factory_;
      set_ast_value_factory_owned(false);
    }
    ast_value_factory_ = nullptr;
  }

  Zone* zone() { return zone_; }

// Convenience accessor methods for flags.
#define FLAG_ACCESSOR(flag, getter, setter)     \
  bool getter() const { return GetFlag(flag); } \
  void setter() { SetFlag(flag); }              \
  void setter(bool val) { SetFlag(flag, val); }

  FLAG_ACCESSOR(kToplevel, is_toplevel, set_toplevel)
  FLAG_ACCESSOR(kLazy, is_lazy, set_lazy)
  FLAG_ACCESSOR(kEval, is_eval, set_eval)
  FLAG_ACCESSOR(kGlobal, is_global, set_global)
  FLAG_ACCESSOR(kStrictMode, is_strict_mode, set_strict_mode)
  FLAG_ACCESSOR(kStrongMode, is_strong_mode, set_strong_mode)
  FLAG_ACCESSOR(kNative, is_native, set_native)
  FLAG_ACCESSOR(kModule, is_module, set_module)
  FLAG_ACCESSOR(kAllowLazyParsing, allow_lazy_parsing, set_allow_lazy_parsing)
  FLAG_ACCESSOR(kAstValueFactoryOwned, ast_value_factory_owned,
                set_ast_value_factory_owned)

#undef FLAG_ACCESSOR

  void set_parse_restriction(ParseRestriction restriction) {
    SetFlag(kParseRestriction, restriction != NO_PARSE_RESTRICTION);
  }

  ParseRestriction parse_restriction() const {
    return GetFlag(kParseRestriction) ? ONLY_SINGLE_FUNCTION_LITERAL
                                      : NO_PARSE_RESTRICTION;
  }

  ScriptCompiler::ExternalSourceStream* source_stream() {
    return source_stream_;
  }
  void set_source_stream(ScriptCompiler::ExternalSourceStream* source_stream) {
    source_stream_ = source_stream;
  }

  ScriptCompiler::StreamedSource::Encoding source_stream_encoding() {
    return source_stream_encoding_;
  }
  void set_source_stream_encoding(
      ScriptCompiler::StreamedSource::Encoding source_stream_encoding) {
    source_stream_encoding_ = source_stream_encoding;
  }

  v8::Extension* extension() { return extension_; }
  void set_extension(v8::Extension* extension) { extension_ = extension; }

  ScriptData** cached_data() { return cached_data_; }
  void set_cached_data(ScriptData** cached_data) { cached_data_ = cached_data; }

  ScriptCompiler::CompileOptions compile_options() { return compile_options_; }
  void set_compile_options(ScriptCompiler::CompileOptions compile_options) {
    compile_options_ = compile_options;
  }

  Scope* script_scope() { return script_scope_; }
  void set_script_scope(Scope* script_scope) { script_scope_ = script_scope; }

  AstValueFactory* ast_value_factory() { return ast_value_factory_; }
  void set_ast_value_factory(AstValueFactory* ast_value_factory) {
    ast_value_factory_ = ast_value_factory;
  }

  FunctionLiteral* literal() { return literal_; }
  void set_literal(FunctionLiteral* literal) { literal_ = literal; }

  Scope* scope() { return scope_; }
  void set_scope(Scope* scope) { scope_ = scope; }

  UnicodeCache* unicode_cache() { return unicode_cache_; }
  void set_unicode_cache(UnicodeCache* unicode_cache) {
    unicode_cache_ = unicode_cache;
  }

  uintptr_t stack_limit() { return stack_limit_; }
  void set_stack_limit(uintptr_t stack_limit) { stack_limit_ = stack_limit; }

  uint32_t hash_seed() { return hash_seed_; }
  void set_hash_seed(uint32_t hash_seed) { hash_seed_ = hash_seed; }

  //--------------------------------------------------------------------------
  // TODO(titzer): these should not be part of ParseInfo.
  //--------------------------------------------------------------------------
  Isolate* isolate() { return isolate_; }
  Handle<JSFunction> closure() { return closure_; }
  Handle<SharedFunctionInfo> shared_info() { return shared_; }
  Handle<Script> script() { return script_; }
  Handle<Context> context() { return context_; }
  void clear_script() { script_ = Handle<Script>::null(); }
  void set_isolate(Isolate* isolate) { isolate_ = isolate; }
  void set_context(Handle<Context> context) { context_ = context; }
  void set_script(Handle<Script> script) { script_ = script; }
  //--------------------------------------------------------------------------

  LanguageMode language_mode() {
    return construct_language_mode(is_strict_mode(), is_strong_mode());
  }
  void set_language_mode(LanguageMode language_mode) {
    STATIC_ASSERT(LANGUAGE_END == 3);
    set_strict_mode(language_mode & STRICT_BIT);
    set_strong_mode(language_mode & STRONG_BIT);
  }

  void ReopenHandlesInNewHandleScope() {
    closure_ = Handle<JSFunction>(*closure_);
    shared_ = Handle<SharedFunctionInfo>(*shared_);
    script_ = Handle<Script>(*script_);
    context_ = Handle<Context>(*context_);
  }

#ifdef DEBUG
  bool script_is_native() { return script_->type() == Script::TYPE_NATIVE; }
#endif  // DEBUG

 private:
  // Various configuration flags for parsing.
  enum Flag {
    // ---------- Input flags ---------------------------
    kToplevel = 1 << 0,
    kLazy = 1 << 1,
    kEval = 1 << 2,
    kGlobal = 1 << 3,
    kStrictMode = 1 << 4,
    kStrongMode = 1 << 5,
    kNative = 1 << 6,
    kParseRestriction = 1 << 7,
    kModule = 1 << 8,
    kAllowLazyParsing = 1 << 9,
    // ---------- Output flags --------------------------
    kAstValueFactoryOwned = 1 << 10
  };

  //------------- Inputs to parsing and scope analysis -----------------------
  Zone* zone_;
  unsigned flags_;
  ScriptCompiler::ExternalSourceStream* source_stream_;
  ScriptCompiler::StreamedSource::Encoding source_stream_encoding_;
  v8::Extension* extension_;
  ScriptCompiler::CompileOptions compile_options_;
  Scope* script_scope_;
  UnicodeCache* unicode_cache_;
  uintptr_t stack_limit_;
  uint32_t hash_seed_;

  // TODO(titzer): Move handles and isolate out of ParseInfo.
  Isolate* isolate_;
  Handle<JSFunction> closure_;
  Handle<SharedFunctionInfo> shared_;
  Handle<Script> script_;
  Handle<Context> context_;

  //----------- Inputs+Outputs of parsing and scope analysis -----------------
  ScriptData** cached_data_;  // used if available, populated if requested.
  AstValueFactory* ast_value_factory_;  // used if available, otherwise new.

  //----------- Outputs of parsing and scope analysis ------------------------
  FunctionLiteral* literal_;  // produced by full parser.
  Scope* scope_;              // produced by scope analysis.

  void SetFlag(Flag f) { flags_ |= f; }
  void SetFlag(Flag f, bool v) { flags_ = v ? flags_ | f : flags_ & ~f; }
  bool GetFlag(Flag f) const { return (flags_ & f) != 0; }

  void set_shared_info(Handle<SharedFunctionInfo> shared) { shared_ = shared; }
  void set_closure(Handle<JSFunction> closure) { closure_ = closure; }
};

class FunctionEntry BASE_EMBEDDED {
 public:
  enum {
    kStartPositionIndex,
    kEndPositionIndex,
    kLiteralCountIndex,
    kPropertyCountIndex,
    kLanguageModeIndex,
    kUsesSuperPropertyIndex,
    kCallsEvalIndex,
    kSize
  };

  explicit FunctionEntry(Vector<unsigned> backing)
    : backing_(backing) { }

  FunctionEntry() : backing_() { }

  int start_pos() { return backing_[kStartPositionIndex]; }
  int end_pos() { return backing_[kEndPositionIndex]; }
  int literal_count() { return backing_[kLiteralCountIndex]; }
  int property_count() { return backing_[kPropertyCountIndex]; }
  LanguageMode language_mode() {
    DCHECK(is_valid_language_mode(backing_[kLanguageModeIndex]));
    return static_cast<LanguageMode>(backing_[kLanguageModeIndex]);
  }
  bool uses_super_property() { return backing_[kUsesSuperPropertyIndex]; }
  bool calls_eval() { return backing_[kCallsEvalIndex]; }

  bool is_valid() { return !backing_.is_empty(); }

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

  bool HasError();

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
class SingletonLogger;


struct ParserFormalParameters : FormalParametersBase {
  struct Parameter {
    Parameter(const AstRawString* name, Expression* pattern,
              Expression* initializer, int initializer_end_position,
              bool is_rest)
        : name(name),
          pattern(pattern),
          initializer(initializer),
          initializer_end_position(initializer_end_position),
          is_rest(is_rest) {}
    const AstRawString* name;
    Expression* pattern;
    Expression* initializer;
    int initializer_end_position;
    bool is_rest;
    bool is_simple() const {
      return pattern->IsVariableProxy() && initializer == nullptr && !is_rest;
    }
  };

  explicit ParserFormalParameters(Scope* scope)
      : FormalParametersBase(scope), params(4, scope->zone()) {}
  ZoneList<Parameter> params;

  int Arity() const { return params.length(); }
  const Parameter& at(int i) const { return params[i]; }
};


class ParserTraits {
 public:
  struct Type {
    // TODO(marja): To be removed. The Traits object should contain all the data
    // it needs.
    typedef v8::internal::Parser* Parser;

    typedef Variable GeneratorVariable;

    typedef v8::internal::AstProperties AstProperties;

    // Return types for traversing functions.
    typedef const AstRawString* Identifier;
    typedef v8::internal::Expression* Expression;
    typedef Yield* YieldExpression;
    typedef v8::internal::FunctionLiteral* FunctionLiteral;
    typedef v8::internal::ClassLiteral* ClassLiteral;
    typedef v8::internal::Literal* Literal;
    typedef ObjectLiteral::Property* ObjectLiteralProperty;
    typedef ZoneList<v8::internal::Expression*>* ExpressionList;
    typedef ZoneList<ObjectLiteral::Property*>* PropertyList;
    typedef ParserFormalParameters::Parameter FormalParameter;
    typedef ParserFormalParameters FormalParameters;
    typedef ZoneList<v8::internal::Statement*>* StatementList;

    // For constructing objects returned by the traversing functions.
    typedef AstNodeFactory Factory;
  };

  explicit ParserTraits(Parser* parser) : parser_(parser) {}

  // Helper functions for recursive descent.
  bool IsEval(const AstRawString* identifier) const;
  bool IsArguments(const AstRawString* identifier) const;
  bool IsEvalOrArguments(const AstRawString* identifier) const;
  bool IsUndefined(const AstRawString* identifier) const;
  V8_INLINE bool IsFutureStrictReserved(const AstRawString* identifier) const;

  // Returns true if the expression is of type "this.foo".
  static bool IsThisProperty(Expression* expression);

  static bool IsIdentifier(Expression* expression);

  bool IsPrototype(const AstRawString* identifier) const;

  bool IsConstructor(const AstRawString* identifier) const;

  static const AstRawString* AsIdentifier(Expression* expression) {
    DCHECK(IsIdentifier(expression));
    return expression->AsVariableProxy()->raw_name();
  }

  static bool IsBoilerplateProperty(ObjectLiteral::Property* property) {
    return ObjectLiteral::IsBoilerplateProperty(property);
  }

  static bool IsArrayIndex(const AstRawString* string, uint32_t* index) {
    return string->AsArrayIndex(index);
  }

  static Expression* GetPropertyValue(ObjectLiteral::Property* property) {
    return property->value();
  }

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  static void PushLiteralName(FuncNameInferrer* fni, const AstRawString* id) {
    fni->PushLiteralName(id);
  }

  void PushPropertyName(FuncNameInferrer* fni, Expression* expression);

  static void InferFunctionName(FuncNameInferrer* fni,
                                FunctionLiteral* func_to_infer) {
    fni->AddFunction(func_to_infer);
  }

  static void CheckFunctionLiteralInsideTopLevelObjectLiteral(
      Scope* scope, ObjectLiteralProperty* property, bool* has_function) {
    Expression* value = property->value();
    if (scope->DeclarationScope()->is_script_scope() &&
        value->AsFunctionLiteral() != NULL) {
      *has_function = true;
      value->AsFunctionLiteral()->set_pretenure();
    }
  }

  // If we assign a function literal to a property we pretenure the
  // literal so it can be added as a constant function property.
  static void CheckAssigningFunctionLiteralToProperty(Expression* left,
                                                      Expression* right);

  // Determine if the expression is a variable proxy and mark it as being used
  // in an assignment or with a increment/decrement operator.
  static Expression* MarkExpressionAsAssigned(Expression* expression);

  // Returns true if we have a binary expression between two numeric
  // literals. In that case, *x will be changed to an expression which is the
  // computed value.
  bool ShortcutNumericLiteralBinaryExpression(Expression** x, Expression* y,
                                              Token::Value op, int pos,
                                              AstNodeFactory* factory);

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
                                   int pos, AstNodeFactory* factory);

  // Generate AST node that throws a ReferenceError with the given type.
  Expression* NewThrowReferenceError(MessageTemplate::Template message,
                                     int pos);

  // Generate AST node that throws a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  Expression* NewThrowSyntaxError(MessageTemplate::Template message,
                                  const AstRawString* arg, int pos);

  // Generate AST node that throws a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  Expression* NewThrowTypeError(MessageTemplate::Template message,
                                const AstRawString* arg, int pos);

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(Runtime::FunctionId function_id,
                            MessageTemplate::Template message,
                            const AstRawString* arg, int pos);

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);
  void ReportMessage(MessageTemplate::Template message, const char* arg = NULL,
                     ParseErrorType error_type = kSyntaxError);
  void ReportMessage(MessageTemplate::Template message, const AstRawString* arg,
                     ParseErrorType error_type = kSyntaxError);
  void ReportMessageAt(Scanner::Location source_location,
                       MessageTemplate::Template message,
                       const AstRawString* arg,
                       ParseErrorType error_type = kSyntaxError);

  // "null" return type creators.
  static const AstRawString* EmptyIdentifier() {
    return NULL;
  }
  static Expression* EmptyExpression() {
    return NULL;
  }
  static Literal* EmptyLiteral() {
    return NULL;
  }
  static ObjectLiteralProperty* EmptyObjectLiteralProperty() { return NULL; }
  static FunctionLiteral* EmptyFunctionLiteral() { return NULL; }

  // Used in error return values.
  static ZoneList<Expression*>* NullExpressionList() {
    return NULL;
  }
  static const AstRawString* EmptyFormalParameter() { return NULL; }

  // Non-NULL empty string.
  V8_INLINE const AstRawString* EmptyIdentifierString();

  // Odd-ball literal creators.
  Literal* GetLiteralTheHole(int position, AstNodeFactory* factory);

  // Producing data during the recursive descent.
  const AstRawString* GetSymbol(Scanner* scanner);
  const AstRawString* GetNextSymbol(Scanner* scanner);
  const AstRawString* GetNumberAsSymbol(Scanner* scanner);

  Expression* ThisExpression(Scope* scope, AstNodeFactory* factory,
                             int pos = RelocInfo::kNoPosition);
  Expression* SuperPropertyReference(Scope* scope, AstNodeFactory* factory,
                                     int pos);
  Expression* SuperCallReference(Scope* scope, AstNodeFactory* factory,
                                 int pos);
  Expression* NewTargetExpression(Scope* scope, AstNodeFactory* factory,
                                  int pos);
  Expression* DefaultConstructor(bool call_super, Scope* scope, int pos,
                                 int end_pos, LanguageMode language_mode);
  Literal* ExpressionFromLiteral(Token::Value token, int pos, Scanner* scanner,
                                 AstNodeFactory* factory);
  Expression* ExpressionFromIdentifier(const AstRawString* name,
                                       int start_position, int end_position,
                                       Scope* scope, AstNodeFactory* factory);
  Expression* ExpressionFromString(int pos, Scanner* scanner,
                                   AstNodeFactory* factory);
  Expression* GetIterator(Expression* iterable, AstNodeFactory* factory,
                          int pos);
  ZoneList<v8::internal::Expression*>* NewExpressionList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Expression*>(size, zone);
  }
  ZoneList<ObjectLiteral::Property*>* NewPropertyList(int size, Zone* zone) {
    return new(zone) ZoneList<ObjectLiteral::Property*>(size, zone);
  }
  ZoneList<v8::internal::Statement*>* NewStatementList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Statement*>(size, zone);
  }

  V8_INLINE void AddParameterInitializationBlock(
      const ParserFormalParameters& parameters,
      ZoneList<v8::internal::Statement*>* body, bool* ok);

  V8_INLINE Scope* NewScope(Scope* parent_scope, ScopeType scope_type,
                            FunctionKind kind = kNormalFunction);

  V8_INLINE void AddFormalParameter(ParserFormalParameters* parameters,
                                    Expression* pattern,
                                    Expression* initializer,
                                    int initializer_end_position, bool is_rest);
  V8_INLINE void DeclareFormalParameter(
      Scope* scope, const ParserFormalParameters::Parameter& parameter,
      ExpressionClassifier* classifier);
  void ParseArrowFunctionFormalParameters(ParserFormalParameters* parameters,
                                          Expression* params,
                                          const Scanner::Location& params_loc,
                                          bool* ok);
  void ParseArrowFunctionFormalParameterList(
      ParserFormalParameters* parameters, Expression* params,
      const Scanner::Location& params_loc,
      Scanner::Location* duplicate_loc, bool* ok);

  V8_INLINE DoExpression* ParseDoExpression(bool* ok);

  void ReindexLiterals(const ParserFormalParameters& parameters);

  // Temporary glue; these functions will move to ParserBase.
  Expression* ParseV8Intrinsic(bool* ok);
  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction,
      LanguageMode language_mode, bool* ok);
  V8_INLINE void SkipLazyFunctionBody(
      int* materialized_literal_count, int* expected_property_count, bool* ok,
      Scanner::BookmarkScope* bookmark = nullptr);
  V8_INLINE ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* name, int pos,
      const ParserFormalParameters& parameters, FunctionKind kind,
      FunctionLiteral::FunctionType function_type, bool* ok);

  ClassLiteral* ParseClassLiteral(const AstRawString* name,
                                  Scanner::Location class_name_location,
                                  bool name_is_strict_reserved, int pos,
                                  bool* ok);

  V8_INLINE void CheckConflictingVarDeclarations(v8::internal::Scope* scope,
                                                 bool* ok);

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

  V8_INLINE TemplateLiteralState OpenTemplateLiteral(int pos);
  V8_INLINE void AddTemplateSpan(TemplateLiteralState* state, bool tail);
  V8_INLINE void AddTemplateExpression(TemplateLiteralState* state,
                                       Expression* expression);
  V8_INLINE Expression* CloseTemplateLiteral(TemplateLiteralState* state,
                                             int start, Expression* tag);
  V8_INLINE Expression* NoTemplateTag() { return NULL; }
  V8_INLINE static bool IsTaggedTemplate(const Expression* tag) {
    return tag != NULL;
  }

  V8_INLINE ZoneList<v8::internal::Expression*>* PrepareSpreadArguments(
      ZoneList<v8::internal::Expression*>* list);
  V8_INLINE void MaterializeUnspreadArgumentsLiterals(int count) {}
  V8_INLINE Expression* SpreadCall(Expression* function,
                                   ZoneList<v8::internal::Expression*>* args,
                                   int pos);
  V8_INLINE Expression* SpreadCallNew(Expression* function,
                                      ZoneList<v8::internal::Expression*>* args,
                                      int pos);

  // Rewrite all DestructuringAssignments in the current FunctionState.
  V8_INLINE void RewriteDestructuringAssignments();

  V8_INLINE void QueueDestructuringAssignmentForRewriting(
      Expression* assignment);

  void SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                       const AstRawString* name);

  void SetFunctionNameFromIdentifierRef(Expression* value,
                                        Expression* identifier);

  // Rewrite expressions that are not used as patterns
  V8_INLINE Expression* RewriteNonPattern(
      Expression* expr, const ExpressionClassifier* classifier, bool* ok);
  V8_INLINE ZoneList<Expression*>* RewriteNonPatternArguments(
      ZoneList<Expression*>* args, const ExpressionClassifier* classifier,
      bool* ok);
  V8_INLINE ObjectLiteralProperty* RewriteNonPatternObjectLiteralProperty(
      ObjectLiteralProperty* property, const ExpressionClassifier* classifier,
      bool* ok);

 private:
  Parser* parser_;
};


class Parser : public ParserBase<ParserTraits> {
 public:
  explicit Parser(ParseInfo* info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = NULL;
    delete cached_parse_data_;
    cached_parse_data_ = NULL;
  }

  // Parses the source code represented by the compilation info and sets its
  // function literal.  Returns false (and deallocates any allocated AST
  // nodes) if parsing failed.
  static bool ParseStatic(ParseInfo* info);
  bool Parse(ParseInfo* info);
  void ParseOnBackground(ParseInfo* info);

  // Handle errors detected during parsing, move statistics to Isolate,
  // internalize strings (move them to the heap).
  void Internalize(Isolate* isolate, Handle<Script> script, bool error);
  void HandleSourceURLComments(Isolate* isolate, Handle<Script> script);

 private:
  friend class ParserTraits;

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

  FunctionLiteral* ParseLazy(Isolate* isolate, ParseInfo* info);
  FunctionLiteral* ParseLazy(Isolate* isolate, ParseInfo* info,
                             Utf16CharacterStream* source);

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(ParseInfo* info);

  void SetCachedData(ParseInfo* info);

  ScriptCompiler::CompileOptions compile_options() const {
    return compile_options_;
  }
  bool consume_cached_parse_data() const {
    return compile_options_ == ScriptCompiler::kConsumeParserCache &&
           cached_parse_data_ != NULL;
  }
  bool produce_cached_parse_data() const {
    return compile_options_ == ScriptCompiler::kProduceParserCache;
  }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void* ParseStatementList(ZoneList<Statement*>* body, int end_token, bool* ok);
  Statement* ParseStatementListItem(bool* ok);
  void* ParseModuleItemList(ZoneList<Statement*>* body, bool* ok);
  Statement* ParseModuleItem(bool* ok);
  const AstRawString* ParseModuleSpecifier(bool* ok);
  Statement* ParseImportDeclaration(bool* ok);
  Statement* ParseExportDeclaration(bool* ok);
  Statement* ParseExportDefault(bool* ok);
  void* ParseExportClause(ZoneList<const AstRawString*>* export_names,
                          ZoneList<Scanner::Location>* export_locations,
                          ZoneList<const AstRawString*>* local_names,
                          Scanner::Location* reserved_loc, bool* ok);
  ZoneList<ImportDeclaration*>* ParseNamedImports(int pos, bool* ok);
  Statement* ParseStatement(ZoneList<const AstRawString*>* labels, bool* ok);
  Statement* ParseSubStatement(ZoneList<const AstRawString*>* labels, bool* ok);
  Statement* ParseStatementAsUnlabelled(ZoneList<const AstRawString*>* labels,
                                   bool* ok);
  Statement* ParseFunctionDeclaration(ZoneList<const AstRawString*>* names,
                                      bool* ok);
  Statement* ParseClassDeclaration(ZoneList<const AstRawString*>* names,
                                   bool* ok);
  Statement* ParseNativeDeclaration(bool* ok);
  Block* ParseBlock(ZoneList<const AstRawString*>* labels, bool* ok);
  Block* ParseBlock(ZoneList<const AstRawString*>* labels,
                    bool finalize_block_scope, bool* ok);
  Block* ParseVariableStatement(VariableDeclarationContext var_context,
                                ZoneList<const AstRawString*>* names,
                                bool* ok);
  DoExpression* ParseDoExpression(bool* ok);

  struct DeclarationDescriptor {
    enum Kind { NORMAL, PARAMETER };
    Parser* parser;
    Scope* scope;
    Scope* hoist_scope;
    VariableMode mode;
    bool needs_init;
    int declaration_pos;
    int initialization_pos;
    Kind declaration_kind;
  };

  struct DeclarationParsingResult {
    struct Declaration {
      Declaration(Expression* pattern, int initializer_position,
                  Expression* initializer)
          : pattern(pattern),
            initializer_position(initializer_position),
            initializer(initializer) {}

      Expression* pattern;
      int initializer_position;
      Expression* initializer;
    };

    DeclarationParsingResult()
        : declarations(4),
          first_initializer_loc(Scanner::Location::invalid()),
          bindings_loc(Scanner::Location::invalid()) {}

    Block* BuildInitializationBlock(ZoneList<const AstRawString*>* names,
                                    bool* ok);

    DeclarationDescriptor descriptor;
    List<Declaration> declarations;
    Scanner::Location first_initializer_loc;
    Scanner::Location bindings_loc;
  };

  class PatternRewriter : private AstVisitor {
   public:
    static void DeclareAndInitializeVariables(
        Block* block, const DeclarationDescriptor* declaration_descriptor,
        const DeclarationParsingResult::Declaration* declaration,
        ZoneList<const AstRawString*>* names, bool* ok);

    static void RewriteDestructuringAssignment(
        Parser* parser, RewritableAssignmentExpression* expr, Scope* Scope);

    static Expression* RewriteDestructuringAssignment(Parser* parser,
                                                      Assignment* assignment,
                                                      Scope* scope);

    void set_initializer_position(int pos) { initializer_position_ = pos; }

   private:
    PatternRewriter() {}

#define DECLARE_VISIT(type) void Visit##type(v8::internal::type* node) override;
    // Visiting functions for AST nodes make this an AstVisitor.
    AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT
    void Visit(AstNode* node) override;

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
      pattern->Accept(this);
      recursion_level_--;
      current_value_ = old_value;
    }

    void VisitObjectLiteral(ObjectLiteral* node, Variable** temp_var);
    void VisitArrayLiteral(ArrayLiteral* node, Variable** temp_var);

    bool IsBindingContext() const { return IsBindingContext(context_); }
    bool IsInitializerContext() const { return context_ != ASSIGNMENT; }
    bool IsAssignmentContext() const { return IsAssignmentContext(context_); }
    bool IsAssignmentContext(PatternContext c) const;
    bool IsBindingContext(PatternContext c) const;
    bool IsSubPattern() const { return recursion_level_ > 1; }
    PatternContext SetAssignmentContextIfNeeded(Expression* node);
    PatternContext SetInitializerContextIfNeeded(Expression* node);

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
  };


  void ParseVariableDeclarations(VariableDeclarationContext var_context,
                                 DeclarationParsingResult* parsing_result,
                                 bool* ok);
  Statement* ParseExpressionOrLabelledStatement(
      ZoneList<const AstRawString*>* labels, bool* ok);
  IfStatement* ParseIfStatement(ZoneList<const AstRawString*>* labels,
                                bool* ok);
  Statement* ParseContinueStatement(bool* ok);
  Statement* ParseBreakStatement(ZoneList<const AstRawString*>* labels,
                                 bool* ok);
  Statement* ParseReturnStatement(bool* ok);
  Statement* ParseWithStatement(ZoneList<const AstRawString*>* labels,
                                bool* ok);
  CaseClause* ParseCaseClause(bool* default_seen_ptr, bool* ok);
  Statement* ParseSwitchStatement(ZoneList<const AstRawString*>* labels,
                                  bool* ok);
  DoWhileStatement* ParseDoWhileStatement(ZoneList<const AstRawString*>* labels,
                                          bool* ok);
  WhileStatement* ParseWhileStatement(ZoneList<const AstRawString*>* labels,
                                      bool* ok);
  Statement* ParseForStatement(ZoneList<const AstRawString*>* labels, bool* ok);
  Statement* ParseThrowStatement(bool* ok);
  Expression* MakeCatchContext(Handle<String> id, VariableProxy* value);
  TryStatement* ParseTryStatement(bool* ok);
  DebuggerStatement* ParseDebuggerStatement(bool* ok);

  // !%_IsJSReceiver(result = iterator.next()) &&
  //     %ThrowIteratorResultNotAnObject(result)
  Expression* BuildIteratorNextResult(Expression* iterator, Variable* result,
                                      int pos);


  // Initialize the components of a for-in / for-of statement.
  void InitializeForEachStatement(ForEachStatement* stmt, Expression* each,
                                  Expression* subject, Statement* body,
                                  bool is_destructuring);
  Statement* DesugarLexicalBindingsInForStatement(
      Scope* inner_scope, bool is_const, ZoneList<const AstRawString*>* names,
      ForStatement* loop, Statement* init, Expression* cond, Statement* next,
      Statement* body, bool* ok);

  void RewriteDoExpression(Expression* expr, bool* ok);

  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      FunctionNameValidity function_name_validity, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction,
      LanguageMode language_mode, bool* ok);


  ClassLiteral* ParseClassLiteral(const AstRawString* name,
                                  Scanner::Location class_name_location,
                                  bool name_is_strict_reserved, int pos,
                                  bool* ok);

  // Magical syntax support.
  Expression* ParseV8Intrinsic(bool* ok);

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
  void InsertSloppyBlockFunctionVarBindings(Scope* scope, bool* ok);

  // Parser support
  VariableProxy* NewUnresolved(const AstRawString* name, VariableMode mode);
  Variable* Declare(Declaration* declaration,
                    DeclarationDescriptor::Kind declaration_kind, bool resolve,
                    bool* ok, Scope* declaration_scope = nullptr);

  bool TargetStackContainsLabel(const AstRawString* label);
  BreakableStatement* LookupBreakTarget(const AstRawString* label, bool* ok);
  IterationStatement* LookupContinueTarget(const AstRawString* label, bool* ok);

  Statement* BuildAssertIsCoercible(Variable* var);

  // Factory methods.
  FunctionLiteral* DefaultConstructor(bool call_super, Scope* scope, int pos,
                                      int end_pos, LanguageMode language_mode);

  // Skip over a lazy function, either using cached data if we have it, or
  // by parsing the function with PreParser. Consumes the ending }.
  //
  // If bookmark is set, the (pre-)parser may decide to abort skipping
  // in order to force the function to be eagerly parsed, after all.
  // In this case, it'll reset the scanner using the bookmark.
  void SkipLazyFunctionBody(int* materialized_literal_count,
                            int* expected_property_count, bool* ok,
                            Scanner::BookmarkScope* bookmark = nullptr);

  PreParser::PreParseResult ParseLazyFunctionBodyWithPreParser(
      SingletonLogger* logger, Scanner::BookmarkScope* bookmark = nullptr);

  Block* BuildParameterInitializationBlock(
      const ParserFormalParameters& parameters, bool* ok);

  // Consumes the ending }.
  ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* function_name, int pos,
      const ParserFormalParameters& parameters, FunctionKind kind,
      FunctionLiteral::FunctionType function_type, bool* ok);

  void ThrowPendingError(Isolate* isolate, Handle<Script> script);

  TemplateLiteralState OpenTemplateLiteral(int pos);
  void AddTemplateSpan(TemplateLiteralState* state, bool tail);
  void AddTemplateExpression(TemplateLiteralState* state,
                             Expression* expression);
  Expression* CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                   Expression* tag);
  uint32_t ComputeTemplateLiteralHash(const TemplateLiteral* lit);

  ZoneList<v8::internal::Expression*>* PrepareSpreadArguments(
      ZoneList<v8::internal::Expression*>* list);
  Expression* SpreadCall(Expression* function,
                         ZoneList<v8::internal::Expression*>* args, int pos);
  Expression* SpreadCallNew(Expression* function,
                            ZoneList<v8::internal::Expression*>* args, int pos);

  void SetLanguageMode(Scope* scope, LanguageMode mode);
  void RaiseLanguageMode(LanguageMode mode);

  V8_INLINE void RewriteDestructuringAssignments();

  V8_INLINE Expression* RewriteNonPattern(
      Expression* expr, const ExpressionClassifier* classifier, bool* ok);
  V8_INLINE ZoneList<Expression*>* RewriteNonPatternArguments(
      ZoneList<Expression*>* args, const ExpressionClassifier* classifier,
      bool* ok);
  V8_INLINE ObjectLiteralProperty* RewriteNonPatternObjectLiteralProperty(
      ObjectLiteralProperty* property, const ExpressionClassifier* classifier,
      bool* ok);

  friend class InitializerRewriter;
  void RewriteParameterInitializer(Expression* expr, Scope* scope);

  Scanner scanner_;
  PreParser* reusable_preparser_;
  Scope* original_scope_;  // for ES5 function declarations in sloppy eval
  Target* target_stack_;  // for break, continue statements
  ScriptCompiler::CompileOptions compile_options_;
  ParseData* cached_parse_data_;

  PendingCompilationErrorHandler pending_error_handler_;

  // Other information which will be stored in Parser and moved to Isolate after
  // parsing.
  int use_counts_[v8::Isolate::kUseCounterFeatureCount];
  int total_preparse_skipped_;
  HistogramTimer* pre_parse_timer_;

  bool parsing_on_main_thread_;
};


bool ParserTraits::IsFutureStrictReserved(
    const AstRawString* identifier) const {
  return parser_->scanner()->IdentifierIsFutureStrictReserved(identifier);
}


Scope* ParserTraits::NewScope(Scope* parent_scope, ScopeType scope_type,
                              FunctionKind kind) {
  return parser_->NewScope(parent_scope, scope_type, kind);
}


const AstRawString* ParserTraits::EmptyIdentifierString() {
  return parser_->ast_value_factory()->empty_string();
}


void ParserTraits::SkipLazyFunctionBody(int* materialized_literal_count,
                                        int* expected_property_count, bool* ok,
                                        Scanner::BookmarkScope* bookmark) {
  return parser_->SkipLazyFunctionBody(materialized_literal_count,
                                       expected_property_count, ok, bookmark);
}


ZoneList<Statement*>* ParserTraits::ParseEagerFunctionBody(
    const AstRawString* name, int pos, const ParserFormalParameters& parameters,
    FunctionKind kind, FunctionLiteral::FunctionType function_type, bool* ok) {
  return parser_->ParseEagerFunctionBody(name, pos, parameters, kind,
                                         function_type, ok);
}


void ParserTraits::CheckConflictingVarDeclarations(v8::internal::Scope* scope,
                                                   bool* ok) {
  parser_->CheckConflictingVarDeclarations(scope, ok);
}


// Support for handling complex values (array and object literals) that
// can be fully handled at compile time.
class CompileTimeValue: public AllStatic {
 public:
  enum LiteralType {
    OBJECT_LITERAL_FAST_ELEMENTS,
    OBJECT_LITERAL_SLOW_ELEMENTS,
    ARRAY_LITERAL
  };

  static bool IsCompileTimeValue(Expression* expression);

  // Get the value as a compile time value.
  static Handle<FixedArray> GetValue(Isolate* isolate, Expression* expression);

  // Get the type of a compile time value returned by GetValue().
  static LiteralType GetLiteralType(Handle<FixedArray> value);

  // Get the elements array of a compile time value returned by GetValue().
  static Handle<FixedArray> GetElements(Handle<FixedArray> value);

 private:
  static const int kLiteralTypeSlot = 0;
  static const int kElementsSlot = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompileTimeValue);
};


ParserTraits::TemplateLiteralState ParserTraits::OpenTemplateLiteral(int pos) {
  return parser_->OpenTemplateLiteral(pos);
}


void ParserTraits::AddTemplateSpan(TemplateLiteralState* state, bool tail) {
  parser_->AddTemplateSpan(state, tail);
}


void ParserTraits::AddTemplateExpression(TemplateLiteralState* state,
                                         Expression* expression) {
  parser_->AddTemplateExpression(state, expression);
}


Expression* ParserTraits::CloseTemplateLiteral(TemplateLiteralState* state,
                                               int start, Expression* tag) {
  return parser_->CloseTemplateLiteral(state, start, tag);
}


ZoneList<v8::internal::Expression*>* ParserTraits::PrepareSpreadArguments(
    ZoneList<v8::internal::Expression*>* list) {
  return parser_->PrepareSpreadArguments(list);
}


Expression* ParserTraits::SpreadCall(Expression* function,
                                     ZoneList<v8::internal::Expression*>* args,
                                     int pos) {
  return parser_->SpreadCall(function, args, pos);
}


Expression* ParserTraits::SpreadCallNew(
    Expression* function, ZoneList<v8::internal::Expression*>* args, int pos) {
  return parser_->SpreadCallNew(function, args, pos);
}


void ParserTraits::AddFormalParameter(ParserFormalParameters* parameters,
                                      Expression* pattern,
                                      Expression* initializer,
                                      int initializer_end_position,
                                      bool is_rest) {
  bool is_simple = pattern->IsVariableProxy() && initializer == nullptr;
  const AstRawString* name = is_simple
                                 ? pattern->AsVariableProxy()->raw_name()
                                 : parser_->ast_value_factory()->empty_string();
  parameters->params.Add(
      ParserFormalParameters::Parameter(name, pattern, initializer,
                                        initializer_end_position, is_rest),
      parameters->scope->zone());
}


void ParserTraits::DeclareFormalParameter(
    Scope* scope, const ParserFormalParameters::Parameter& parameter,
    ExpressionClassifier* classifier) {
  bool is_duplicate = false;
  bool is_simple = classifier->is_simple_parameter_list();
  auto name = is_simple || parameter.is_rest
                  ? parameter.name
                  : parser_->ast_value_factory()->empty_string();
  auto mode = is_simple || parameter.is_rest ? VAR : TEMPORARY;
  if (!is_simple) scope->SetHasNonSimpleParameters();
  bool is_optional = parameter.initializer != nullptr;
  Variable* var = scope->DeclareParameter(
      name, mode, is_optional, parameter.is_rest, &is_duplicate);
  if (is_duplicate) {
    classifier->RecordDuplicateFormalParameterError(
        parser_->scanner()->location());
  }
  if (is_sloppy(scope->language_mode())) {
    // TODO(sigurds) Mark every parameter as maybe assigned. This is a
    // conservative approximation necessary to account for parameters
    // that are assigned via the arguments array.
    var->set_maybe_assigned();
  }
}


void ParserTraits::AddParameterInitializationBlock(
    const ParserFormalParameters& parameters,
    ZoneList<v8::internal::Statement*>* body, bool* ok) {
  if (!parameters.is_simple) {
    auto* init_block =
        parser_->BuildParameterInitializationBlock(parameters, ok);
    if (!*ok) return;
    if (init_block != nullptr) {
      body->Add(init_block, parser_->zone());
    }
  }
}


DoExpression* ParserTraits::ParseDoExpression(bool* ok) {
  return parser_->ParseDoExpression(ok);
}


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSER_H_
