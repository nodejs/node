// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSER_H_
#define V8_PARSER_H_

#include "src/allocation.h"
#include "src/ast.h"
#include "src/compiler.h"  // TODO(titzer): remove this include dependency
#include "src/pending-compilation-error-handler.h"
#include "src/preparse-data.h"
#include "src/preparse-data-format.h"
#include "src/preparser.h"
#include "src/scopes.h"

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

  FunctionLiteral* function() {  // TODO(titzer): temporary name adapter
    return literal_;
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
// REGEXP PARSING

// A BufferedZoneList is an automatically growing list, just like (and backed
// by) a ZoneList, that is optimized for the case of adding and removing
// a single element. The last element added is stored outside the backing list,
// and if no more than one element is ever added, the ZoneList isn't even
// allocated.
// Elements must not be NULL pointers.
template <typename T, int initial_size>
class BufferedZoneList {
 public:
  BufferedZoneList() : list_(NULL), last_(NULL) {}

  // Adds element at end of list. This element is buffered and can
  // be read using last() or removed using RemoveLast until a new Add or until
  // RemoveLast or GetList has been called.
  void Add(T* value, Zone* zone) {
    if (last_ != NULL) {
      if (list_ == NULL) {
        list_ = new(zone) ZoneList<T*>(initial_size, zone);
      }
      list_->Add(last_, zone);
    }
    last_ = value;
  }

  T* last() {
    DCHECK(last_ != NULL);
    return last_;
  }

  T* RemoveLast() {
    DCHECK(last_ != NULL);
    T* result = last_;
    if ((list_ != NULL) && (list_->length() > 0))
      last_ = list_->RemoveLast();
    else
      last_ = NULL;
    return result;
  }

  T* Get(int i) {
    DCHECK((0 <= i) && (i < length()));
    if (list_ == NULL) {
      DCHECK_EQ(0, i);
      return last_;
    } else {
      if (i == list_->length()) {
        DCHECK(last_ != NULL);
        return last_;
      } else {
        return list_->at(i);
      }
    }
  }

  void Clear() {
    list_ = NULL;
    last_ = NULL;
  }

  int length() {
    int length = (list_ == NULL) ? 0 : list_->length();
    return length + ((last_ == NULL) ? 0 : 1);
  }

  ZoneList<T*>* GetList(Zone* zone) {
    if (list_ == NULL) {
      list_ = new(zone) ZoneList<T*>(initial_size, zone);
    }
    if (last_ != NULL) {
      list_->Add(last_, zone);
      last_ = NULL;
    }
    return list_;
  }

 private:
  ZoneList<T*>* list_;
  T* last_;
};


// Accumulates RegExp atoms and assertions into lists of terms and alternatives.
class RegExpBuilder: public ZoneObject {
 public:
  explicit RegExpBuilder(Zone* zone);
  void AddCharacter(uc16 character);
  // "Adds" an empty expression. Does nothing except consume a
  // following quantifier
  void AddEmpty();
  void AddAtom(RegExpTree* tree);
  void AddAssertion(RegExpTree* tree);
  void NewAlternative();  // '|'
  void AddQuantifierToAtom(
      int min, int max, RegExpQuantifier::QuantifierType type);
  RegExpTree* ToRegExp();

 private:
  void FlushCharacters();
  void FlushText();
  void FlushTerms();
  Zone* zone() const { return zone_; }

  Zone* zone_;
  bool pending_empty_;
  ZoneList<uc16>* characters_;
  BufferedZoneList<RegExpTree, 2> terms_;
  BufferedZoneList<RegExpTree, 2> text_;
  BufferedZoneList<RegExpTree, 2> alternatives_;
#ifdef DEBUG
  enum {ADD_NONE, ADD_CHAR, ADD_TERM, ADD_ASSERT, ADD_ATOM} last_added_;
#define LAST(x) last_added_ = x;
#else
#define LAST(x)
#endif
};


class RegExpParser BASE_EMBEDDED {
 public:
  RegExpParser(FlatStringReader* in, Handle<String>* error, bool multiline_mode,
               bool unicode, Isolate* isolate, Zone* zone);

  static bool ParseRegExp(Isolate* isolate, Zone* zone, FlatStringReader* input,
                          bool multiline, bool unicode,
                          RegExpCompileData* result);

  RegExpTree* ParsePattern();
  RegExpTree* ParseDisjunction();
  RegExpTree* ParseGroup();
  RegExpTree* ParseCharacterClass();

  // Parses a {...,...} quantifier and stores the range in the given
  // out parameters.
  bool ParseIntervalQuantifier(int* min_out, int* max_out);

  // Parses and returns a single escaped character.  The character
  // must not be 'b' or 'B' since they are usually handle specially.
  uc32 ParseClassCharacterEscape();

  // Checks whether the following is a length-digit hexadecimal number,
  // and sets the value if it is.
  bool ParseHexEscape(int length, uc32* value);
  bool ParseUnicodeEscape(uc32* value);
  bool ParseUnlimitedLengthHexNumber(int max_value, uc32* value);

  uc32 ParseOctalLiteral();

  // Tries to parse the input as a back reference.  If successful it
  // stores the result in the output parameter and returns true.  If
  // it fails it will push back the characters read so the same characters
  // can be reparsed.
  bool ParseBackReferenceIndex(int* index_out);

  CharacterRange ParseClassAtom(uc16* char_class);
  RegExpTree* ReportError(Vector<const char> message);
  void Advance();
  void Advance(int dist);
  void Reset(int pos);

  // Reports whether the pattern might be used as a literal search string.
  // Only use if the result of the parse is a single atom node.
  bool simple();
  bool contains_anchor() { return contains_anchor_; }
  void set_contains_anchor() { contains_anchor_ = true; }
  int captures_started() { return captures_ == NULL ? 0 : captures_->length(); }
  int position() { return next_pos_ - 1; }
  bool failed() { return failed_; }

  static bool IsSyntaxCharacter(uc32 c);

  static const int kMaxCaptures = 1 << 16;
  static const uc32 kEndMarker = (1 << 21);

 private:
  enum SubexpressionType {
    INITIAL,
    CAPTURE,  // All positive values represent captures.
    POSITIVE_LOOKAHEAD,
    NEGATIVE_LOOKAHEAD,
    GROUPING
  };

  class RegExpParserState : public ZoneObject {
   public:
    RegExpParserState(RegExpParserState* previous_state,
                      SubexpressionType group_type,
                      int disjunction_capture_index,
                      Zone* zone)
        : previous_state_(previous_state),
          builder_(new(zone) RegExpBuilder(zone)),
          group_type_(group_type),
          disjunction_capture_index_(disjunction_capture_index) {}
    // Parser state of containing expression, if any.
    RegExpParserState* previous_state() { return previous_state_; }
    bool IsSubexpression() { return previous_state_ != NULL; }
    // RegExpBuilder building this regexp's AST.
    RegExpBuilder* builder() { return builder_; }
    // Type of regexp being parsed (parenthesized group or entire regexp).
    SubexpressionType group_type() { return group_type_; }
    // Index in captures array of first capture in this sub-expression, if any.
    // Also the capture index of this sub-expression itself, if group_type
    // is CAPTURE.
    int capture_index() { return disjunction_capture_index_; }

   private:
    // Linked list implementation of stack of states.
    RegExpParserState* previous_state_;
    // Builder for the stored disjunction.
    RegExpBuilder* builder_;
    // Stored disjunction type (capture, look-ahead or grouping), if any.
    SubexpressionType group_type_;
    // Stored disjunction's capture index (if any).
    int disjunction_capture_index_;
  };

  Isolate* isolate() { return isolate_; }
  Zone* zone() const { return zone_; }

  uc32 current() { return current_; }
  bool has_more() { return has_more_; }
  bool has_next() { return next_pos_ < in()->length(); }
  uc32 Next();
  FlatStringReader* in() { return in_; }
  void ScanForCaptures();

  Isolate* isolate_;
  Zone* zone_;
  Handle<String>* error_;
  ZoneList<RegExpCapture*>* captures_;
  FlatStringReader* in_;
  uc32 current_;
  int next_pos_;
  // The capture count is only valid after we have scanned for captures.
  int capture_count_;
  bool has_more_;
  bool multiline_;
  bool unicode_;
  bool simple_;
  bool contains_anchor_;
  bool is_scanned_for_captures_;
  bool failed_;
};

// ----------------------------------------------------------------------------
// JAVASCRIPT PARSING

class Parser;
class SingletonLogger;

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
    typedef const v8::internal::AstRawString* FormalParameter;
    typedef Scope FormalParameterScope;
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

  // Keep track of eval() calls since they disable all local variable
  // optimizations. This checks if expression is an eval call, and if yes,
  // forwards the information to scope.
  void CheckPossibleEvalCall(Expression* expression, Scope* scope);

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
  Expression* NewThrowReferenceError(const char* type, int pos);

  // Generate AST node that throws a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  Expression* NewThrowSyntaxError(
      const char* type, const AstRawString* arg, int pos);

  // Generate AST node that throws a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  Expression* NewThrowTypeError(const char* type, const AstRawString* arg,
                                int pos);

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(
      const AstRawString* constructor, const char* type,
      const AstRawString* arg, int pos);

  // Reporting errors.
  void ReportMessageAt(Scanner::Location source_location, const char* message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);
  void ReportMessage(const char* message, const char* arg = NULL,
                     ParseErrorType error_type = kSyntaxError);
  void ReportMessage(const char* message, const AstRawString* arg,
                     ParseErrorType error_type = kSyntaxError);
  void ReportMessageAt(Scanner::Location source_location, const char* message,
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
  Expression* SuperReference(Scope* scope, AstNodeFactory* factory,
                             int pos = RelocInfo::kNoPosition);
  Expression* DefaultConstructor(bool call_super, Scope* scope, int pos,
                                 int end_pos);
  Literal* ExpressionFromLiteral(Token::Value token, int pos, Scanner* scanner,
                                 AstNodeFactory* factory);
  Expression* ExpressionFromIdentifier(const AstRawString* name,
                                       int start_position, int end_position,
                                       Scope* scope, AstNodeFactory* factory);
  Expression* ExpressionFromString(int pos, Scanner* scanner,
                                   AstNodeFactory* factory);
  Expression* GetIterator(Expression* iterable, AstNodeFactory* factory);
  ZoneList<v8::internal::Expression*>* NewExpressionList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Expression*>(size, zone);
  }
  ZoneList<ObjectLiteral::Property*>* NewPropertyList(int size, Zone* zone) {
    return new(zone) ZoneList<ObjectLiteral::Property*>(size, zone);
  }
  ZoneList<v8::internal::Statement*>* NewStatementList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Statement*>(size, zone);
  }
  V8_INLINE Scope* NewScope(Scope* parent_scope, ScopeType scope_type,
                            FunctionKind kind = kNormalFunction);

  bool DeclareFormalParameter(Scope* scope, const AstRawString* name,
                              bool is_rest) {
    bool is_duplicate = false;
    Variable* var = scope->DeclareParameter(name, VAR, is_rest, &is_duplicate);
    if (is_sloppy(scope->language_mode())) {
      // TODO(sigurds) Mark every parameter as maybe assigned. This is a
      // conservative approximation necessary to account for parameters
      // that are assigned via the arguments array.
      var->set_maybe_assigned();
    }
    return is_duplicate;
  }

  void DeclareArrowFunctionParameters(Scope* scope, Expression* expr,
                                      const Scanner::Location& params_loc,
                                      FormalParameterErrorLocations* error_locs,
                                      bool* ok);
  void ParseArrowFunctionFormalParameters(
      Scope* scope, Expression* params, const Scanner::Location& params_loc,
      FormalParameterErrorLocations* error_locs, bool* is_rest, bool* ok);

  // Temporary glue; these functions will move to ParserBase.
  Expression* ParseV8Intrinsic(bool* ok);
  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      bool name_is_strict_reserved, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction, bool* ok);
  V8_INLINE void SkipLazyFunctionBody(
      int* materialized_literal_count, int* expected_property_count, bool* ok,
      Scanner::BookmarkScope* bookmark = nullptr);
  V8_INLINE ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* name, int pos, Variable* fvar,
      Token::Value fvar_init_op, FunctionKind kind, bool* ok);

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

  bool inside_with() const { return scope_->inside_with(); }
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
  Scope* DeclarationScope(VariableMode mode) {
    return IsLexicalVariableMode(mode)
        ? scope_ : scope_->DeclarationScope();
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
  Block* ParseVariableStatement(VariableDeclarationContext var_context,
                                ZoneList<const AstRawString*>* names,
                                bool* ok);
  Block* ParseVariableDeclarations(VariableDeclarationContext var_context,
                                   int* num_decl,
                                   ZoneList<const AstRawString*>* names,
                                   const AstRawString** out,
                                   Scanner::Location* first_initializer_loc,
                                   Scanner::Location* bindings_loc, bool* ok);
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
  SwitchStatement* ParseSwitchStatement(ZoneList<const AstRawString*>* labels,
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

  // Support for hamony block scoped bindings.
  Block* ParseScopedBlock(ZoneList<const AstRawString*>* labels, bool* ok);

  // Initialize the components of a for-in / for-of statement.
  void InitializeForEachStatement(ForEachStatement* stmt,
                                  Expression* each,
                                  Expression* subject,
                                  Statement* body);
  Statement* DesugarLexicalBindingsInForStatement(
      Scope* inner_scope, bool is_const, ZoneList<const AstRawString*>* names,
      ForStatement* loop, Statement* init, Expression* cond, Statement* next,
      Statement* body, bool* ok);

  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      bool name_is_strict_reserved, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction, bool* ok);


  ClassLiteral* ParseClassLiteral(const AstRawString* name,
                                  Scanner::Location class_name_location,
                                  bool name_is_strict_reserved, int pos,
                                  bool* ok);

  // Magical syntax support.
  Expression* ParseV8Intrinsic(bool* ok);

  // Get odd-ball literals.
  Literal* GetLiteralUndefined(int position);

  // For harmony block scoping mode: Check if the scope has conflicting var/let
  // declarations from different scopes. It covers for example
  //
  // function f() { { { var x; } let x; } }
  // function g() { { var x; let x; } }
  //
  // The var declarations are hoisted to the function scope, but originate from
  // a scope where the name has also been let bound or the var declaration is
  // hoisted over such a scope.
  void CheckConflictingVarDeclarations(Scope* scope, bool* ok);

  // Parser support
  VariableProxy* NewUnresolved(const AstRawString* name, VariableMode mode);
  Variable* Declare(Declaration* declaration, bool resolve, bool* ok);

  bool TargetStackContainsLabel(const AstRawString* label);
  BreakableStatement* LookupBreakTarget(const AstRawString* label, bool* ok);
  IterationStatement* LookupContinueTarget(const AstRawString* label, bool* ok);

  void AddAssertIsConstruct(ZoneList<Statement*>* body, int pos);

  // Factory methods.
  FunctionLiteral* DefaultConstructor(bool call_super, Scope* scope, int pos,
                                      int end_pos);

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

  // Consumes the ending }.
  ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* function_name, int pos, Variable* fvar,
      Token::Value fvar_init_op, FunctionKind kind, bool* ok);

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
    const AstRawString* name, int pos, Variable* fvar,
    Token::Value fvar_init_op, FunctionKind kind, bool* ok) {
  return parser_->ParseEagerFunctionBody(name, pos, fvar, fvar_init_op, kind,
                                         ok);
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
} }  // namespace v8::internal

#endif  // V8_PARSER_H_
