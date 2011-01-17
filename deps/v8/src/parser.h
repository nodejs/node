// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "scanner.h"
#include "scopes.h"
#include "preparse-data.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class FuncNameInferrer;
class ParserLog;
class PositionStack;
class Target;
class TemporaryScope;

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
  explicit FunctionEntry(Vector<unsigned> backing) : backing_(backing) { }
  FunctionEntry() : backing_(Vector<unsigned>::empty()) { }

  int start_pos() { return backing_[kStartPosOffset]; }
  void set_start_pos(int value) { backing_[kStartPosOffset] = value; }

  int end_pos() { return backing_[kEndPosOffset]; }
  void set_end_pos(int value) { backing_[kEndPosOffset] = value; }

  int literal_count() { return backing_[kLiteralCountOffset]; }
  void set_literal_count(int value) { backing_[kLiteralCountOffset] = value; }

  int property_count() { return backing_[kPropertyCountOffset]; }
  void set_property_count(int value) {
    backing_[kPropertyCountOffset] = value;
  }

  bool is_valid() { return backing_.length() > 0; }

  static const int kSize = 4;

 private:
  Vector<unsigned> backing_;
  static const int kStartPosOffset = 0;
  static const int kEndPosOffset = 1;
  static const int kLiteralCountOffset = 2;
  static const int kPropertyCountOffset = 3;
};


class ScriptDataImpl : public ScriptData {
 public:
  explicit ScriptDataImpl(Vector<unsigned> store)
      : store_(store),
        owns_store_(true) { }

  // Create an empty ScriptDataImpl that is guaranteed to not satisfy
  // a SanityCheck.
  ScriptDataImpl() : store_(Vector<unsigned>()), owns_store_(false) { }

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


class ParserApi {
 public:
  // Parses the source code represented by the compilation info and sets its
  // function literal.  Returns false (and deallocates any allocated AST
  // nodes) if parsing failed.
  static bool Parse(CompilationInfo* info);

  // Generic preparser generating full preparse data.
  static ScriptDataImpl* PreParse(UC16CharacterStream* source,
                                  v8::Extension* extension);

  // Preparser that only does preprocessing that makes sense if only used
  // immediately after.
  static ScriptDataImpl* PartialPreParse(UC16CharacterStream* source,
                                         v8::Extension* extension);
};

// ----------------------------------------------------------------------------
// REGEXP PARSING

// A BuffferedZoneList is an automatically growing list, just like (and backed
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
  void Add(T* value) {
    if (last_ != NULL) {
      if (list_ == NULL) {
        list_ = new ZoneList<T*>(initial_size);
      }
      list_->Add(last_);
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

  ZoneList<T*>* GetList() {
    if (list_ == NULL) {
      list_ = new ZoneList<T*>(initial_size);
    }
    if (last_ != NULL) {
      list_->Add(last_);
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
  RegExpBuilder();
  void AddCharacter(uc16 character);
  // "Adds" an empty expression. Does nothing except consume a
  // following quantifier
  void AddEmpty();
  void AddAtom(RegExpTree* tree);
  void AddAssertion(RegExpTree* tree);
  void NewAlternative();  // '|'
  void AddQuantifierToAtom(int min, int max, RegExpQuantifier::Type type);
  RegExpTree* ToRegExp();

 private:
  void FlushCharacters();
  void FlushText();
  void FlushTerms();
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


class RegExpParser {
 public:
  RegExpParser(FlatStringReader* in,
               Handle<String>* error,
               bool multiline_mode);

  static bool ParseRegExp(FlatStringReader* input,
                          bool multiline,
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
                      int disjunction_capture_index)
        : previous_state_(previous_state),
          builder_(new RegExpBuilder()),
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

  uc32 current() { return current_; }
  bool has_more() { return has_more_; }
  bool has_next() { return next_pos_ < in()->length(); }
  uc32 Next();
  FlatStringReader* in() { return in_; }
  void ScanForCaptures();

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

class Parser {
 public:
  Parser(Handle<Script> script,
         bool allow_natives_syntax,
         v8::Extension* extension,
         ScriptDataImpl* pre_data);
  virtual ~Parser() { }

  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram(Handle<String> source,
                                bool in_global_context);

  FunctionLiteral* ParseLazy(Handle<SharedFunctionInfo> info);

  void ReportMessageAt(Scanner::Location loc,
                       const char* message,
                       Vector<const char*> args);
  void ReportMessageAt(Scanner::Location loc,
                       const char* message,
                       Vector<Handle<String> > args);

 protected:
  FunctionLiteral* ParseLazy(Handle<SharedFunctionInfo> info,
                             UC16CharacterStream* source,
                             ZoneScope* zone_scope);
  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

  // Called by ParseProgram after setting up the scanner.
  FunctionLiteral* DoParseProgram(Handle<String> source,
                                  bool in_global_context,
                                  ZoneScope* zone_scope);

  // Report syntax error
  void ReportUnexpectedToken(Token::Value token);
  void ReportInvalidPreparseData(Handle<String> name, bool* ok);
  void ReportMessage(const char* message, Vector<const char*> args);

  bool inside_with() const { return with_nesting_level_ > 0; }
  V8JavaScriptScanner& scanner()  { return scanner_; }
  Mode mode() const { return mode_; }
  ScriptDataImpl* pre_data() const { return pre_data_; }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void* ParseSourceElements(ZoneList<Statement*>* processor,
                            int end_token, bool* ok);
  Statement* ParseStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseFunctionDeclaration(bool* ok);
  Statement* ParseNativeDeclaration(bool* ok);
  Block* ParseBlock(ZoneStringList* labels, bool* ok);
  Block* ParseVariableStatement(bool* ok);
  Block* ParseVariableDeclarations(bool accept_IN, Expression** var, bool* ok);
  Statement* ParseExpressionOrLabelledStatement(ZoneStringList* labels,
                                                bool* ok);
  IfStatement* ParseIfStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseContinueStatement(bool* ok);
  Statement* ParseBreakStatement(ZoneStringList* labels, bool* ok);
  Statement* ParseReturnStatement(bool* ok);
  Block* WithHelper(Expression* obj,
                    ZoneStringList* labels,
                    bool is_catch_block,
                    bool* ok);
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

  Expression* ParseExpression(bool accept_IN, bool* ok);
  Expression* ParseAssignmentExpression(bool accept_IN, bool* ok);
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

  Expression* NewCompareNode(Token::Value op,
                             Expression* x,
                             Expression* y,
                             int position);

  // Populate the constant properties fixed array for a materialized object
  // literal.
  void BuildObjectLiteralConstantProperties(
      ZoneList<ObjectLiteral::Property*>* properties,
      Handle<FixedArray> constants,
      bool* is_simple,
      bool* fast_elements,
      int* depth);

  // Populate the literals fixed array for a materialized array literal.
  void BuildArrayLiteralBoilerplateLiterals(ZoneList<Expression*>* properties,
                                            Handle<FixedArray> constants,
                                            bool* is_simple,
                                            int* depth);

  // Decide if a property should be in the object boilerplate.
  bool IsBoilerplateProperty(ObjectLiteral::Property* property);
  // If the expression is a literal, return the literal value;
  // if the expression is a materialized literal and is simple return a
  // compile time value as encoded by CompileTimeValue::GetValue().
  // Otherwise, return undefined literal as the placeholder
  // in the object literal boilerplate.
  Handle<Object> GetBoilerplateValue(Expression* expression);

  enum FunctionLiteralType {
    EXPRESSION,
    DECLARATION,
    NESTED
  };

  ZoneList<Expression*>* ParseArguments(bool* ok);
  FunctionLiteral* ParseFunctionLiteral(Handle<String> var_name,
                                        int function_token_position,
                                        FunctionLiteralType type,
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
    if (StackLimitCheck().HasOverflowed()) {
      // Any further calls to Next or peek will return the illegal token.
      // The current call must return the next token, which might already
      // have been peek'ed.
      stack_overflow_ = true;
    }
    return scanner().Next();
  }

  INLINE(void Consume(Token::Value token));
  void Expect(Token::Value token, bool* ok);
  bool Check(Token::Value token);
  void ExpectSemicolon(bool* ok);

  Handle<String> LiteralString(PretenureFlag tenured) {
    if (scanner().is_literal_ascii()) {
      return Factory::NewStringFromAscii(scanner().literal_ascii_string(),
                                         tenured);
    } else {
      return Factory::NewStringFromTwoByte(scanner().literal_uc16_string(),
                                           tenured);
    }
  }

  Handle<String> NextLiteralString(PretenureFlag tenured) {
    if (scanner().is_next_literal_ascii()) {
      return Factory::NewStringFromAscii(scanner().next_literal_ascii_string(),
                                         tenured);
    } else {
      return Factory::NewStringFromTwoByte(scanner().next_literal_uc16_string(),
                                           tenured);
    }
  }

  Handle<String> GetSymbol(bool* ok);

  // Get odd-ball literals.
  Literal* GetLiteralUndefined();
  Literal* GetLiteralTheHole();
  Literal* GetLiteralNumber(double value);

  Handle<String> ParseIdentifier(bool* ok);
  Handle<String> ParseIdentifierName(bool* ok);
  Handle<String> ParseIdentifierOrGetOrSet(bool* is_get,
                                           bool* is_set,
                                           bool* ok);

  // Parser support
  VariableProxy* Declare(Handle<String> name, Variable::Mode mode,
                         FunctionLiteral* fun,
                         bool resolve,
                         bool* ok);

  bool TargetStackContainsLabel(Handle<String> label);
  BreakableStatement* LookupBreakTarget(Handle<String> label, bool* ok);
  IterationStatement* LookupContinueTarget(Handle<String> label, bool* ok);

  void RegisterTargetUse(BreakTarget* target, Target* stop);

  // Factory methods.

  Statement* EmptyStatement() {
    static v8::internal::EmptyStatement empty;
    return &empty;
  }

  Scope* NewScope(Scope* parent, Scope::Type type, bool inside_with);

  Handle<String> LookupSymbol(int symbol_id);

  Handle<String> LookupCachedSymbol(int symbol_id);

  Expression* NewCall(Expression* expression,
                      ZoneList<Expression*>* arguments,
                      int pos) {
    return new Call(expression, arguments, pos);
  }


  // Create a number literal.
  Literal* NewNumberLiteral(double value);

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

  ZoneList<Handle<String> > symbol_cache_;

  Handle<Script> script_;
  V8JavaScriptScanner scanner_;

  Scope* top_scope_;
  int with_nesting_level_;

  TemporaryScope* temp_scope_;
  Mode mode_;

  Target* target_stack_;  // for break, continue statements
  bool allow_natives_syntax_;
  v8::Extension* extension_;
  bool is_pre_parsing_;
  ScriptDataImpl* pre_data_;
  FuncNameInferrer* fni_;
  bool stack_overflow_;
  // If true, the next (and immediately following) function literal is
  // preceded by a parenthesis.
  // Heuristically that means that the function will be called immediately,
  // so never lazily compile it.
  bool parenthesized_function_;
};


// Support for handling complex values (array and object literals) that
// can be fully handled at compile time.
class CompileTimeValue: public AllStatic {
 public:
  enum Type {
    OBJECT_LITERAL_FAST_ELEMENTS,
    OBJECT_LITERAL_SLOW_ELEMENTS,
    ARRAY_LITERAL
  };

  static bool IsCompileTimeValue(Expression* expression);

  static bool ArrayLiteralElementNeedsInitialization(Expression* value);

  // Get the value as a compile time value.
  static Handle<FixedArray> GetValue(Expression* expression);

  // Get the type of a compile time value returned by GetValue().
  static Type GetType(Handle<FixedArray> value);

  // Get the elements array of a compile time value returned by GetValue().
  static Handle<FixedArray> GetElements(Handle<FixedArray> value);

 private:
  static const int kTypeSlot = 0;
  static const int kElementsSlot = 1;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompileTimeValue);
};


// ----------------------------------------------------------------------------
// JSON PARSING

// JSON is a subset of JavaScript, as specified in, e.g., the ECMAScript 5
// specification section 15.12.1 (and appendix A.8).
// The grammar is given section 15.12.1.2 (and appendix A.8.2).
class JsonParser BASE_EMBEDDED {
 public:
  // Parse JSON input as a single JSON value.
  // Returns null handle and sets exception if parsing failed.
  static Handle<Object> Parse(Handle<String> source) {
    if (source->IsExternalTwoByteString()) {
      ExternalTwoByteStringUC16CharacterStream stream(
          Handle<ExternalTwoByteString>::cast(source), 0, source->length());
      return JsonParser().ParseJson(source, &stream);
    } else {
      GenericStringUC16CharacterStream stream(source, 0, source->length());
      return JsonParser().ParseJson(source, &stream);
    }
  }

 private:
  JsonParser() { }
  ~JsonParser() { }

  // Parse a string containing a single JSON value.
  Handle<Object> ParseJson(Handle<String> script, UC16CharacterStream* source);
  // Parse a single JSON value from input (grammar production JSONValue).
  // A JSON value is either a (double-quoted) string literal, a number literal,
  // one of "true", "false", or "null", or an object or array literal.
  Handle<Object> ParseJsonValue();
  // Parse a JSON object literal (grammar production JSONObject).
  // An object literal is a squiggly-braced and comma separated sequence
  // (possibly empty) of key/value pairs, where the key is a JSON string
  // literal, the value is a JSON value, and the two are separated by a colon.
  // A JSON array dosn't allow numbers and identifiers as keys, like a
  // JavaScript array.
  Handle<Object> ParseJsonObject();
  // Parses a JSON array literal (grammar production JSONArray). An array
  // literal is a square-bracketed and comma separated sequence (possibly empty)
  // of JSON values.
  // A JSON array doesn't allow leaving out values from the sequence, nor does
  // it allow a terminal comma, like a JavaScript array does.
  Handle<Object> ParseJsonArray();

  // Mark that a parsing error has happened at the current token, and
  // return a null handle. Primarily for readability.
  Handle<Object> ReportUnexpectedToken() { return Handle<Object>::null(); }
  // Converts the currently parsed literal to a JavaScript String.
  Handle<String> GetString();

  JsonScanner scanner_;
  bool stack_overflow_;
};
} }  // namespace v8::internal

#endif  // V8_PARSER_H_
