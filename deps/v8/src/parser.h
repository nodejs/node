// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PARSER_H_
#define V8_PARSER_H_

#include "allocation.h"
#include "ast.h"
#include "preparse-data-format.h"
#include "preparse-data.h"
#include "scopes.h"
#include "preparser.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class FuncNameInferrer;
class ParserLog;
class PositionStack;
class Target;

template <typename T> class ZoneListWrapper;


class ParserMessage : public Malloced {
 public:
  ParserMessage(Scanner::Location loc, const char* message,
                Vector<const char*> args)
      : loc_(loc),
        message_(message),
        args_(args) { }
  ~ParserMessage();
  Scanner::Location location() { return loc_; }
  const char* message() { return message_; }
  Vector<const char*> args() { return args_; }
 private:
  Scanner::Location loc_;
  const char* message_;
  Vector<const char*> args_;
};


class FunctionEntry BASE_EMBEDDED {
 public:
  enum {
    kStartPositionIndex,
    kEndPositionIndex,
    kLiteralCountIndex,
    kPropertyCountIndex,
    kLanguageModeIndex,
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
    ASSERT(backing_[kLanguageModeIndex] == CLASSIC_MODE ||
           backing_[kLanguageModeIndex] == STRICT_MODE ||
           backing_[kLanguageModeIndex] == EXTENDED_MODE);
    return static_cast<LanguageMode>(backing_[kLanguageModeIndex]);
  }

  bool is_valid() { return !backing_.is_empty(); }

 private:
  Vector<unsigned> backing_;
};


class ScriptDataImpl : public ScriptData {
 public:
  explicit ScriptDataImpl(Vector<unsigned> store)
      : store_(store),
        owns_store_(true) { }

  // Create an empty ScriptDataImpl that is guaranteed to not satisfy
  // a SanityCheck.
  ScriptDataImpl() : owns_store_(false) { }

  virtual ~ScriptDataImpl();
  virtual int Length();
  virtual const char* Data();
  virtual bool HasError();

  void Initialize();
  void ReadNextSymbolPosition();

  FunctionEntry GetFunctionEntry(int start);
  int GetSymbolIdentifier();
  bool SanityCheck();

  Scanner::Location MessageLocation();
  const char* BuildMessage();
  Vector<const char*> BuildArgs();

  int symbol_count() {
    return (store_.length() > PreparseDataConstants::kHeaderSize)
        ? store_[PreparseDataConstants::kSymbolCountOffset]
        : 0;
  }
  // The following functions should only be called if SanityCheck has
  // returned true.
  bool has_error() { return store_[PreparseDataConstants::kHasErrorOffset]; }
  unsigned magic() { return store_[PreparseDataConstants::kMagicOffset]; }
  unsigned version() { return store_[PreparseDataConstants::kVersionOffset]; }

 private:
  Vector<unsigned> store_;
  unsigned char* symbol_data_;
  unsigned char* symbol_data_end_;
  int function_index_;
  bool owns_store_;

  unsigned Read(int position);
  unsigned* ReadAddress(int position);
  // Reads a number from the current symbols
  int ReadNumber(byte** source);

  ScriptDataImpl(const char* backing_store, int length)
      : store_(reinterpret_cast<unsigned*>(const_cast<char*>(backing_store)),
               length / static_cast<int>(sizeof(unsigned))),
        owns_store_(false) {
    ASSERT_EQ(0, static_cast<int>(
        reinterpret_cast<intptr_t>(backing_store) % sizeof(unsigned)));
  }

  // Read strings written by ParserRecorder::WriteString.
  static const char* ReadString(unsigned* start, int* chars);

  friend class ScriptData;
};


class PreParserApi {
 public:
  // Pre-parse a character stream and return full preparse data.
  //
  // This interface is here instead of in preparser.h because it instantiates a
  // preparser recorder object that is suited to the parser's purposes.  Also,
  // the preparser doesn't know about ScriptDataImpl.
  static ScriptDataImpl* PreParse(Utf16CharacterStream* source);
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
    ASSERT(last_ != NULL);
    return last_;
  }

  T* RemoveLast() {
    ASSERT(last_ != NULL);
    T* result = last_;
    if ((list_ != NULL) && (list_->length() > 0))
      last_ = list_->RemoveLast();
    else
      last_ = NULL;
    return result;
  }

  T* Get(int i) {
    ASSERT((0 <= i) && (i < length()));
    if (list_ == NULL) {
      ASSERT_EQ(0, i);
      return last_;
    } else {
      if (i == list_->length()) {
        ASSERT(last_ != NULL);
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

// Forward declaration.
class SingletonLogger;

class Parser BASE_EMBEDDED {
 public:
  explicit Parser(CompilationInfo* info);
  ~Parser() {
    delete reusable_preparser_;
    reusable_preparser_ = NULL;
  }

  bool allow_natives_syntax() const { return allow_natives_syntax_; }
  bool allow_lazy() const { return allow_lazy_; }
  bool allow_modules() { return scanner().HarmonyModules(); }
  bool allow_harmony_scoping() { return scanner().HarmonyScoping(); }
  bool allow_generators() const { return allow_generators_; }
  bool allow_for_of() const { return allow_for_of_; }

  void set_allow_natives_syntax(bool allow) { allow_natives_syntax_ = allow; }
  void set_allow_lazy(bool allow) { allow_lazy_ = allow; }
  void set_allow_modules(bool allow) { scanner().SetHarmonyModules(allow); }
  void set_allow_harmony_scoping(bool allow) {
    scanner().SetHarmonyScoping(allow);
  }
  void set_allow_generators(bool allow) { allow_generators_ = allow; }
  void set_allow_for_of(bool allow) { allow_for_of_ = allow; }

  // Parses the source code represented by the compilation info and sets its
  // function literal.  Returns false (and deallocates any allocated AST
  // nodes) if parsing failed.
  static bool Parse(CompilationInfo* info) { return Parser(info).Parse(); }
  bool Parse();

  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram();

  void ReportMessageAt(Scanner::Location loc,
                       const char* message,
                       Vector<const char*> args);
  void ReportMessageAt(Scanner::Location loc,
                       const char* message,
                       Vector<Handle<String> > args);

 private:
  static const int kMaxNumFunctionLocals = 131071;  // 2^17-1

  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

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

  class BlockState;

  class FunctionState BASE_EMBEDDED {
   public:
    FunctionState(Parser* parser,
                  Scope* scope,
                  Isolate* isolate);
    ~FunctionState();

    int NextMaterializedLiteralIndex() {
      return next_materialized_literal_index_++;
    }
    int materialized_literal_count() {
      return next_materialized_literal_index_ - JSFunction::kLiteralsPrefixSize;
    }

    int NextHandlerIndex() { return next_handler_index_++; }
    int handler_count() { return next_handler_index_; }

    void AddProperty() { expected_property_count_++; }
    int expected_property_count() { return expected_property_count_; }

    void set_generator_object_variable(Variable *variable) {
      ASSERT(variable != NULL);
      ASSERT(!is_generator());
      generator_object_variable_ = variable;
    }
    Variable* generator_object_variable() const {
      return generator_object_variable_;
    }
    bool is_generator() const {
      return generator_object_variable_ != NULL;
    }

    AstNodeFactory<AstConstructionVisitor>* factory() { return &factory_; }

   private:
    // Used to assign an index to each literal that needs materialization in
    // the function.  Includes regexp literals, and boilerplate for object and
    // array literals.
    int next_materialized_literal_index_;

    // Used to assign a per-function index to try and catch handlers.
    int next_handler_index_;

    // Properties count estimation.
    int expected_property_count_;

    // For generators, the variable that holds the generator object.  This
    // variable is used by yield expressions and return statements.  NULL
    // indicates that this function is not a generator.
    Variable* generator_object_variable_;

    Parser* parser_;
    FunctionState* outer_function_state_;
    Scope* outer_scope_;
    int saved_ast_node_id_;
    AstNodeFactory<AstConstructionVisitor> factory_;
  };

  class ParsingModeScope BASE_EMBEDDED {
   public:
    ParsingModeScope(Parser* parser, Mode mode)
        : parser_(parser),
          old_mode_(parser->mode()) {
      parser_->mode_ = mode;
    }
    ~ParsingModeScope() {
      parser_->mode_ = old_mode_;
    }

   private:
    Parser* parser_;
    Mode old_mode_;
  };

  FunctionLiteral* ParseLazy();
  FunctionLiteral* ParseLazy(Utf16CharacterStream* source);

  Isolate* isolate() { return isolate_; }
  Zone* zone() const { return zone_; }
  CompilationInfo* info() const { return info_; }

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(CompilationInfo* info,
                                  Handle<String> source);

  // Report syntax error
  void ReportUnexpectedToken(Token::Value token);
  void ReportInvalidPreparseData(Handle<String> name, bool* ok);
  void ReportMessage(const char* message, Vector<const char*> args);
  void ReportMessage(const char* message, Vector<Handle<String> > args);

  void set_pre_parse_data(ScriptDataImpl *data) {
    pre_parse_data_ = data;
    symbol_cache_.Initialize(data ? data->symbol_count() : 0, zone());
  }

  bool inside_with() const { return top_scope_->inside_with(); }
  Scanner& scanner()  { return scanner_; }
  Mode mode() const { return mode_; }
  ScriptDataImpl* pre_parse_data() const { return pre_parse_data_; }
  bool is_extended_mode() {
    ASSERT(top_scope_ != NULL);
    return top_scope_->is_extended_mode();
  }
  Scope* DeclarationScope(VariableMode mode) {
    return IsLexicalVariableMode(mode)
        ? top_scope_ : top_scope_->DeclarationScope();
  }

  // Check if the given string is 'eval' or 'arguments'.
  bool IsEvalOrArguments(Handle<String> string);

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void* ParseSourceElements(ZoneList<Statement*>* processor, int end_token,
                            bool is_eval, bool is_global, bool* ok);
  Statement* ParseModuleElement(ZoneStringList* labels, bool* ok);
  Statement* ParseModuleDeclaration(ZoneStringList* names, bool* ok);
  Module* ParseModule(bool* ok);
  Module* ParseModuleLiteral(bool* ok);
  Module* ParseModulePath(bool* ok);
  Module* ParseModuleVariable(bool* ok);
  Module* ParseModuleUrl(bool* ok);
  Module* ParseModuleSpecifier(bool* ok);
  Block* ParseImportDeclaration(bool* ok);
  Statement* ParseExportDeclaration(bool* ok);
  Statement* ParseBlockElement(ZoneStringList* labels, bool* ok);
  Statement* ParseStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseFunctionDeclaration(ZoneStringList* names, bool* ok);
  Statement* ParseNativeDeclaration(bool* ok);
  Block* ParseBlock(ZoneStringList* labels, bool* ok);
  Block* ParseVariableStatement(VariableDeclarationContext var_context,
                                ZoneStringList* names,
                                bool* ok);
  Block* ParseVariableDeclarations(VariableDeclarationContext var_context,
                                   VariableDeclarationProperties* decl_props,
                                   ZoneStringList* names,
                                   Handle<String>* out,
                                   bool* ok);
  Statement* ParseExpressionOrLabelledStatement(ZoneStringList* labels,
                                                bool* ok);
  IfStatement* ParseIfStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseContinueStatement(bool* ok);
  Statement* ParseBreakStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseReturnStatement(bool* ok);
  Statement* ParseWithStatement(ZoneStringList* labels, bool* ok);
  CaseClause* ParseCaseClause(bool* default_seen_ptr, bool* ok);
  SwitchStatement* ParseSwitchStatement(ZoneStringList* labels, bool* ok);
  DoWhileStatement* ParseDoWhileStatement(ZoneStringList* labels, bool* ok);
  WhileStatement* ParseWhileStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseForStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseThrowStatement(bool* ok);
  Expression* MakeCatchContext(Handle<String> id, VariableProxy* value);
  TryStatement* ParseTryStatement(bool* ok);
  DebuggerStatement* ParseDebuggerStatement(bool* ok);

  // Support for hamony block scoped bindings.
  Block* ParseScopedBlock(ZoneStringList* labels, bool* ok);

  Expression* ParseExpression(bool accept_IN, bool* ok);
  Expression* ParseAssignmentExpression(bool accept_IN, bool* ok);
  Expression* ParseYieldExpression(bool* ok);
  Expression* ParseConditionalExpression(bool accept_IN, bool* ok);
  Expression* ParseBinaryExpression(int prec, bool accept_IN, bool* ok);
  Expression* ParseUnaryExpression(bool* ok);
  Expression* ParsePostfixExpression(bool* ok);
  Expression* ParseLeftHandSideExpression(bool* ok);
  Expression* ParseNewExpression(bool* ok);
  Expression* ParseMemberExpression(bool* ok);
  Expression* ParseNewPrefix(PositionStack* stack, bool* ok);
  Expression* ParseMemberWithNewPrefixesExpression(PositionStack* stack,
                                                   bool* ok);
  Expression* ParsePrimaryExpression(bool* ok);
  Expression* ParseArrayLiteral(bool* ok);
  Expression* ParseObjectLiteral(bool* ok);
  ObjectLiteral::Property* ParseObjectLiteralGetSet(bool is_getter, bool* ok);
  Expression* ParseRegExpLiteral(bool seen_equal, bool* ok);

  // Populate the constant properties fixed array for a materialized object
  // literal.
  void BuildObjectLiteralConstantProperties(
      ZoneList<ObjectLiteral::Property*>* properties,
      Handle<FixedArray> constants,
      bool* is_simple,
      bool* fast_elements,
      int* depth,
      bool* may_store_doubles);

  // Decide if a property should be in the object boilerplate.
  bool IsBoilerplateProperty(ObjectLiteral::Property* property);
  // If the expression is a literal, return the literal value;
  // if the expression is a materialized literal and is simple return a
  // compile time value as encoded by CompileTimeValue::GetValue().
  // Otherwise, return undefined literal as the placeholder
  // in the object literal boilerplate.
  Handle<Object> GetBoilerplateValue(Expression* expression);

  // Initialize the components of a for-in / for-of statement.
  void InitializeForEachStatement(ForEachStatement* stmt,
                                  Expression* each,
                                  Expression* subject,
                                  Statement* body);

  ZoneList<Expression*>* ParseArguments(bool* ok);
  FunctionLiteral* ParseFunctionLiteral(Handle<String> var_name,
                                        bool name_is_reserved,
                                        bool is_generator,
                                        int function_token_position,
                                        FunctionLiteral::FunctionType type,
                                        bool* ok);


  // Magical syntax support.
  Expression* ParseV8Intrinsic(bool* ok);

  INLINE(Token::Value peek()) {
    if (stack_overflow_) return Token::ILLEGAL;
    return scanner().peek();
  }

  INLINE(Token::Value Next()) {
    // BUG 1215673: Find a thread safe way to set a stack limit in
    // pre-parse mode. Otherwise, we cannot safely pre-parse from other
    // threads.
    if (stack_overflow_) {
      return Token::ILLEGAL;
    }
    if (StackLimitCheck(isolate()).HasOverflowed()) {
      // Any further calls to Next or peek will return the illegal token.
      // The current call must return the next token, which might already
      // have been peek'ed.
      stack_overflow_ = true;
    }
    return scanner().Next();
  }

  bool is_generator() const { return current_function_state_->is_generator(); }

  bool CheckInOrOf(bool accept_OF, ForEachStatement::VisitMode* visit_mode);

  bool peek_any_identifier();

  INLINE(void Consume(Token::Value token));
  void Expect(Token::Value token, bool* ok);
  bool Check(Token::Value token);
  void ExpectSemicolon(bool* ok);
  bool CheckContextualKeyword(Vector<const char> keyword);
  void ExpectContextualKeyword(Vector<const char> keyword, bool* ok);

  Handle<String> LiteralString(PretenureFlag tenured) {
    if (scanner().is_literal_ascii()) {
      return isolate_->factory()->NewStringFromAscii(
          scanner().literal_ascii_string(), tenured);
    } else {
      return isolate_->factory()->NewStringFromTwoByte(
            scanner().literal_utf16_string(), tenured);
    }
  }

  Handle<String> NextLiteralString(PretenureFlag tenured) {
    if (scanner().is_next_literal_ascii()) {
      return isolate_->factory()->NewStringFromAscii(
          scanner().next_literal_ascii_string(), tenured);
    } else {
      return isolate_->factory()->NewStringFromTwoByte(
          scanner().next_literal_utf16_string(), tenured);
    }
  }

  Handle<String> GetSymbol();

  // Get odd-ball literals.
  Literal* GetLiteralUndefined();
  Literal* GetLiteralTheHole();

  Handle<String> ParseIdentifier(bool* ok);
  Handle<String> ParseIdentifierOrStrictReservedWord(
      bool* is_strict_reserved, bool* ok);
  Handle<String> ParseIdentifierName(bool* ok);
  Handle<String> ParseIdentifierNameOrGetOrSet(bool* is_get,
                                               bool* is_set,
                                               bool* ok);

  // Determine if the expression is a variable proxy and mark it as being used
  // in an assignment or with a increment/decrement operator. This is currently
  // used on for the statically checking assignments to harmony const bindings.
  void MarkAsLValue(Expression* expression);

  // Strict mode validation of LValue expressions
  void CheckStrictModeLValue(Expression* expression,
                             const char* error,
                             bool* ok);

  // Strict mode octal literal validation.
  void CheckOctalLiteral(int beg_pos, int end_pos, bool* ok);

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
  VariableProxy* NewUnresolved(Handle<String> name,
                               VariableMode mode,
                               Interface* interface);
  void Declare(Declaration* declaration, bool resolve, bool* ok);

  bool TargetStackContainsLabel(Handle<String> label);
  BreakableStatement* LookupBreakTarget(Handle<String> label, bool* ok);
  IterationStatement* LookupContinueTarget(Handle<String> label, bool* ok);

  void RegisterTargetUse(Label* target, Target* stop);

  // Factory methods.

  Scope* NewScope(Scope* parent, ScopeType type);

  Handle<String> LookupSymbol(int symbol_id);

  Handle<String> LookupCachedSymbol(int symbol_id);

  // Generate AST node that throw a ReferenceError with the given type.
  Expression* NewThrowReferenceError(Handle<String> type);

  // Generate AST node that throw a SyntaxError with the given
  // type. The first argument may be null (in the handle sense) in
  // which case no arguments are passed to the constructor.
  Expression* NewThrowSyntaxError(Handle<String> type, Handle<Object> first);

  // Generate AST node that throw a TypeError with the given
  // type. Both arguments must be non-null (in the handle sense).
  Expression* NewThrowTypeError(Handle<String> type,
                                Handle<Object> first,
                                Handle<Object> second);

  // Generic AST generator for throwing errors from compiled code.
  Expression* NewThrowError(Handle<String> constructor,
                            Handle<String> type,
                            Vector< Handle<Object> > arguments);

  preparser::PreParser::PreParseResult LazyParseFunctionLiteral(
       SingletonLogger* logger);

  AstNodeFactory<AstConstructionVisitor>* factory() {
    return current_function_state_->factory();
  }

  Isolate* isolate_;
  ZoneList<Handle<String> > symbol_cache_;

  Handle<Script> script_;
  Scanner scanner_;
  preparser::PreParser* reusable_preparser_;
  Scope* top_scope_;
  FunctionState* current_function_state_;
  Target* target_stack_;  // for break, continue statements
  v8::Extension* extension_;
  ScriptDataImpl* pre_parse_data_;
  FuncNameInferrer* fni_;

  Mode mode_;
  bool allow_natives_syntax_;
  bool allow_lazy_;
  bool allow_generators_;
  bool allow_for_of_;
  bool stack_overflow_;
  // If true, the next (and immediately following) function literal is
  // preceded by a parenthesis.
  // Heuristically that means that the function will be called immediately,
  // so never lazily compile it.
  bool parenthesized_function_;

  Zone* zone_;
  CompilationInfo* info_;
  friend class BlockState;
  friend class FunctionState;
};


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
  static Handle<FixedArray> GetValue(Expression* expression);

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
