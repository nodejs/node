// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSER_H_
#define V8_PARSER_H_

#include "src/allocation.h"
#include "src/ast.h"
#include "src/compiler.h"  // For CachedDataMode
#include "src/preparse-data.h"
#include "src/preparse-data-format.h"
#include "src/preparser.h"
#include "src/scopes.h"

namespace v8 {
class ScriptCompiler;

namespace internal {

class CompilationInfo;
class ParserLog;
class PositionStack;
class Target;

template <typename T> class ZoneListWrapper;


class FunctionEntry BASE_EMBEDDED {
 public:
  enum {
    kStartPositionIndex,
    kEndPositionIndex,
    kLiteralCountIndex,
    kPropertyCountIndex,
    kStrictModeIndex,
    kSize
  };

  explicit FunctionEntry(Vector<unsigned> backing)
    : backing_(backing) { }

  FunctionEntry() : backing_() { }

  int start_pos() { return backing_[kStartPositionIndex]; }
  int end_pos() { return backing_[kEndPositionIndex]; }
  int literal_count() { return backing_[kLiteralCountIndex]; }
  int property_count() { return backing_[kPropertyCountIndex]; }
  StrictMode strict_mode() {
    DCHECK(backing_[kStrictModeIndex] == SLOPPY ||
           backing_[kStrictModeIndex] == STRICT);
    return static_cast<StrictMode>(backing_[kStrictModeIndex]);
  }

  bool is_valid() { return !backing_.is_empty(); }

 private:
  Vector<unsigned> backing_;
};


// Wrapper around ScriptData to provide parser-specific functionality.
class ParseData {
 public:
  explicit ParseData(ScriptData* script_data) : script_data_(script_data) {
    CHECK(IsAligned(script_data->length(), sizeof(unsigned)));
    CHECK(IsSane());
  }
  void Initialize();
  FunctionEntry GetFunctionEntry(int start);
  int FunctionCount();

  bool HasError();

  unsigned* Data() {  // Writable data as unsigned int array.
    return reinterpret_cast<unsigned*>(const_cast<byte*>(script_data_->data()));
  }

 private:
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
  RegExpParser(FlatStringReader* in,
               Handle<String>* error,
               bool multiline_mode,
               Zone* zone);

  static bool ParseRegExp(FlatStringReader* input,
                          bool multiline,
                          RegExpCompileData* result,
                          Zone* zone);

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

    // Used by FunctionState and BlockState.
    typedef v8::internal::Scope Scope;
    typedef v8::internal::Scope* ScopePtr;
    inline static Scope* ptr_to_scope(ScopePtr scope) { return scope; }

    typedef Variable GeneratorVariable;
    typedef v8::internal::Zone Zone;

    typedef v8::internal::AstProperties AstProperties;
    typedef Vector<VariableProxy*> ParameterIdentifierVector;

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
    typedef ZoneList<v8::internal::Statement*>* StatementList;

    // For constructing objects returned by the traversing functions.
    typedef AstNodeFactory<AstConstructionVisitor> Factory;
  };

  explicit ParserTraits(Parser* parser) : parser_(parser) {}

  // Helper functions for recursive descent.
  bool IsEvalOrArguments(const AstRawString* identifier) const;
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

  bool IsConstructorProperty(ObjectLiteral::Property* property) {
    return property->key()->raw_value()->EqualsString(
        ast_value_factory()->constructor_string());
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
    if (scope->DeclarationScope()->is_global_scope() &&
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
  bool ShortcutNumericLiteralBinaryExpression(
      Expression** x, Expression* y, Token::Value op, int pos,
      AstNodeFactory<AstConstructionVisitor>* factory);

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
  Expression* BuildUnaryExpression(
      Expression* expression, Token::Value op, int pos,
      AstNodeFactory<AstConstructionVisitor>* factory);

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
  void ReportMessageAt(Scanner::Location source_location,
                       const char* message,
                       const char* arg = NULL,
                       bool is_reference_error = false);
  void ReportMessage(const char* message,
                     const char* arg = NULL,
                     bool is_reference_error = false);
  void ReportMessage(const char* message,
                     const AstRawString* arg,
                     bool is_reference_error = false);
  void ReportMessageAt(Scanner::Location source_location,
                       const char* message,
                       const AstRawString* arg,
                       bool is_reference_error = false);

  // "null" return type creators.
  static const AstRawString* EmptyIdentifier() {
    return NULL;
  }
  static Expression* EmptyExpression() {
    return NULL;
  }
  static Expression* EmptyArrowParamList() { return NULL; }
  static Literal* EmptyLiteral() {
    return NULL;
  }
  static ObjectLiteralProperty* EmptyObjectLiteralProperty() { return NULL; }
  static FunctionLiteral* EmptyFunctionLiteral() { return NULL; }

  // Used in error return values.
  static ZoneList<Expression*>* NullExpressionList() {
    return NULL;
  }

  // Non-NULL empty string.
  V8_INLINE const AstRawString* EmptyIdentifierString();

  // Odd-ball literal creators.
  Literal* GetLiteralTheHole(int position,
                             AstNodeFactory<AstConstructionVisitor>* factory);

  // Producing data during the recursive descent.
  const AstRawString* GetSymbol(Scanner* scanner);
  const AstRawString* GetNextSymbol(Scanner* scanner);
  const AstRawString* GetNumberAsSymbol(Scanner* scanner);

  Expression* ThisExpression(Scope* scope,
                             AstNodeFactory<AstConstructionVisitor>* factory,
                             int pos = RelocInfo::kNoPosition);
  Expression* SuperReference(Scope* scope,
                             AstNodeFactory<AstConstructionVisitor>* factory,
                             int pos = RelocInfo::kNoPosition);
  Expression* ClassExpression(const AstRawString* name, Expression* extends,
                              Expression* constructor,
                              ZoneList<ObjectLiteral::Property*>* properties,
                              int start_position, int end_position,
                              AstNodeFactory<AstConstructionVisitor>* factory);

  Literal* ExpressionFromLiteral(
      Token::Value token, int pos, Scanner* scanner,
      AstNodeFactory<AstConstructionVisitor>* factory);
  Expression* ExpressionFromIdentifier(
      const AstRawString* name, int pos, Scope* scope,
      AstNodeFactory<AstConstructionVisitor>* factory);
  Expression* ExpressionFromString(
      int pos, Scanner* scanner,
      AstNodeFactory<AstConstructionVisitor>* factory);
  Expression* GetIterator(Expression* iterable,
                          AstNodeFactory<AstConstructionVisitor>* factory);
  ZoneList<v8::internal::Expression*>* NewExpressionList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Expression*>(size, zone);
  }
  ZoneList<ObjectLiteral::Property*>* NewPropertyList(int size, Zone* zone) {
    return new(zone) ZoneList<ObjectLiteral::Property*>(size, zone);
  }
  ZoneList<v8::internal::Statement*>* NewStatementList(int size, Zone* zone) {
    return new(zone) ZoneList<v8::internal::Statement*>(size, zone);
  }
  V8_INLINE Scope* NewScope(Scope* parent_scope, ScopeType scope_type);

  // Utility functions
  int DeclareArrowParametersFromExpression(Expression* expression, Scope* scope,
                                           Scanner::Location* dupe_loc,
                                           bool* ok);
  V8_INLINE AstValueFactory* ast_value_factory();

  // Temporary glue; these functions will move to ParserBase.
  Expression* ParseV8Intrinsic(bool* ok);
  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      bool name_is_strict_reserved, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction, bool* ok);
  V8_INLINE void SkipLazyFunctionBody(const AstRawString* name,
                                      int* materialized_literal_count,
                                      int* expected_property_count, bool* ok);
  V8_INLINE ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* name, int pos, Variable* fvar,
      Token::Value fvar_init_op, bool is_generator, bool* ok);
  V8_INLINE void CheckConflictingVarDeclarations(v8::internal::Scope* scope,
                                                 bool* ok);

 private:
  Parser* parser_;
};


class Parser : public ParserBase<ParserTraits> {
 public:
  // Note that the hash seed in ParseInfo must be the hash seed from the
  // Isolate's heap, otherwise the heap will be in an inconsistent state once
  // the strings created by the Parser are internalized.
  struct ParseInfo {
    uintptr_t stack_limit;
    uint32_t hash_seed;
    UnicodeCache* unicode_cache;
  };

  Parser(CompilationInfo* info, ParseInfo* parse_info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = NULL;
    delete cached_parse_data_;
    cached_parse_data_ = NULL;
  }

  // Parses the source code represented by the compilation info and sets its
  // function literal.  Returns false (and deallocates any allocated AST
  // nodes) if parsing failed.
  static bool Parse(CompilationInfo* info,
                    bool allow_lazy = false) {
    ParseInfo parse_info = {info->isolate()->stack_guard()->real_climit(),
                            info->isolate()->heap()->HashSeed(),
                            info->isolate()->unicode_cache()};
    Parser parser(info, &parse_info);
    parser.set_allow_lazy(allow_lazy);
    if (parser.Parse()) {
      info->SetStrictMode(info->function()->strict_mode());
      return true;
    }
    return false;
  }
  bool Parse();
  void ParseOnBackground();

  // Handle errors detected during parsing, move statistics to Isolate,
  // internalize strings (move them to the heap).
  void Internalize();

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

  enum VariableDeclarationContext {
    kModuleElement,
    kBlockElement,
    kStatement,
    kForStatement
  };

  // If a list of variable declarations includes any initializers.
  enum VariableDeclarationProperties {
    kHasInitializers,
    kHasNoInitializers
  };

  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram();

  FunctionLiteral* ParseLazy();
  FunctionLiteral* ParseLazy(Utf16CharacterStream* source);

  Isolate* isolate() { return info_->isolate(); }
  CompilationInfo* info() const { return info_; }
  Handle<Script> script() const { return info_->script(); }
  AstValueFactory* ast_value_factory() const {
    return info_->ast_value_factory();
  }

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(CompilationInfo* info, Scope** scope,
                                  Scope** ad_hoc_eval_scope);

  void SetCachedData();

  bool inside_with() const { return scope_->inside_with(); }
  ScriptCompiler::CompileOptions compile_options() const {
    return info_->compile_options();
  }
  Scope* DeclarationScope(VariableMode mode) {
    return IsLexicalVariableMode(mode)
        ? scope_ : scope_->DeclarationScope();
  }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void* ParseSourceElements(ZoneList<Statement*>* processor, int end_token,
                            bool is_eval, bool is_global,
                            Scope** ad_hoc_eval_scope, bool* ok);
  Statement* ParseModuleElement(ZoneList<const AstRawString*>* labels,
                                bool* ok);
  Statement* ParseModuleDeclaration(ZoneList<const AstRawString*>* names,
                                    bool* ok);
  Module* ParseModule(bool* ok);
  Module* ParseModuleLiteral(bool* ok);
  Module* ParseModulePath(bool* ok);
  Module* ParseModuleVariable(bool* ok);
  Module* ParseModuleUrl(bool* ok);
  Module* ParseModuleSpecifier(bool* ok);
  Block* ParseImportDeclaration(bool* ok);
  Statement* ParseExportDeclaration(bool* ok);
  Statement* ParseBlockElement(ZoneList<const AstRawString*>* labels, bool* ok);
  Statement* ParseStatement(ZoneList<const AstRawString*>* labels, bool* ok);
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
                                   VariableDeclarationProperties* decl_props,
                                   ZoneList<const AstRawString*>* names,
                                   const AstRawString** out,
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
  Statement* DesugarLetBindingsInForStatement(
      Scope* inner_scope, ZoneList<const AstRawString*>* names,
      ForStatement* loop, Statement* init, Expression* cond, Statement* next,
      Statement* body, bool* ok);

  FunctionLiteral* ParseFunctionLiteral(
      const AstRawString* name, Scanner::Location function_name_location,
      bool name_is_strict_reserved, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction, bool* ok);

  // Magical syntax support.
  Expression* ParseV8Intrinsic(bool* ok);

  bool CheckInOrOf(bool accept_OF, ForEachStatement::VisitMode* visit_mode);

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
  VariableProxy* NewUnresolved(const AstRawString* name,
                               VariableMode mode,
                               Interface* interface);
  void Declare(Declaration* declaration, bool resolve, bool* ok);

  bool TargetStackContainsLabel(const AstRawString* label);
  BreakableStatement* LookupBreakTarget(const AstRawString* label, bool* ok);
  IterationStatement* LookupContinueTarget(const AstRawString* label, bool* ok);

  void RegisterTargetUse(Label* target, Target* stop);

  // Factory methods.

  Scope* NewScope(Scope* parent, ScopeType type);

  // Skip over a lazy function, either using cached data if we have it, or
  // by parsing the function with PreParser. Consumes the ending }.
  void SkipLazyFunctionBody(const AstRawString* function_name,
                            int* materialized_literal_count,
                            int* expected_property_count,
                            bool* ok);

  PreParser::PreParseResult ParseLazyFunctionBodyWithPreParser(
      SingletonLogger* logger);

  // Consumes the ending }.
  ZoneList<Statement*>* ParseEagerFunctionBody(
      const AstRawString* function_name, int pos, Variable* fvar,
      Token::Value fvar_init_op, bool is_generator, bool* ok);

  void HandleSourceURLComments();

  void ThrowPendingError();

  Scanner scanner_;
  PreParser* reusable_preparser_;
  Scope* original_scope_;  // for ES5 function declarations in sloppy eval
  Target* target_stack_;  // for break, continue statements
  ParseData* cached_parse_data_;

  CompilationInfo* info_;

  // Pending errors.
  bool has_pending_error_;
  Scanner::Location pending_error_location_;
  const char* pending_error_message_;
  const AstRawString* pending_error_arg_;
  const char* pending_error_char_arg_;
  bool pending_error_is_reference_error_;

  // Other information which will be stored in Parser and moved to Isolate after
  // parsing.
  int use_counts_[v8::Isolate::kUseCounterFeatureCount];
  int total_preparse_skipped_;
  HistogramTimer* pre_parse_timer_;

  // Temporary; for debugging. See Parser::SkipLazyFunctionBody. TODO(marja):
  // remove this once done.
  ScriptCompiler::CompileOptions debug_saved_compile_options_;
};


bool ParserTraits::IsFutureStrictReserved(
    const AstRawString* identifier) const {
  return parser_->scanner()->IdentifierIsFutureStrictReserved(identifier);
}


Scope* ParserTraits::NewScope(Scope* parent_scope, ScopeType scope_type) {
  return parser_->NewScope(parent_scope, scope_type);
}


const AstRawString* ParserTraits::EmptyIdentifierString() {
  return parser_->ast_value_factory()->empty_string();
}


void ParserTraits::SkipLazyFunctionBody(const AstRawString* function_name,
                                        int* materialized_literal_count,
                                        int* expected_property_count,
                                        bool* ok) {
  return parser_->SkipLazyFunctionBody(
      function_name, materialized_literal_count, expected_property_count, ok);
}


ZoneList<Statement*>* ParserTraits::ParseEagerFunctionBody(
    const AstRawString* name, int pos, Variable* fvar,
    Token::Value fvar_init_op, bool is_generator, bool* ok) {
  return parser_->ParseEagerFunctionBody(name, pos, fvar, fvar_init_op,
                                         is_generator, ok);
}

void ParserTraits::CheckConflictingVarDeclarations(v8::internal::Scope* scope,
                                                   bool* ok) {
  parser_->CheckConflictingVarDeclarations(scope, ok);
}


AstValueFactory* ParserTraits::ast_value_factory() {
  return parser_->ast_value_factory();
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

} }  // namespace v8::internal

#endif  // V8_PARSER_H_
