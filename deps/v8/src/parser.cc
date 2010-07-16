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

#include "v8.h"

#include "api.h"
#include "ast.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "compiler.h"
#include "messages.h"
#include "parser.h"
#include "platform.h"
#include "runtime.h"
#include "scopeinfo.h"
#include "scopes.h"
#include "string-stream.h"

#include "ast-inl.h"
#include "jump-target-inl.h"

namespace v8 {
namespace internal {

class ParserFactory;
class ParserLog;
class TemporaryScope;
class Target;

template <typename T> class ZoneListWrapper;


// PositionStack is used for on-stack allocation of token positions for
// new expressions. Please look at ParseNewExpression.

class PositionStack  {
 public:
  explicit PositionStack(bool* ok) : top_(NULL), ok_(ok) {}
  ~PositionStack() { ASSERT(!*ok_ || is_empty()); }

  class Element  {
   public:
    Element(PositionStack* stack, int value) {
      previous_ = stack->top();
      value_ = value;
      stack->set_top(this);
    }

   private:
    Element* previous() { return previous_; }
    int value() { return value_; }
    friend class PositionStack;
    Element* previous_;
    int value_;
  };

  bool is_empty() { return top_ == NULL; }
  int pop() {
    ASSERT(!is_empty());
    int result = top_->value();
    top_ = top_->previous();
    return result;
  }

 private:
  Element* top() { return top_; }
  void set_top(Element* value) { top_ = value; }
  Element* top_;
  bool* ok_;
};


class Parser {
 public:
  Parser(Handle<Script> script, bool allow_natives_syntax,
         v8::Extension* extension, ParserMode is_pre_parsing,
         ParserFactory* factory, ParserLog* log, ScriptDataImpl* pre_data);
  virtual ~Parser() { }

  // Pre-parse the program from the character stream; returns true on
  // success, false if a stack-overflow happened during parsing.
  bool PreParseProgram(Handle<String> source, unibrow::CharacterStream* stream);

  void ReportMessage(const char* message, Vector<const char*> args);
  virtual void ReportMessageAt(Scanner::Location loc,
                               const char* message,
                               Vector<const char*> args) = 0;


  // Returns NULL if parsing failed.
  FunctionLiteral* ParseProgram(Handle<String> source,
                                bool in_global_context);
  FunctionLiteral* ParseLazy(Handle<String> source,
                             Handle<String> name,
                             int start_position,
                             int end_position,
                             bool is_expression);
  FunctionLiteral* ParseJson(Handle<String> source);

  // The minimum number of contiguous assignment that will
  // be treated as an initialization block. Benchmarks show that
  // the overhead exceeds the savings below this limit.
  static const int kMinInitializationBlock = 3;

 protected:

  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

  // Report syntax error
  void ReportUnexpectedToken(Token::Value token);
  void ReportInvalidPreparseData(Handle<String> name, bool* ok);

  Handle<Script> script_;
  Scanner scanner_;

  Scope* top_scope_;
  int with_nesting_level_;

  TemporaryScope* temp_scope_;
  Mode mode_;

  Target* target_stack_;  // for break, continue statements
  bool allow_natives_syntax_;
  v8::Extension* extension_;
  ParserFactory* factory_;
  ParserLog* log_;
  bool is_pre_parsing_;
  ScriptDataImpl* pre_data_;
  bool seen_loop_stmt_;  // Used for inner loop detection.

  bool inside_with() const  { return with_nesting_level_ > 0; }
  ParserFactory* factory() const  { return factory_; }
  ParserLog* log() const { return log_; }
  Scanner& scanner()  { return scanner_; }
  Mode mode() const  { return mode_; }
  ScriptDataImpl* pre_data() const  { return pre_data_; }

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  void* ParseSourceElements(ZoneListWrapper<Statement>* processor,
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
  Expression* ParseRegExpLiteral(bool seen_equal, bool* ok);

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

  INLINE(Token::Value peek()) { return scanner_.peek(); }
  INLINE(Token::Value Next()) { return scanner_.Next(); }
  INLINE(void Consume(Token::Value token));
  void Expect(Token::Value token, bool* ok);
  bool Check(Token::Value token);
  void ExpectSemicolon(bool* ok);

  // Get odd-ball literals.
  Literal* GetLiteralUndefined();
  Literal* GetLiteralTheHole();
  Literal* GetLiteralNumber(double value);

  Handle<String> ParseIdentifier(bool* ok);
  Handle<String> ParseIdentifierOrGetOrSet(bool* is_get,
                                           bool* is_set,
                                           bool* ok);

  // Parser support
  virtual VariableProxy* Declare(Handle<String> name, Variable::Mode mode,
                                 FunctionLiteral* fun,
                                 bool resolve,
                                 bool* ok) = 0;

  bool TargetStackContainsLabel(Handle<String> label);
  BreakableStatement* LookupBreakTarget(Handle<String> label, bool* ok);
  IterationStatement* LookupContinueTarget(Handle<String> label, bool* ok);

  void RegisterTargetUse(BreakTarget* target, Target* stop);

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

  // JSON is a subset of JavaScript, as specified in, e.g., the ECMAScript 5
  // specification section 15.12.1 (and appendix A.8).
  // The grammar is given section 15.12.1.2 (and appendix A.8.2).

  // Parse JSON input as a single JSON value.
  Expression* ParseJson(bool* ok);

  // Parse a single JSON value from input (grammar production JSONValue).
  // A JSON value is either a (double-quoted) string literal, a number literal,
  // one of "true", "false", or "null", or an object or array literal.
  Expression* ParseJsonValue(bool* ok);
  // Parse a JSON object literal (grammar production JSONObject).
  // An object literal is a squiggly-braced and comma separated sequence
  // (possibly empty) of key/value pairs, where the key is a JSON string
  // literal, the value is a JSON value, and the two are spearated by a colon.
  // A JavaScript object also allows numbers and identifiers as keys.
  Expression* ParseJsonObject(bool* ok);
  // Parses a JSON array literal (grammar production JSONArray). An array
  // literal is a square-bracketed and comma separated sequence (possibly empty)
  // of JSON values.
  // A JavaScript array allows leaving out values from the sequence.
  Expression* ParseJsonArray(bool* ok);

  friend class Target;
  friend class TargetScope;
  friend class LexicalScope;
  friend class TemporaryScope;
};


template <typename T, int initial_size>
class BufferedZoneList {
 public:

  BufferedZoneList() :
    list_(NULL), last_(NULL) {}

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
    if (list_ != NULL && list_->length() > 0)
      last_ = list_->RemoveLast();
    else
      last_ = NULL;
    return result;
  }

  T* Get(int i) {
    ASSERT(0 <= i && i < length());
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


RegExpBuilder::RegExpBuilder()
  : pending_empty_(false),
    characters_(NULL),
    terms_(),
    alternatives_()
#ifdef DEBUG
  , last_added_(ADD_NONE)
#endif
  {}


void RegExpBuilder::FlushCharacters() {
  pending_empty_ = false;
  if (characters_ != NULL) {
    RegExpTree* atom = new RegExpAtom(characters_->ToConstVector());
    characters_ = NULL;
    text_.Add(atom);
    LAST(ADD_ATOM);
  }
}


void RegExpBuilder::FlushText() {
  FlushCharacters();
  int num_text = text_.length();
  if (num_text == 0) {
    return;
  } else if (num_text == 1) {
    terms_.Add(text_.last());
  } else {
    RegExpText* text = new RegExpText();
    for (int i = 0; i < num_text; i++)
      text_.Get(i)->AppendToText(text);
    terms_.Add(text);
  }
  text_.Clear();
}


void RegExpBuilder::AddCharacter(uc16 c) {
  pending_empty_ = false;
  if (characters_ == NULL) {
    characters_ = new ZoneList<uc16>(4);
  }
  characters_->Add(c);
  LAST(ADD_CHAR);
}


void RegExpBuilder::AddEmpty() {
  pending_empty_ = true;
}


void RegExpBuilder::AddAtom(RegExpTree* term) {
  if (term->IsEmpty()) {
    AddEmpty();
    return;
  }
  if (term->IsTextElement()) {
    FlushCharacters();
    text_.Add(term);
  } else {
    FlushText();
    terms_.Add(term);
  }
  LAST(ADD_ATOM);
}


void RegExpBuilder::AddAssertion(RegExpTree* assert) {
  FlushText();
  terms_.Add(assert);
  LAST(ADD_ASSERT);
}


void RegExpBuilder::NewAlternative() {
  FlushTerms();
}


void RegExpBuilder::FlushTerms() {
  FlushText();
  int num_terms = terms_.length();
  RegExpTree* alternative;
  if (num_terms == 0) {
    alternative = RegExpEmpty::GetInstance();
  } else if (num_terms == 1) {
    alternative = terms_.last();
  } else {
    alternative = new RegExpAlternative(terms_.GetList());
  }
  alternatives_.Add(alternative);
  terms_.Clear();
  LAST(ADD_NONE);
}


RegExpTree* RegExpBuilder::ToRegExp() {
  FlushTerms();
  int num_alternatives = alternatives_.length();
  if (num_alternatives == 0) {
    return RegExpEmpty::GetInstance();
  }
  if (num_alternatives == 1) {
    return alternatives_.last();
  }
  return new RegExpDisjunction(alternatives_.GetList());
}


void RegExpBuilder::AddQuantifierToAtom(int min,
                                        int max,
                                        RegExpQuantifier::Type type) {
  if (pending_empty_) {
    pending_empty_ = false;
    return;
  }
  RegExpTree* atom;
  if (characters_ != NULL) {
    ASSERT(last_added_ == ADD_CHAR);
    // Last atom was character.
    Vector<const uc16> char_vector = characters_->ToConstVector();
    int num_chars = char_vector.length();
    if (num_chars > 1) {
      Vector<const uc16> prefix = char_vector.SubVector(0, num_chars - 1);
      text_.Add(new RegExpAtom(prefix));
      char_vector = char_vector.SubVector(num_chars - 1, num_chars);
    }
    characters_ = NULL;
    atom = new RegExpAtom(char_vector);
    FlushText();
  } else if (text_.length() > 0) {
    ASSERT(last_added_ == ADD_ATOM);
    atom = text_.RemoveLast();
    FlushText();
  } else if (terms_.length() > 0) {
    ASSERT(last_added_ == ADD_ATOM);
    atom = terms_.RemoveLast();
    if (atom->max_match() == 0) {
      // Guaranteed to only match an empty string.
      LAST(ADD_TERM);
      if (min == 0) {
        return;
      }
      terms_.Add(atom);
      return;
    }
  } else {
    // Only call immediately after adding an atom or character!
    UNREACHABLE();
    return;
  }
  terms_.Add(new RegExpQuantifier(min, max, type, atom));
  LAST(ADD_TERM);
}


class RegExpParser {
 public:
  RegExpParser(FlatStringReader* in,
               Handle<String>* error,
               bool multiline_mode);
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

  uc32 ParseControlLetterEscape();
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
  uc32 current_;
  bool has_more_;
  bool multiline_;
  int next_pos_;
  FlatStringReader* in_;
  Handle<String>* error_;
  bool simple_;
  bool contains_anchor_;
  ZoneList<RegExpCapture*>* captures_;
  bool is_scanned_for_captures_;
  // The capture count is only valid after we have scanned for captures.
  int capture_count_;
  bool failed_;
};


// A temporary scope stores information during parsing, just like
// a plain scope.  However, temporary scopes are not kept around
// after parsing or referenced by syntax trees so they can be stack-
// allocated and hence used by the pre-parser.
class TemporaryScope BASE_EMBEDDED {
 public:
  explicit TemporaryScope(Parser* parser);
  ~TemporaryScope();

  int NextMaterializedLiteralIndex() {
    int next_index =
        materialized_literal_count_ + JSFunction::kLiteralsPrefixSize;
    materialized_literal_count_++;
    return next_index;
  }
  int materialized_literal_count() { return materialized_literal_count_; }

  void SetThisPropertyAssignmentInfo(
      bool only_simple_this_property_assignments,
      Handle<FixedArray> this_property_assignments) {
    only_simple_this_property_assignments_ =
        only_simple_this_property_assignments;
    this_property_assignments_ = this_property_assignments;
  }
  bool only_simple_this_property_assignments() {
    return only_simple_this_property_assignments_;
  }
  Handle<FixedArray> this_property_assignments() {
    return this_property_assignments_;
  }

  void AddProperty() { expected_property_count_++; }
  int expected_property_count() { return expected_property_count_; }
 private:
  // Captures the number of literals that need materialization in the
  // function.  Includes regexp literals, and boilerplate for object
  // and array literals.
  int materialized_literal_count_;

  // Properties count estimation.
  int expected_property_count_;

  bool only_simple_this_property_assignments_;
  Handle<FixedArray> this_property_assignments_;

  // Bookkeeping
  Parser* parser_;
  TemporaryScope* parent_;

  friend class Parser;
};


TemporaryScope::TemporaryScope(Parser* parser)
  : materialized_literal_count_(0),
    expected_property_count_(0),
    only_simple_this_property_assignments_(false),
    this_property_assignments_(Factory::empty_fixed_array()),
    parser_(parser),
    parent_(parser->temp_scope_) {
  parser->temp_scope_ = this;
}


TemporaryScope::~TemporaryScope() {
  parser_->temp_scope_ = parent_;
}


// A zone list wrapper lets code either access a access a zone list
// or appear to do so while actually ignoring all operations.
template <typename T>
class ZoneListWrapper {
 public:
  ZoneListWrapper() : list_(NULL) { }
  explicit ZoneListWrapper(int size) : list_(new ZoneList<T*>(size)) { }
  void Add(T* that) { if (list_) list_->Add(that); }
  int length() { return list_->length(); }
  ZoneList<T*>* elements() { return list_; }
  T* at(int index) { return list_->at(index); }
 private:
  ZoneList<T*>* list_;
};


// Allocation macro that should be used to allocate objects that must
// only be allocated in real parsing mode.  Note that in preparse mode
// not only is the syntax tree not created but the constructor
// arguments are not evaluated.
#define NEW(expr) (is_pre_parsing_ ? NULL : new expr)


class ParserFactory BASE_EMBEDDED {
 public:
  explicit ParserFactory(bool is_pre_parsing) :
      is_pre_parsing_(is_pre_parsing) { }

  virtual ~ParserFactory() { }

  virtual Scope* NewScope(Scope* parent, Scope::Type type, bool inside_with);

  virtual Handle<String> LookupSymbol(const char* string, int length) {
    return Handle<String>();
  }

  virtual Handle<String> EmptySymbol() {
    return Handle<String>();
  }

  virtual Expression* NewProperty(Expression* obj, Expression* key, int pos) {
    if (obj == VariableProxySentinel::this_proxy()) {
      return Property::this_property();
    } else {
      return ValidLeftHandSideSentinel::instance();
    }
  }

  virtual Expression* NewCall(Expression* expression,
                              ZoneList<Expression*>* arguments,
                              int pos) {
    return Call::sentinel();
  }

  virtual Statement* EmptyStatement() {
    return NULL;
  }

  template <typename T> ZoneListWrapper<T> NewList(int size) {
    return is_pre_parsing_ ? ZoneListWrapper<T>() : ZoneListWrapper<T>(size);
  }

 private:
  bool is_pre_parsing_;
};


class ParserLog BASE_EMBEDDED {
 public:
  virtual ~ParserLog() { }

  // Records the occurrence of a function.  The returned object is
  // only guaranteed to be valid until the next function has been
  // logged.
  virtual FunctionEntry LogFunction(int start) { return FunctionEntry(); }

  virtual void LogError() { }
};


class AstBuildingParserFactory : public ParserFactory {
 public:
  AstBuildingParserFactory() : ParserFactory(false) { }

  virtual Scope* NewScope(Scope* parent, Scope::Type type, bool inside_with);

  virtual Handle<String> LookupSymbol(const char* string, int length) {
    return Factory::LookupSymbol(Vector<const char>(string, length));
  }

  virtual Handle<String> EmptySymbol() {
    return Factory::empty_symbol();
  }

  virtual Expression* NewProperty(Expression* obj, Expression* key, int pos) {
    return new Property(obj, key, pos);
  }

  virtual Expression* NewCall(Expression* expression,
                              ZoneList<Expression*>* arguments,
                              int pos) {
    return new Call(expression, arguments, pos);
  }

  virtual Statement* EmptyStatement();
};


class ParserRecorder: public ParserLog {
 public:
  ParserRecorder();
  virtual FunctionEntry LogFunction(int start);
  virtual void LogError() { }
  virtual void LogMessage(Scanner::Location loc,
                          const char* message,
                          Vector<const char*> args);
  void WriteString(Vector<const char> str);
  static const char* ReadString(unsigned* start, int* chars);
  List<unsigned>* store() { return &store_; }
 private:
  bool has_error_;
  List<unsigned> store_;
};


FunctionEntry ScriptDataImpl::GetFunctionEnd(int start) {
  if (nth(last_entry_).start_pos() > start) {
    // If the last entry we looked up is higher than what we're
    // looking for then it's useless and we reset it.
    last_entry_ = 0;
  }
  for (int i = last_entry_; i < EntryCount(); i++) {
    FunctionEntry entry = nth(i);
    if (entry.start_pos() == start) {
      last_entry_ = i;
      return entry;
    }
  }
  return FunctionEntry();
}


bool ScriptDataImpl::SanityCheck() {
  if (store_.length() < static_cast<int>(ScriptDataImpl::kHeaderSize))
    return false;
  if (magic() != ScriptDataImpl::kMagicNumber)
    return false;
  if (version() != ScriptDataImpl::kCurrentVersion)
    return false;
  return true;
}


int ScriptDataImpl::EntryCount() {
  return (store_.length() - kHeaderSize) / FunctionEntry::kSize;
}


FunctionEntry ScriptDataImpl::nth(int n) {
  int offset = kHeaderSize + n * FunctionEntry::kSize;
  return FunctionEntry(Vector<unsigned>(store_.start() + offset,
                                        FunctionEntry::kSize));
}


ParserRecorder::ParserRecorder()
  : has_error_(false), store_(4) {
  Vector<unsigned> preamble = store()->AddBlock(0, ScriptDataImpl::kHeaderSize);
  preamble[ScriptDataImpl::kMagicOffset] = ScriptDataImpl::kMagicNumber;
  preamble[ScriptDataImpl::kVersionOffset] = ScriptDataImpl::kCurrentVersion;
  preamble[ScriptDataImpl::kHasErrorOffset] = false;
}


void ParserRecorder::WriteString(Vector<const char> str) {
  store()->Add(str.length());
  for (int i = 0; i < str.length(); i++)
    store()->Add(str[i]);
}


const char* ParserRecorder::ReadString(unsigned* start, int* chars) {
  int length = start[0];
  char* result = NewArray<char>(length + 1);
  for (int i = 0; i < length; i++)
    result[i] = start[i + 1];
  result[length] = '\0';
  if (chars != NULL) *chars = length;
  return result;
}


void ParserRecorder::LogMessage(Scanner::Location loc, const char* message,
                                Vector<const char*> args) {
  if (has_error_) return;
  store()->Rewind(ScriptDataImpl::kHeaderSize);
  store()->at(ScriptDataImpl::kHasErrorOffset) = true;
  store()->Add(loc.beg_pos);
  store()->Add(loc.end_pos);
  store()->Add(args.length());
  WriteString(CStrVector(message));
  for (int i = 0; i < args.length(); i++)
    WriteString(CStrVector(args[i]));
}


Scanner::Location ScriptDataImpl::MessageLocation() {
  int beg_pos = Read(0);
  int end_pos = Read(1);
  return Scanner::Location(beg_pos, end_pos);
}


const char* ScriptDataImpl::BuildMessage() {
  unsigned* start = ReadAddress(3);
  return ParserRecorder::ReadString(start, NULL);
}


Vector<const char*> ScriptDataImpl::BuildArgs() {
  int arg_count = Read(2);
  const char** array = NewArray<const char*>(arg_count);
  int pos = ScriptDataImpl::kHeaderSize + Read(3);
  for (int i = 0; i < arg_count; i++) {
    int count = 0;
    array[i] = ParserRecorder::ReadString(ReadAddress(pos), &count);
    pos += count + 1;
  }
  return Vector<const char*>(array, arg_count);
}


unsigned ScriptDataImpl::Read(int position) {
  return store_[ScriptDataImpl::kHeaderSize + position];
}


unsigned* ScriptDataImpl::ReadAddress(int position) {
  return &store_[ScriptDataImpl::kHeaderSize + position];
}


FunctionEntry ParserRecorder::LogFunction(int start) {
  if (has_error_) return FunctionEntry();
  FunctionEntry result(store()->AddBlock(0, FunctionEntry::kSize));
  result.set_start_pos(start);
  return result;
}


class AstBuildingParser : public Parser {
 public:
  AstBuildingParser(Handle<Script> script, bool allow_natives_syntax,
                    v8::Extension* extension, ScriptDataImpl* pre_data)
      : Parser(script, allow_natives_syntax, extension, PARSE,
               factory(), log(), pre_data) { }
  virtual void ReportMessageAt(Scanner::Location loc, const char* message,
                               Vector<const char*> args);
  virtual VariableProxy* Declare(Handle<String> name, Variable::Mode mode,
                                 FunctionLiteral* fun, bool resolve, bool* ok);
  AstBuildingParserFactory* factory() { return &factory_; }
  ParserLog* log() { return &log_; }

 private:
  ParserLog log_;
  AstBuildingParserFactory factory_;
};


class PreParser : public Parser {
 public:
  PreParser(Handle<Script> script, bool allow_natives_syntax,
            v8::Extension* extension)
      : Parser(script, allow_natives_syntax, extension, PREPARSE,
               factory(), recorder(), NULL),
        factory_(true) { }
  virtual void ReportMessageAt(Scanner::Location loc, const char* message,
                               Vector<const char*> args);
  virtual VariableProxy* Declare(Handle<String> name, Variable::Mode mode,
                                 FunctionLiteral* fun, bool resolve, bool* ok);
  ParserFactory* factory() { return &factory_; }
  ParserRecorder* recorder() { return &recorder_; }

 private:
  ParserRecorder recorder_;
  ParserFactory factory_;
};


Scope* AstBuildingParserFactory::NewScope(Scope* parent, Scope::Type type,
                                          bool inside_with) {
  Scope* result = new Scope(parent, type);
  result->Initialize(inside_with);
  return result;
}


Statement* AstBuildingParserFactory::EmptyStatement() {
  // Use a statically allocated empty statement singleton to avoid
  // allocating lots and lots of empty statements.
  static v8::internal::EmptyStatement empty;
  return &empty;
}


Scope* ParserFactory::NewScope(Scope* parent, Scope::Type type,
                               bool inside_with) {
  ASSERT(parent != NULL);
  parent->type_ = type;
  return parent;
}


VariableProxy* PreParser::Declare(Handle<String> name, Variable::Mode mode,
                                  FunctionLiteral* fun, bool resolve,
                                  bool* ok) {
  return NULL;
}



// ----------------------------------------------------------------------------
// Target is a support class to facilitate manipulation of the
// Parser's target_stack_ (the stack of potential 'break' and
// 'continue' statement targets). Upon construction, a new target is
// added; it is removed upon destruction.

class Target BASE_EMBEDDED {
 public:
  Target(Parser* parser, AstNode* node)
      : parser_(parser), node_(node), previous_(parser_->target_stack_) {
    parser_->target_stack_ = this;
  }

  ~Target() {
    parser_->target_stack_ = previous_;
  }

  Target* previous() { return previous_; }
  AstNode* node() { return node_; }

 private:
  Parser* parser_;
  AstNode* node_;
  Target* previous_;
};


class TargetScope BASE_EMBEDDED {
 public:
  explicit TargetScope(Parser* parser)
      : parser_(parser), previous_(parser->target_stack_) {
    parser->target_stack_ = NULL;
  }

  ~TargetScope() {
    parser_->target_stack_ = previous_;
  }

 private:
  Parser* parser_;
  Target* previous_;
};


// ----------------------------------------------------------------------------
// LexicalScope is a support class to facilitate manipulation of the
// Parser's scope stack. The constructor sets the parser's top scope
// to the incoming scope, and the destructor resets it.

class LexicalScope BASE_EMBEDDED {
 public:
  LexicalScope(Parser* parser, Scope* scope)
    : parser_(parser),
      prev_scope_(parser->top_scope_),
      prev_level_(parser->with_nesting_level_) {
    parser_->top_scope_ = scope;
    parser_->with_nesting_level_ = 0;
  }

  ~LexicalScope() {
    parser_->top_scope_ = prev_scope_;
    parser_->with_nesting_level_ = prev_level_;
  }

 private:
  Parser* parser_;
  Scope* prev_scope_;
  int prev_level_;
};


// ----------------------------------------------------------------------------
// The CHECK_OK macro is a convenient macro to enforce error
// handling for functions that may fail (by returning !*ok).
//
// CAUTION: This macro appends extra statements after a call,
// thus it must never be used where only a single statement
// is correct (e.g. an if statement branch w/o braces)!

#define CHECK_OK  ok);   \
  if (!*ok) return NULL; \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

#define CHECK_FAILED  /**/);   \
  if (failed_) return NULL; \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

// ----------------------------------------------------------------------------
// Implementation of Parser

Parser::Parser(Handle<Script> script,
               bool allow_natives_syntax,
               v8::Extension* extension,
               ParserMode is_pre_parsing,
               ParserFactory* factory,
               ParserLog* log,
               ScriptDataImpl* pre_data)
    : script_(script),
      scanner_(is_pre_parsing),
      top_scope_(NULL),
      with_nesting_level_(0),
      temp_scope_(NULL),
      target_stack_(NULL),
      allow_natives_syntax_(allow_natives_syntax),
      extension_(extension),
      factory_(factory),
      log_(log),
      is_pre_parsing_(is_pre_parsing == PREPARSE),
      pre_data_(pre_data),
      seen_loop_stmt_(false) {
}


bool Parser::PreParseProgram(Handle<String> source,
                             unibrow::CharacterStream* stream) {
  HistogramTimerScope timer(&Counters::pre_parse);
  AssertNoZoneAllocation assert_no_zone_allocation;
  AssertNoAllocation assert_no_allocation;
  NoHandleAllocation no_handle_allocation;
  scanner_.Initialize(source, stream, JAVASCRIPT);
  ASSERT(target_stack_ == NULL);
  mode_ = PARSE_EAGERLY;
  DummyScope top_scope;
  LexicalScope scope(this, &top_scope);
  TemporaryScope temp_scope(this);
  ZoneListWrapper<Statement> processor;
  bool ok = true;
  ParseSourceElements(&processor, Token::EOS, &ok);
  return !scanner().stack_overflow();
}


FunctionLiteral* Parser::ParseProgram(Handle<String> source,
                                      bool in_global_context) {
  CompilationZoneScope zone_scope(DONT_DELETE_ON_EXIT);

  HistogramTimerScope timer(&Counters::parse);
  Counters::total_parse_size.Increment(source->length());

  // Initialize parser state.
  source->TryFlatten();
  scanner_.Initialize(source, JAVASCRIPT);
  ASSERT(target_stack_ == NULL);

  // Compute the parsing mode.
  mode_ = FLAG_lazy ? PARSE_LAZILY : PARSE_EAGERLY;
  if (allow_natives_syntax_ || extension_ != NULL) mode_ = PARSE_EAGERLY;

  Scope::Type type =
    in_global_context
      ? Scope::GLOBAL_SCOPE
      : Scope::EVAL_SCOPE;
  Handle<String> no_name = factory()->EmptySymbol();

  FunctionLiteral* result = NULL;
  { Scope* scope = factory()->NewScope(top_scope_, type, inside_with());
    LexicalScope lexical_scope(this, scope);
    TemporaryScope temp_scope(this);
    ZoneListWrapper<Statement> body(16);
    bool ok = true;
    ParseSourceElements(&body, Token::EOS, &ok);
    if (ok) {
      result = NEW(FunctionLiteral(
          no_name,
          top_scope_,
          body.elements(),
          temp_scope.materialized_literal_count(),
          temp_scope.expected_property_count(),
          temp_scope.only_simple_this_property_assignments(),
          temp_scope.this_property_assignments(),
          0,
          0,
          source->length(),
          false));
    } else if (scanner().stack_overflow()) {
      Top::StackOverflow();
    }
  }

  // Make sure the target stack is empty.
  ASSERT(target_stack_ == NULL);

  // If there was a syntax error we have to get rid of the AST
  // and it is not safe to do so before the scope has been deleted.
  if (result == NULL) zone_scope.DeleteOnExit();
  return result;
}


FunctionLiteral* Parser::ParseLazy(Handle<String> source,
                                   Handle<String> name,
                                   int start_position,
                                   int end_position,
                                   bool is_expression) {
  CompilationZoneScope zone_scope(DONT_DELETE_ON_EXIT);
  HistogramTimerScope timer(&Counters::parse_lazy);
  Counters::total_parse_size.Increment(source->length());

  // Initialize parser state.
  source->TryFlatten();
  scanner_.Initialize(source, start_position, end_position, JAVASCRIPT);
  ASSERT(target_stack_ == NULL);
  mode_ = PARSE_EAGERLY;

  // Place holder for the result.
  FunctionLiteral* result = NULL;

  {
    // Parse the function literal.
    Handle<String> no_name = factory()->EmptySymbol();
    Scope* scope =
        factory()->NewScope(top_scope_, Scope::GLOBAL_SCOPE, inside_with());
    LexicalScope lexical_scope(this, scope);
    TemporaryScope temp_scope(this);

    FunctionLiteralType type = is_expression ? EXPRESSION : DECLARATION;
    bool ok = true;
    result = ParseFunctionLiteral(name, RelocInfo::kNoPosition, type, &ok);
    // Make sure the results agree.
    ASSERT(ok == (result != NULL));
    // The only errors should be stack overflows.
    ASSERT(ok || scanner_.stack_overflow());
  }

  // Make sure the target stack is empty.
  ASSERT(target_stack_ == NULL);

  // If there was a stack overflow we have to get rid of AST and it is
  // not safe to do before scope has been deleted.
  if (result == NULL) {
    Top::StackOverflow();
    zone_scope.DeleteOnExit();
  }
  return result;
}

FunctionLiteral* Parser::ParseJson(Handle<String> source) {
  CompilationZoneScope zone_scope(DONT_DELETE_ON_EXIT);

  HistogramTimerScope timer(&Counters::parse);
  Counters::total_parse_size.Increment(source->length());

  // Initialize parser state.
  source->TryFlatten(TENURED);
  scanner_.Initialize(source, JSON);
  ASSERT(target_stack_ == NULL);

  FunctionLiteral* result = NULL;
  Handle<String> no_name = factory()->EmptySymbol();

  {
    Scope* scope = factory()->NewScope(top_scope_, Scope::GLOBAL_SCOPE, false);
    LexicalScope lexical_scope(this, scope);
    TemporaryScope temp_scope(this);
    bool ok = true;
    Expression* expression = ParseJson(&ok);
    if (ok) {
      ZoneListWrapper<Statement> statement = factory()->NewList<Statement>(1);
      statement.Add(new ExpressionStatement(expression));
      result = NEW(FunctionLiteral(
          no_name,
          top_scope_,
          statement.elements(),
          temp_scope.materialized_literal_count(),
          temp_scope.expected_property_count(),
          temp_scope.only_simple_this_property_assignments(),
          temp_scope.this_property_assignments(),
          0,
          0,
          source->length(),
          false));
    } else if (scanner().stack_overflow()) {
      Top::StackOverflow();
    }
  }

  // Make sure the target stack is empty.
  ASSERT(target_stack_ == NULL);

  // If there was a syntax error we have to get rid of the AST
  // and it is not safe to do so before the scope has been deleted.
  if (result == NULL) zone_scope.DeleteOnExit();
  return result;
}

void Parser::ReportMessage(const char* type, Vector<const char*> args) {
  Scanner::Location source_location = scanner_.location();
  ReportMessageAt(source_location, type, args);
}


void AstBuildingParser::ReportMessageAt(Scanner::Location source_location,
                                        const char* type,
                                        Vector<const char*> args) {
  MessageLocation location(script_,
                           source_location.beg_pos, source_location.end_pos);
  Handle<JSArray> array = Factory::NewJSArray(args.length());
  for (int i = 0; i < args.length(); i++) {
    SetElement(array, i, Factory::NewStringFromUtf8(CStrVector(args[i])));
  }
  Handle<Object> result = Factory::NewSyntaxError(type, array);
  Top::Throw(*result, &location);
}


void PreParser::ReportMessageAt(Scanner::Location source_location,
                                const char* type,
                                Vector<const char*> args) {
  recorder()->LogMessage(source_location, type, args);
}


// Base class containing common code for the different finder classes used by
// the parser.
class ParserFinder {
 protected:
  ParserFinder() {}
  static Assignment* AsAssignment(Statement* stat) {
    if (stat == NULL) return NULL;
    ExpressionStatement* exp_stat = stat->AsExpressionStatement();
    if (exp_stat == NULL) return NULL;
    return exp_stat->expression()->AsAssignment();
  }
};


// An InitializationBlockFinder finds and marks sequences of statements of the
// form expr.a = ...; expr.b = ...; etc.
class InitializationBlockFinder : public ParserFinder {
 public:
  InitializationBlockFinder()
    : first_in_block_(NULL), last_in_block_(NULL), block_size_(0) {}

  ~InitializationBlockFinder() {
    if (InBlock()) EndBlock();
  }

  void Update(Statement* stat) {
    Assignment* assignment = AsAssignment(stat);
    if (InBlock()) {
      if (BlockContinues(assignment)) {
        UpdateBlock(assignment);
      } else {
        EndBlock();
      }
    }
    if (!InBlock() && (assignment != NULL) &&
        (assignment->op() == Token::ASSIGN)) {
      StartBlock(assignment);
    }
  }

 private:
  // Returns true if the expressions appear to denote the same object.
  // In the context of initialization blocks, we only consider expressions
  // of the form 'expr.x' or expr["x"].
  static bool SameObject(Expression* e1, Expression* e2) {
    VariableProxy* v1 = e1->AsVariableProxy();
    VariableProxy* v2 = e2->AsVariableProxy();
    if (v1 != NULL && v2 != NULL) {
      return v1->name()->Equals(*v2->name());
    }
    Property* p1 = e1->AsProperty();
    Property* p2 = e2->AsProperty();
    if ((p1 == NULL) || (p2 == NULL)) return false;
    Literal* key1 = p1->key()->AsLiteral();
    Literal* key2 = p2->key()->AsLiteral();
    if ((key1 == NULL) || (key2 == NULL)) return false;
    if (!key1->handle()->IsString() || !key2->handle()->IsString()) {
      return false;
    }
    String* name1 = String::cast(*key1->handle());
    String* name2 = String::cast(*key2->handle());
    if (!name1->Equals(name2)) return false;
    return SameObject(p1->obj(), p2->obj());
  }

  // Returns true if the expressions appear to denote different properties
  // of the same object.
  static bool PropertyOfSameObject(Expression* e1, Expression* e2) {
    Property* p1 = e1->AsProperty();
    Property* p2 = e2->AsProperty();
    if ((p1 == NULL) || (p2 == NULL)) return false;
    return SameObject(p1->obj(), p2->obj());
  }

  bool BlockContinues(Assignment* assignment) {
    if ((assignment == NULL) || (first_in_block_ == NULL)) return false;
    if (assignment->op() != Token::ASSIGN) return false;
    return PropertyOfSameObject(first_in_block_->target(),
                                assignment->target());
  }

  void StartBlock(Assignment* assignment) {
    first_in_block_ = assignment;
    last_in_block_ = assignment;
    block_size_ = 1;
  }

  void UpdateBlock(Assignment* assignment) {
    last_in_block_ = assignment;
    ++block_size_;
  }

  void EndBlock() {
    if (block_size_ >= Parser::kMinInitializationBlock) {
      first_in_block_->mark_block_start();
      last_in_block_->mark_block_end();
    }
    last_in_block_ = first_in_block_ = NULL;
    block_size_ = 0;
  }

  bool InBlock() { return first_in_block_ != NULL; }

  Assignment* first_in_block_;
  Assignment* last_in_block_;
  int block_size_;

  DISALLOW_COPY_AND_ASSIGN(InitializationBlockFinder);
};


// A ThisNamedPropertyAssigmentFinder finds and marks statements of the form
// this.x = ...;, where x is a named property. It also determines whether a
// function contains only assignments of this type.
class ThisNamedPropertyAssigmentFinder : public ParserFinder {
 public:
  ThisNamedPropertyAssigmentFinder()
      : only_simple_this_property_assignments_(true),
        names_(NULL),
        assigned_arguments_(NULL),
        assigned_constants_(NULL) {}

  void Update(Scope* scope, Statement* stat) {
    // Bail out if function already has property assignment that are
    // not simple this property assignments.
    if (!only_simple_this_property_assignments_) {
      return;
    }

    // Check whether this statement is of the form this.x = ...;
    Assignment* assignment = AsAssignment(stat);
    if (IsThisPropertyAssignment(assignment)) {
      HandleThisPropertyAssignment(scope, assignment);
    } else {
      only_simple_this_property_assignments_ = false;
    }
  }

  // Returns whether only statements of the form this.x = y; where y is either a
  // constant or a function argument was encountered.
  bool only_simple_this_property_assignments() {
    return only_simple_this_property_assignments_;
  }

  // Returns a fixed array containing three elements for each assignment of the
  // form this.x = y;
  Handle<FixedArray> GetThisPropertyAssignments() {
    if (names_ == NULL) {
      return Factory::empty_fixed_array();
    }
    ASSERT(names_ != NULL);
    ASSERT(assigned_arguments_ != NULL);
    ASSERT_EQ(names_->length(), assigned_arguments_->length());
    ASSERT_EQ(names_->length(), assigned_constants_->length());
    Handle<FixedArray> assignments =
        Factory::NewFixedArray(names_->length() * 3);
    for (int i = 0; i < names_->length(); i++) {
      assignments->set(i * 3, *names_->at(i));
      assignments->set(i * 3 + 1, Smi::FromInt(assigned_arguments_->at(i)));
      assignments->set(i * 3 + 2, *assigned_constants_->at(i));
    }
    return assignments;
  }

 private:
  bool IsThisPropertyAssignment(Assignment* assignment) {
    if (assignment != NULL) {
      Property* property = assignment->target()->AsProperty();
      return assignment->op() == Token::ASSIGN
             && property != NULL
             && property->obj()->AsVariableProxy() != NULL
             && property->obj()->AsVariableProxy()->is_this();
    }
    return false;
  }

  void HandleThisPropertyAssignment(Scope* scope, Assignment* assignment) {
    // Check that the property assigned to is a named property, which is not
    // __proto__.
    Property* property = assignment->target()->AsProperty();
    ASSERT(property != NULL);
    Literal* literal = property->key()->AsLiteral();
    uint32_t dummy;
    if (literal != NULL &&
        literal->handle()->IsString() &&
        !String::cast(*(literal->handle()))->Equals(Heap::Proto_symbol()) &&
        !String::cast(*(literal->handle()))->AsArrayIndex(&dummy)) {
      Handle<String> key = Handle<String>::cast(literal->handle());

      // Check whether the value assigned is either a constant or matches the
      // name of one of the arguments to the function.
      if (assignment->value()->AsLiteral() != NULL) {
        // Constant assigned.
        Literal* literal = assignment->value()->AsLiteral();
        AssignmentFromConstant(key, literal->handle());
        return;
      } else if (assignment->value()->AsVariableProxy() != NULL) {
        // Variable assigned.
        Handle<String> name =
            assignment->value()->AsVariableProxy()->name();
        // Check whether the variable assigned matches an argument name.
        for (int i = 0; i < scope->num_parameters(); i++) {
          if (*scope->parameter(i)->name() == *name) {
            // Assigned from function argument.
            AssignmentFromParameter(key, i);
            return;
          }
        }
      }
    }
    // It is not a simple "this.x = value;" assignment with a constant
    // or parameter value.
    AssignmentFromSomethingElse();
  }

  void AssignmentFromParameter(Handle<String> name, int index) {
    EnsureAllocation();
    names_->Add(name);
    assigned_arguments_->Add(index);
    assigned_constants_->Add(Factory::undefined_value());
  }

  void AssignmentFromConstant(Handle<String> name, Handle<Object> value) {
    EnsureAllocation();
    names_->Add(name);
    assigned_arguments_->Add(-1);
    assigned_constants_->Add(value);
  }

  void AssignmentFromSomethingElse() {
    // The this assignment is not a simple one.
    only_simple_this_property_assignments_ = false;
  }

  void EnsureAllocation() {
    if (names_ == NULL) {
      ASSERT(assigned_arguments_ == NULL);
      ASSERT(assigned_constants_ == NULL);
      names_ = new ZoneStringList(4);
      assigned_arguments_ = new ZoneList<int>(4);
      assigned_constants_ = new ZoneObjectList(4);
    }
  }

  bool only_simple_this_property_assignments_;
  ZoneStringList* names_;
  ZoneList<int>* assigned_arguments_;
  ZoneObjectList* assigned_constants_;
};


void* Parser::ParseSourceElements(ZoneListWrapper<Statement>* processor,
                                  int end_token,
                                  bool* ok) {
  // SourceElements ::
  //   (Statement)* <end_token>

  // Allocate a target stack to use for this set of source
  // elements. This way, all scripts and functions get their own
  // target stack thus avoiding illegal breaks and continues across
  // functions.
  TargetScope scope(this);

  ASSERT(processor != NULL);
  InitializationBlockFinder block_finder;
  ThisNamedPropertyAssigmentFinder this_property_assignment_finder;
  while (peek() != end_token) {
    Statement* stat = ParseStatement(NULL, CHECK_OK);
    if (stat == NULL || stat->IsEmpty()) continue;
    // We find and mark the initialization blocks on top level code only.
    // This is because the optimization prevents reuse of the map transitions,
    // so it should be used only for code that will only be run once.
    if (top_scope_->is_global_scope()) {
      block_finder.Update(stat);
    }
    // Find and mark all assignments to named properties in this (this.x =)
    if (top_scope_->is_function_scope()) {
      this_property_assignment_finder.Update(top_scope_, stat);
    }
    processor->Add(stat);
  }

  // Propagate the collected information on this property assignments.
  if (top_scope_->is_function_scope()) {
    bool only_simple_this_property_assignments =
        this_property_assignment_finder.only_simple_this_property_assignments()
        && top_scope_->declarations()->length() == 0;
    if (only_simple_this_property_assignments) {
      temp_scope_->SetThisPropertyAssignmentInfo(
          only_simple_this_property_assignments,
          this_property_assignment_finder.GetThisPropertyAssignments());
    }
  }
  return 0;
}


Statement* Parser::ParseStatement(ZoneStringList* labels, bool* ok) {
  // Statement ::
  //   Block
  //   VariableStatement
  //   EmptyStatement
  //   ExpressionStatement
  //   IfStatement
  //   IterationStatement
  //   ContinueStatement
  //   BreakStatement
  //   ReturnStatement
  //   WithStatement
  //   LabelledStatement
  //   SwitchStatement
  //   ThrowStatement
  //   TryStatement
  //   DebuggerStatement

  // Note: Since labels can only be used by 'break' and 'continue'
  // statements, which themselves are only valid within blocks,
  // iterations or 'switch' statements (i.e., BreakableStatements),
  // labels can be simply ignored in all other cases; except for
  // trivial labeled break statements 'label: break label' which is
  // parsed into an empty statement.

  // Keep the source position of the statement
  int statement_pos = scanner().peek_location().beg_pos;
  Statement* stmt = NULL;
  switch (peek()) {
    case Token::LBRACE:
      return ParseBlock(labels, ok);

    case Token::CONST:  // fall through
    case Token::VAR:
      stmt = ParseVariableStatement(ok);
      break;

    case Token::SEMICOLON:
      Next();
      return factory()->EmptyStatement();

    case Token::IF:
      stmt = ParseIfStatement(labels, ok);
      break;

    case Token::DO:
      stmt = ParseDoWhileStatement(labels, ok);
      break;

    case Token::WHILE:
      stmt = ParseWhileStatement(labels, ok);
      break;

    case Token::FOR:
      stmt = ParseForStatement(labels, ok);
      break;

    case Token::CONTINUE:
      stmt = ParseContinueStatement(ok);
      break;

    case Token::BREAK:
      stmt = ParseBreakStatement(labels, ok);
      break;

    case Token::RETURN:
      stmt = ParseReturnStatement(ok);
      break;

    case Token::WITH:
      stmt = ParseWithStatement(labels, ok);
      break;

    case Token::SWITCH:
      stmt = ParseSwitchStatement(labels, ok);
      break;

    case Token::THROW:
      stmt = ParseThrowStatement(ok);
      break;

    case Token::TRY: {
      // NOTE: It is somewhat complicated to have labels on
      // try-statements. When breaking out of a try-finally statement,
      // one must take great care not to treat it as a
      // fall-through. It is much easier just to wrap the entire
      // try-statement in a statement block and put the labels there
      Block* result = NEW(Block(labels, 1, false));
      Target target(this, result);
      TryStatement* statement = ParseTryStatement(CHECK_OK);
      if (statement) {
        statement->set_statement_pos(statement_pos);
      }
      if (result) result->AddStatement(statement);
      return result;
    }

    case Token::FUNCTION:
      return ParseFunctionDeclaration(ok);

    case Token::NATIVE:
      return ParseNativeDeclaration(ok);

    case Token::DEBUGGER:
      stmt = ParseDebuggerStatement(ok);
      break;

    default:
      stmt = ParseExpressionOrLabelledStatement(labels, ok);
  }

  // Store the source position of the statement
  if (stmt != NULL) stmt->set_statement_pos(statement_pos);
  return stmt;
}


VariableProxy* AstBuildingParser::Declare(Handle<String> name,
                                          Variable::Mode mode,
                                          FunctionLiteral* fun,
                                          bool resolve,
                                          bool* ok) {
  Variable* var = NULL;
  // If we are inside a function, a declaration of a variable
  // is a truly local variable, and the scope of the variable
  // is always the function scope.

  // If a function scope exists, then we can statically declare this
  // variable and also set its mode. In any case, a Declaration node
  // will be added to the scope so that the declaration can be added
  // to the corresponding activation frame at runtime if necessary.
  // For instance declarations inside an eval scope need to be added
  // to the calling function context.
  if (top_scope_->is_function_scope()) {
    // Declare the variable in the function scope.
    var = top_scope_->LocalLookup(name);
    if (var == NULL) {
      // Declare the name.
      var = top_scope_->DeclareLocal(name, mode);
    } else {
      // The name was declared before; check for conflicting
      // re-declarations. If the previous declaration was a const or the
      // current declaration is a const then we have a conflict. There is
      // similar code in runtime.cc in the Declare functions.
      if ((mode == Variable::CONST) || (var->mode() == Variable::CONST)) {
        // We only have vars and consts in declarations.
        ASSERT(var->mode() == Variable::VAR ||
               var->mode() == Variable::CONST);
        const char* type = (var->mode() == Variable::VAR) ? "var" : "const";
        Handle<String> type_string =
            Factory::NewStringFromUtf8(CStrVector(type), TENURED);
        Expression* expression =
            NewThrowTypeError(Factory::redeclaration_symbol(),
                              type_string, name);
        top_scope_->SetIllegalRedeclaration(expression);
      }
    }
  }

  // We add a declaration node for every declaration. The compiler
  // will only generate code if necessary. In particular, declarations
  // for inner local variables that do not represent functions won't
  // result in any generated code.
  //
  // Note that we always add an unresolved proxy even if it's not
  // used, simply because we don't know in this method (w/o extra
  // parameters) if the proxy is needed or not. The proxy will be
  // bound during variable resolution time unless it was pre-bound
  // below.
  //
  // WARNING: This will lead to multiple declaration nodes for the
  // same variable if it is declared several times. This is not a
  // semantic issue as long as we keep the source order, but it may be
  // a performance issue since it may lead to repeated
  // Runtime::DeclareContextSlot() calls.
  VariableProxy* proxy = top_scope_->NewUnresolved(name, inside_with());
  top_scope_->AddDeclaration(NEW(Declaration(proxy, mode, fun)));

  // For global const variables we bind the proxy to a variable.
  if (mode == Variable::CONST && top_scope_->is_global_scope()) {
    ASSERT(resolve);  // should be set by all callers
    Variable::Kind kind = Variable::NORMAL;
    var = NEW(Variable(top_scope_, name, Variable::CONST, true, kind));
  }

  // If requested and we have a local variable, bind the proxy to the variable
  // at parse-time. This is used for functions (and consts) declared inside
  // statements: the corresponding function (or const) variable must be in the
  // function scope and not a statement-local scope, e.g. as provided with a
  // 'with' statement:
  //
  //   with (obj) {
  //     function f() {}
  //   }
  //
  // which is translated into:
  //
  //   with (obj) {
  //     // in this case this is not: 'var f; f = function () {};'
  //     var f = function () {};
  //   }
  //
  // Note that if 'f' is accessed from inside the 'with' statement, it
  // will be allocated in the context (because we must be able to look
  // it up dynamically) but it will also be accessed statically, i.e.,
  // with a context slot index and a context chain length for this
  // initialization code. Thus, inside the 'with' statement, we need
  // both access to the static and the dynamic context chain; the
  // runtime needs to provide both.
  if (resolve && var != NULL) proxy->BindTo(var);

  return proxy;
}


// Language extension which is only enabled for source files loaded
// through the API's extension mechanism.  A native function
// declaration is resolved by looking up the function through a
// callback provided by the extension.
Statement* Parser::ParseNativeDeclaration(bool* ok) {
  if (extension_ == NULL) {
    ReportUnexpectedToken(Token::NATIVE);
    *ok = false;
    return NULL;
  }

  Expect(Token::NATIVE, CHECK_OK);
  Expect(Token::FUNCTION, CHECK_OK);
  Handle<String> name = ParseIdentifier(CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  bool done = (peek() == Token::RPAREN);
  while (!done) {
    ParseIdentifier(CHECK_OK);
    done = (peek() == Token::RPAREN);
    if (!done) Expect(Token::COMMA, CHECK_OK);
  }
  Expect(Token::RPAREN, CHECK_OK);
  Expect(Token::SEMICOLON, CHECK_OK);

  if (is_pre_parsing_) return NULL;

  // Make sure that the function containing the native declaration
  // isn't lazily compiled. The extension structures are only
  // accessible while parsing the first time not when reparsing
  // because of lazy compilation.
  top_scope_->ForceEagerCompilation();

  // Compute the function template for the native function.
  v8::Handle<v8::FunctionTemplate> fun_template =
      extension_->GetNativeFunction(v8::Utils::ToLocal(name));
  ASSERT(!fun_template.IsEmpty());

  // Instantiate the function and create a shared function info from it.
  Handle<JSFunction> fun = Utils::OpenHandle(*fun_template->GetFunction());
  const int literals = fun->NumberOfLiterals();
  Handle<Code> code = Handle<Code>(fun->shared()->code());
  Handle<Code> construct_stub = Handle<Code>(fun->shared()->construct_stub());
  Handle<SharedFunctionInfo> shared =
      Factory::NewSharedFunctionInfo(name, literals, code,
          Handle<SerializedScopeInfo>(fun->shared()->scope_info()));
  shared->set_construct_stub(*construct_stub);

  // Copy the function data to the shared function info.
  shared->set_function_data(fun->shared()->function_data());
  int parameters = fun->shared()->formal_parameter_count();
  shared->set_formal_parameter_count(parameters);

  // TODO(1240846): It's weird that native function declarations are
  // introduced dynamically when we meet their declarations, whereas
  // other functions are setup when entering the surrounding scope.
  SharedFunctionInfoLiteral* lit = NEW(SharedFunctionInfoLiteral(shared));
  VariableProxy* var = Declare(name, Variable::VAR, NULL, true, CHECK_OK);
  return NEW(ExpressionStatement(
      new Assignment(Token::INIT_VAR, var, lit, RelocInfo::kNoPosition)));
}


Statement* Parser::ParseFunctionDeclaration(bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameterListopt ')' '{' FunctionBody '}'
  Expect(Token::FUNCTION, CHECK_OK);
  int function_token_position = scanner().location().beg_pos;
  Handle<String> name = ParseIdentifier(CHECK_OK);
  FunctionLiteral* fun = ParseFunctionLiteral(name,
                                              function_token_position,
                                              DECLARATION,
                                              CHECK_OK);
  // Even if we're not at the top-level of the global or a function
  // scope, we treat is as such and introduce the function with it's
  // initial value upon entering the corresponding scope.
  Declare(name, Variable::VAR, fun, true, CHECK_OK);
  return factory()->EmptyStatement();
}


Block* Parser::ParseBlock(ZoneStringList* labels, bool* ok) {
  // Block ::
  //   '{' Statement* '}'

  // Note that a Block does not introduce a new execution scope!
  // (ECMA-262, 3rd, 12.2)
  //
  // Construct block expecting 16 statements.
  Block* result = NEW(Block(labels, 16, false));
  Target target(this, result);
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    Statement* stat = ParseStatement(NULL, CHECK_OK);
    if (stat && !stat->IsEmpty()) result->AddStatement(stat);
  }
  Expect(Token::RBRACE, CHECK_OK);
  return result;
}


Block* Parser::ParseVariableStatement(bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  Expression* dummy;  // to satisfy the ParseVariableDeclarations() signature
  Block* result = ParseVariableDeclarations(true, &dummy, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}


// If the variable declaration declares exactly one non-const
// variable, then *var is set to that variable. In all other cases,
// *var is untouched; in particular, it is the caller's responsibility
// to initialize it properly. This mechanism is used for the parsing
// of 'for-in' loops.
Block* Parser::ParseVariableDeclarations(bool accept_IN,
                                         Expression** var,
                                         bool* ok) {
  // VariableDeclarations ::
  //   ('var' | 'const') (Identifier ('=' AssignmentExpression)?)+[',']

  Variable::Mode mode = Variable::VAR;
  bool is_const = false;
  if (peek() == Token::VAR) {
    Consume(Token::VAR);
  } else if (peek() == Token::CONST) {
    Consume(Token::CONST);
    mode = Variable::CONST;
    is_const = true;
  } else {
    UNREACHABLE();  // by current callers
  }

  // The scope of a variable/const declared anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). Thus we can
  // transform a source-level variable/const declaration into a (Function)
  // Scope declaration, and rewrite the source-level initialization into an
  // assignment statement. We use a block to collect multiple assignments.
  //
  // We mark the block as initializer block because we don't want the
  // rewriter to add a '.result' assignment to such a block (to get compliant
  // behavior for code such as print(eval('var x = 7')), and for cosmetic
  // reasons when pretty-printing. Also, unless an assignment (initialization)
  // is inside an initializer block, it is ignored.
  //
  // Create new block with one expected declaration.
  Block* block = NEW(Block(NULL, 1, true));
  VariableProxy* last_var = NULL;  // the last variable declared
  int nvars = 0;  // the number of variables declared
  do {
    // Parse variable name.
    if (nvars > 0) Consume(Token::COMMA);
    Handle<String> name = ParseIdentifier(CHECK_OK);

    // Declare variable.
    // Note that we *always* must treat the initial value via a separate init
    // assignment for variables and constants because the value must be assigned
    // when the variable is encountered in the source. But the variable/constant
    // is declared (and set to 'undefined') upon entering the function within
    // which the variable or constant is declared. Only function variables have
    // an initial value in the declaration (because they are initialized upon
    // entering the function).
    //
    // If we have a const declaration, in an inner scope, the proxy is always
    // bound to the declared variable (independent of possibly surrounding with
    // statements).
    last_var = Declare(name, mode, NULL,
                       is_const /* always bound for CONST! */,
                       CHECK_OK);
    nvars++;

    // Parse initialization expression if present and/or needed. A
    // declaration of the form:
    //
    //    var v = x;
    //
    // is syntactic sugar for:
    //
    //    var v; v = x;
    //
    // In particular, we need to re-lookup 'v' as it may be a
    // different 'v' than the 'v' in the declaration (if we are inside
    // a 'with' statement that makes a object property with name 'v'
    // visible).
    //
    // However, note that const declarations are different! A const
    // declaration of the form:
    //
    //   const c = x;
    //
    // is *not* syntactic sugar for:
    //
    //   const c; c = x;
    //
    // The "variable" c initialized to x is the same as the declared
    // one - there is no re-lookup (see the last parameter of the
    // Declare() call above).

    Expression* value = NULL;
    int position = -1;
    if (peek() == Token::ASSIGN) {
      Expect(Token::ASSIGN, CHECK_OK);
      position = scanner().location().beg_pos;
      value = ParseAssignmentExpression(accept_IN, CHECK_OK);
    }

    // Make sure that 'const c' actually initializes 'c' to undefined
    // even though it seems like a stupid thing to do.
    if (value == NULL && is_const) {
      value = GetLiteralUndefined();
    }

    // Global variable declarations must be compiled in a specific
    // way. When the script containing the global variable declaration
    // is entered, the global variable must be declared, so that if it
    // doesn't exist (not even in a prototype of the global object) it
    // gets created with an initial undefined value. This is handled
    // by the declarations part of the function representing the
    // top-level global code; see Runtime::DeclareGlobalVariable. If
    // it already exists (in the object or in a prototype), it is
    // *not* touched until the variable declaration statement is
    // executed.
    //
    // Executing the variable declaration statement will always
    // guarantee to give the global object a "local" variable; a
    // variable defined in the global object and not in any
    // prototype. This way, global variable declarations can shadow
    // properties in the prototype chain, but only after the variable
    // declaration statement has been executed. This is important in
    // browsers where the global object (window) has lots of
    // properties defined in prototype objects.

    if (!is_pre_parsing_ && top_scope_->is_global_scope()) {
      // Compute the arguments for the runtime call.
      ZoneList<Expression*>* arguments = new ZoneList<Expression*>(2);
      // Be careful not to assign a value to the global variable if
      // we're in a with. The initialization value should not
      // necessarily be stored in the global object in that case,
      // which is why we need to generate a separate assignment node.
      arguments->Add(NEW(Literal(name)));  // we have at least 1 parameter
      if (is_const || (value != NULL && !inside_with())) {
        arguments->Add(value);
        value = NULL;  // zap the value to avoid the unnecessary assignment
      }
      // Construct the call to Runtime::DeclareGlobal{Variable,Const}Locally
      // and add it to the initialization statement block. Note that
      // this function does different things depending on if we have
      // 1 or 2 parameters.
      CallRuntime* initialize;
      if (is_const) {
        initialize =
          NEW(CallRuntime(
            Factory::InitializeConstGlobal_symbol(),
            Runtime::FunctionForId(Runtime::kInitializeConstGlobal),
            arguments));
      } else {
        initialize =
          NEW(CallRuntime(
            Factory::InitializeVarGlobal_symbol(),
            Runtime::FunctionForId(Runtime::kInitializeVarGlobal),
            arguments));
      }
      block->AddStatement(NEW(ExpressionStatement(initialize)));
    }

    // Add an assignment node to the initialization statement block if
    // we still have a pending initialization value. We must distinguish
    // between variables and constants: Variable initializations are simply
    // assignments (with all the consequences if they are inside a 'with'
    // statement - they may change a 'with' object property). Constant
    // initializations always assign to the declared constant which is
    // always at the function scope level. This is only relevant for
    // dynamically looked-up variables and constants (the start context
    // for constant lookups is always the function context, while it is
    // the top context for variables). Sigh...
    if (value != NULL) {
      Token::Value op = (is_const ? Token::INIT_CONST : Token::INIT_VAR);
      Assignment* assignment = NEW(Assignment(op, last_var, value, position));
      if (block) block->AddStatement(NEW(ExpressionStatement(assignment)));
    }
  } while (peek() == Token::COMMA);

  if (!is_const && nvars == 1) {
    // We have a single, non-const variable.
    if (is_pre_parsing_) {
      // If we're preparsing then we need to set the var to something
      // in order for for-in loops to parse correctly.
      *var = ValidLeftHandSideSentinel::instance();
    } else {
      ASSERT(last_var != NULL);
      *var = last_var;
    }
  }

  return block;
}


static bool ContainsLabel(ZoneStringList* labels, Handle<String> label) {
  ASSERT(!label.is_null());
  if (labels != NULL)
    for (int i = labels->length(); i-- > 0; )
      if (labels->at(i).is_identical_to(label))
        return true;

  return false;
}


Statement* Parser::ParseExpressionOrLabelledStatement(ZoneStringList* labels,
                                                      bool* ok) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement

  Expression* expr = ParseExpression(true, CHECK_OK);
  if (peek() == Token::COLON && expr &&
      expr->AsVariableProxy() != NULL &&
      !expr->AsVariableProxy()->is_this()) {
    VariableProxy* var = expr->AsVariableProxy();
    Handle<String> label = var->name();
    // TODO(1240780): We don't check for redeclaration of labels
    // during preparsing since keeping track of the set of active
    // labels requires nontrivial changes to the way scopes are
    // structured.  However, these are probably changes we want to
    // make later anyway so we should go back and fix this then.
    if (!is_pre_parsing_) {
      if (ContainsLabel(labels, label) || TargetStackContainsLabel(label)) {
        SmartPointer<char> c_string = label->ToCString(DISALLOW_NULLS);
        const char* elms[2] = { "Label", *c_string };
        Vector<const char*> args(elms, 2);
        ReportMessage("redeclaration", args);
        *ok = false;
        return NULL;
      }
      if (labels == NULL) labels = new ZoneStringList(4);
      labels->Add(label);
      // Remove the "ghost" variable that turned out to be a label
      // from the top scope. This way, we don't try to resolve it
      // during the scope processing.
      top_scope_->RemoveUnresolved(var);
    }
    Expect(Token::COLON, CHECK_OK);
    return ParseStatement(labels, ok);
  }

  // Parsed expression statement.
  ExpectSemicolon(CHECK_OK);
  return NEW(ExpressionStatement(expr));
}


IfStatement* Parser::ParseIfStatement(ZoneStringList* labels, bool* ok) {
  // IfStatement ::
  //   'if' '(' Expression ')' Statement ('else' Statement)?

  Expect(Token::IF, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* condition = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  Statement* then_statement = ParseStatement(labels, CHECK_OK);
  Statement* else_statement = NULL;
  if (peek() == Token::ELSE) {
    Next();
    else_statement = ParseStatement(labels, CHECK_OK);
  } else if (!is_pre_parsing_) {
    else_statement = factory()->EmptyStatement();
  }
  return NEW(IfStatement(condition, then_statement, else_statement));
}


Statement* Parser::ParseContinueStatement(bool* ok) {
  // ContinueStatement ::
  //   'continue' Identifier? ';'

  Expect(Token::CONTINUE, CHECK_OK);
  Handle<String> label = Handle<String>::null();
  Token::Value tok = peek();
  if (!scanner_.has_line_terminator_before_next() &&
      tok != Token::SEMICOLON && tok != Token::RBRACE && tok != Token::EOS) {
    label = ParseIdentifier(CHECK_OK);
  }
  IterationStatement* target = NULL;
  if (!is_pre_parsing_) {
    target = LookupContinueTarget(label, CHECK_OK);
    if (target == NULL) {
      // Illegal continue statement.  To be consistent with KJS we delay
      // reporting of the syntax error until runtime.
      Handle<String> error_type = Factory::illegal_continue_symbol();
      if (!label.is_null()) error_type = Factory::unknown_label_symbol();
      Expression* throw_error = NewThrowSyntaxError(error_type, label);
      return NEW(ExpressionStatement(throw_error));
    }
  }
  ExpectSemicolon(CHECK_OK);
  return NEW(ContinueStatement(target));
}


Statement* Parser::ParseBreakStatement(ZoneStringList* labels, bool* ok) {
  // BreakStatement ::
  //   'break' Identifier? ';'

  Expect(Token::BREAK, CHECK_OK);
  Handle<String> label;
  Token::Value tok = peek();
  if (!scanner_.has_line_terminator_before_next() &&
      tok != Token::SEMICOLON && tok != Token::RBRACE && tok != Token::EOS) {
    label = ParseIdentifier(CHECK_OK);
  }
  // Parse labeled break statements that target themselves into
  // empty statements, e.g. 'l1: l2: l3: break l2;'
  if (!label.is_null() && ContainsLabel(labels, label)) {
    return factory()->EmptyStatement();
  }
  BreakableStatement* target = NULL;
  if (!is_pre_parsing_) {
    target = LookupBreakTarget(label, CHECK_OK);
    if (target == NULL) {
      // Illegal break statement.  To be consistent with KJS we delay
      // reporting of the syntax error until runtime.
      Handle<String> error_type = Factory::illegal_break_symbol();
      if (!label.is_null()) error_type = Factory::unknown_label_symbol();
      Expression* throw_error = NewThrowSyntaxError(error_type, label);
      return NEW(ExpressionStatement(throw_error));
    }
  }
  ExpectSemicolon(CHECK_OK);
  return NEW(BreakStatement(target));
}


Statement* Parser::ParseReturnStatement(bool* ok) {
  // ReturnStatement ::
  //   'return' Expression? ';'

  // Consume the return token. It is necessary to do the before
  // reporting any errors on it, because of the way errors are
  // reported (underlining).
  Expect(Token::RETURN, CHECK_OK);

  // An ECMAScript program is considered syntactically incorrect if it
  // contains a return statement that is not within the body of a
  // function. See ECMA-262, section 12.9, page 67.
  //
  // To be consistent with KJS we report the syntax error at runtime.
  if (!is_pre_parsing_ && !top_scope_->is_function_scope()) {
    Handle<String> type = Factory::illegal_return_symbol();
    Expression* throw_error = NewThrowSyntaxError(type, Handle<Object>::null());
    return NEW(ExpressionStatement(throw_error));
  }

  Token::Value tok = peek();
  if (scanner_.has_line_terminator_before_next() ||
      tok == Token::SEMICOLON ||
      tok == Token::RBRACE ||
      tok == Token::EOS) {
    ExpectSemicolon(CHECK_OK);
    return NEW(ReturnStatement(GetLiteralUndefined()));
  }

  Expression* expr = ParseExpression(true, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return NEW(ReturnStatement(expr));
}


Block* Parser::WithHelper(Expression* obj,
                          ZoneStringList* labels,
                          bool is_catch_block,
                          bool* ok) {
  // Parse the statement and collect escaping labels.
  ZoneList<BreakTarget*>* target_list = NEW(ZoneList<BreakTarget*>(0));
  TargetCollector collector(target_list);
  Statement* stat;
  { Target target(this, &collector);
    with_nesting_level_++;
    top_scope_->RecordWithStatement();
    stat = ParseStatement(labels, CHECK_OK);
    with_nesting_level_--;
  }
  // Create resulting block with two statements.
  // 1: Evaluate the with expression.
  // 2: The try-finally block evaluating the body.
  Block* result = NEW(Block(NULL, 2, false));

  if (result != NULL) {
    result->AddStatement(NEW(WithEnterStatement(obj, is_catch_block)));

    // Create body block.
    Block* body = NEW(Block(NULL, 1, false));
    body->AddStatement(stat);

    // Create exit block.
    Block* exit = NEW(Block(NULL, 1, false));
    exit->AddStatement(NEW(WithExitStatement()));

    // Return a try-finally statement.
    TryFinallyStatement* wrapper = NEW(TryFinallyStatement(body, exit));
    wrapper->set_escaping_targets(collector.targets());
    result->AddStatement(wrapper);
  }
  return result;
}


Statement* Parser::ParseWithStatement(ZoneStringList* labels, bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement

  Expect(Token::WITH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* expr = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  return WithHelper(expr, labels, false, CHECK_OK);
}


CaseClause* Parser::ParseCaseClause(bool* default_seen_ptr, bool* ok) {
  // CaseClause ::
  //   'case' Expression ':' Statement*
  //   'default' ':' Statement*

  Expression* label = NULL;  // NULL expression indicates default case
  if (peek() == Token::CASE) {
    Expect(Token::CASE, CHECK_OK);
    label = ParseExpression(true, CHECK_OK);
  } else {
    Expect(Token::DEFAULT, CHECK_OK);
    if (*default_seen_ptr) {
      ReportMessage("multiple_defaults_in_switch",
                    Vector<const char*>::empty());
      *ok = false;
      return NULL;
    }
    *default_seen_ptr = true;
  }
  Expect(Token::COLON, CHECK_OK);

  ZoneListWrapper<Statement> statements = factory()->NewList<Statement>(5);
  while (peek() != Token::CASE &&
         peek() != Token::DEFAULT &&
         peek() != Token::RBRACE) {
    Statement* stat = ParseStatement(NULL, CHECK_OK);
    statements.Add(stat);
  }

  return NEW(CaseClause(label, statements.elements()));
}


SwitchStatement* Parser::ParseSwitchStatement(ZoneStringList* labels,
                                              bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'

  SwitchStatement* statement = NEW(SwitchStatement(labels));
  Target target(this, statement);

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* tag = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  bool default_seen = false;
  ZoneListWrapper<CaseClause> cases = factory()->NewList<CaseClause>(4);
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    CaseClause* clause = ParseCaseClause(&default_seen, CHECK_OK);
    cases.Add(clause);
  }
  Expect(Token::RBRACE, CHECK_OK);

  if (statement) statement->Initialize(tag, cases.elements());
  return statement;
}


Statement* Parser::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' Expression ';'

  Expect(Token::THROW, CHECK_OK);
  int pos = scanner().location().beg_pos;
  if (scanner_.has_line_terminator_before_next()) {
    ReportMessage("newline_after_throw", Vector<const char*>::empty());
    *ok = false;
    return NULL;
  }
  Expression* exception = ParseExpression(true, CHECK_OK);
  ExpectSemicolon(CHECK_OK);

  return NEW(ExpressionStatement(new Throw(exception, pos)));
}


TryStatement* Parser::ParseTryStatement(bool* ok) {
  // TryStatement ::
  //   'try' Block Catch
  //   'try' Block Finally
  //   'try' Block Catch Finally
  //
  // Catch ::
  //   'catch' '(' Identifier ')' Block
  //
  // Finally ::
  //   'finally' Block

  Expect(Token::TRY, CHECK_OK);

  ZoneList<BreakTarget*>* target_list = NEW(ZoneList<BreakTarget*>(0));
  TargetCollector collector(target_list);
  Block* try_block;

  { Target target(this, &collector);
    try_block = ParseBlock(NULL, CHECK_OK);
  }

  Block* catch_block = NULL;
  VariableProxy* catch_var = NULL;
  Block* finally_block = NULL;

  Token::Value tok = peek();
  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessage("no_catch_or_finally", Vector<const char*>::empty());
    *ok = false;
    return NULL;
  }

  // If we can break out from the catch block and there is a finally block,
  // then we will need to collect jump targets from the catch block. Since
  // we don't know yet if there will be a finally block, we always collect
  // the jump targets.
  ZoneList<BreakTarget*>* catch_target_list = NEW(ZoneList<BreakTarget*>(0));
  TargetCollector catch_collector(catch_target_list);
  bool has_catch = false;
  if (tok == Token::CATCH) {
    has_catch = true;
    Consume(Token::CATCH);

    Expect(Token::LPAREN, CHECK_OK);
    Handle<String> name = ParseIdentifier(CHECK_OK);
    Expect(Token::RPAREN, CHECK_OK);

    if (peek() == Token::LBRACE) {
      // Allocate a temporary for holding the finally state while
      // executing the finally block.
      catch_var = top_scope_->NewTemporary(Factory::catch_var_symbol());
      Literal* name_literal = NEW(Literal(name));
      Expression* obj = NEW(CatchExtensionObject(name_literal, catch_var));
      { Target target(this, &catch_collector);
        catch_block = WithHelper(obj, NULL, true, CHECK_OK);
      }
    } else {
      Expect(Token::LBRACE, CHECK_OK);
    }

    tok = peek();
  }

  if (tok == Token::FINALLY || !has_catch) {
    Consume(Token::FINALLY);
    // Declare a variable for holding the finally state while
    // executing the finally block.
    finally_block = ParseBlock(NULL, CHECK_OK);
  }

  // Simplify the AST nodes by converting:
  //   'try { } catch { } finally { }'
  // to:
  //   'try { try { } catch { } } finally { }'

  if (!is_pre_parsing_ && catch_block != NULL && finally_block != NULL) {
    TryCatchStatement* statement =
        NEW(TryCatchStatement(try_block, catch_var, catch_block));
    statement->set_escaping_targets(collector.targets());
    try_block = NEW(Block(NULL, 1, false));
    try_block->AddStatement(statement);
    catch_block = NULL;
  }

  TryStatement* result = NULL;
  if (!is_pre_parsing_) {
    if (catch_block != NULL) {
      ASSERT(finally_block == NULL);
      result = NEW(TryCatchStatement(try_block, catch_var, catch_block));
      result->set_escaping_targets(collector.targets());
    } else {
      ASSERT(finally_block != NULL);
      result = NEW(TryFinallyStatement(try_block, finally_block));
      // Add the jump targets of the try block and the catch block.
      for (int i = 0; i < collector.targets()->length(); i++) {
        catch_collector.AddTarget(collector.targets()->at(i));
      }
      result->set_escaping_targets(catch_collector.targets());
    }
  }

  return result;
}


DoWhileStatement* Parser::ParseDoWhileStatement(ZoneStringList* labels,
                                                bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  DoWhileStatement* loop = NEW(DoWhileStatement(labels));
  Target target(this, loop);

  Expect(Token::DO, CHECK_OK);
  Statement* body = ParseStatement(NULL, CHECK_OK);
  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);

  if (loop != NULL) {
    int position = scanner().location().beg_pos;
    loop->set_condition_position(position);
  }

  Expression* cond = ParseExpression(true, CHECK_OK);
  if (cond != NULL) cond->set_is_loop_condition(true);
  Expect(Token::RPAREN, CHECK_OK);

  // Allow do-statements to be terminated with and without
  // semi-colons. This allows code such as 'do;while(0)return' to
  // parse, which would not be the case if we had used the
  // ExpectSemicolon() functionality here.
  if (peek() == Token::SEMICOLON) Consume(Token::SEMICOLON);

  if (loop != NULL) loop->Initialize(cond, body);

  seen_loop_stmt_ = true;

  return loop;
}


WhileStatement* Parser::ParseWhileStatement(ZoneStringList* labels, bool* ok) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  WhileStatement* loop = NEW(WhileStatement(labels));
  Target target(this, loop);

  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* cond = ParseExpression(true, CHECK_OK);
  if (cond != NULL) cond->set_is_loop_condition(true);
  Expect(Token::RPAREN, CHECK_OK);
  Statement* body = ParseStatement(NULL, CHECK_OK);

  if (loop != NULL) loop->Initialize(cond, body);

  seen_loop_stmt_ = true;

  return loop;
}


Statement* Parser::ParseForStatement(ZoneStringList* labels, bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  Statement* init = NULL;

  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  if (peek() != Token::SEMICOLON) {
    if (peek() == Token::VAR || peek() == Token::CONST) {
      Expression* each = NULL;
      Block* variable_statement =
          ParseVariableDeclarations(false, &each, CHECK_OK);
      if (peek() == Token::IN && each != NULL) {
        ForInStatement* loop = NEW(ForInStatement(labels));
        Target target(this, loop);

        Expect(Token::IN, CHECK_OK);
        Expression* enumerable = ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);

        Statement* body = ParseStatement(NULL, CHECK_OK);
        if (is_pre_parsing_) {
          return NULL;
        } else {
          loop->Initialize(each, enumerable, body);
          Block* result = NEW(Block(NULL, 2, false));
          result->AddStatement(variable_statement);
          result->AddStatement(loop);

          seen_loop_stmt_ = true;

          // Parsed for-in loop w/ variable/const declaration.
          return result;
        }

      } else {
        init = variable_statement;
      }

    } else {
      Expression* expression = ParseExpression(false, CHECK_OK);
      if (peek() == Token::IN) {
        // Signal a reference error if the expression is an invalid
        // left-hand side expression.  We could report this as a syntax
        // error here but for compatibility with JSC we choose to report
        // the error at runtime.
        if (expression == NULL || !expression->IsValidLeftHandSide()) {
          Handle<String> type = Factory::invalid_lhs_in_for_in_symbol();
          expression = NewThrowReferenceError(type);
        }
        ForInStatement* loop = NEW(ForInStatement(labels));
        Target target(this, loop);

        Expect(Token::IN, CHECK_OK);
        Expression* enumerable = ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);

        Statement* body = ParseStatement(NULL, CHECK_OK);
        if (loop) loop->Initialize(expression, enumerable, body);

        seen_loop_stmt_ = true;

        // Parsed for-in loop.
        return loop;

      } else {
        init = NEW(ExpressionStatement(expression));
      }
    }
  }

  // Standard 'for' loop
  ForStatement* loop = NEW(ForStatement(labels));
  Target target(this, loop);

  // Parsed initializer at this point.
  Expect(Token::SEMICOLON, CHECK_OK);

  Expression* cond = NULL;
  if (peek() != Token::SEMICOLON) {
    cond = ParseExpression(true, CHECK_OK);
    if (cond != NULL) cond->set_is_loop_condition(true);
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  Statement* next = NULL;
  if (peek() != Token::RPAREN) {
    Expression* exp = ParseExpression(true, CHECK_OK);
    next = NEW(ExpressionStatement(exp));
  }
  Expect(Token::RPAREN, CHECK_OK);

  seen_loop_stmt_ = false;

  Statement* body = ParseStatement(NULL, CHECK_OK);

  // Mark this loop if it is an inner loop.
  if (loop && !seen_loop_stmt_) loop->set_peel_this_loop(true);

  if (loop) loop->Initialize(init, cond, next, body);

  seen_loop_stmt_ = true;

  return loop;
}


// Precedence = 1
Expression* Parser::ParseExpression(bool accept_IN, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  Expression* result = ParseAssignmentExpression(accept_IN, CHECK_OK);
  while (peek() == Token::COMMA) {
    Expect(Token::COMMA, CHECK_OK);
    Expression* right = ParseAssignmentExpression(accept_IN, CHECK_OK);
    result = NEW(BinaryOperation(Token::COMMA, result, right));
  }
  return result;
}


// Precedence = 2
Expression* Parser::ParseAssignmentExpression(bool accept_IN, bool* ok) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression

  Expression* expression = ParseConditionalExpression(accept_IN, CHECK_OK);

  if (!Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    return expression;
  }

  // Signal a reference error if the expression is an invalid left-hand
  // side expression.  We could report this as a syntax error here but
  // for compatibility with JSC we choose to report the error at
  // runtime.
  if (expression == NULL || !expression->IsValidLeftHandSide()) {
    Handle<String> type = Factory::invalid_lhs_in_assignment_symbol();
    expression = NewThrowReferenceError(type);
  }

  Token::Value op = Next();  // Get assignment operator.
  int pos = scanner().location().beg_pos;
  Expression* right = ParseAssignmentExpression(accept_IN, CHECK_OK);

  // TODO(1231235): We try to estimate the set of properties set by
  // constructors. We define a new property whenever there is an
  // assignment to a property of 'this'. We should probably only add
  // properties if we haven't seen them before. Otherwise we'll
  // probably overestimate the number of properties.
  Property* property = expression ? expression->AsProperty() : NULL;
  if (op == Token::ASSIGN &&
      property != NULL &&
      property->obj()->AsVariableProxy() != NULL &&
      property->obj()->AsVariableProxy()->is_this()) {
    temp_scope_->AddProperty();
  }

  return NEW(Assignment(op, expression, right, pos));
}


// Precedence = 3
Expression* Parser::ParseConditionalExpression(bool accept_IN, bool* ok) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  // We start using the binary expression parser for prec >= 4 only!
  Expression* expression = ParseBinaryExpression(4, accept_IN, CHECK_OK);
  if (peek() != Token::CONDITIONAL) return expression;
  Consume(Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  int left_position = scanner().peek_location().beg_pos;
  Expression* left = ParseAssignmentExpression(true, CHECK_OK);
  Expect(Token::COLON, CHECK_OK);
  int right_position = scanner().peek_location().beg_pos;
  Expression* right = ParseAssignmentExpression(accept_IN, CHECK_OK);
  return NEW(Conditional(expression, left, right,
                         left_position, right_position));
}


static int Precedence(Token::Value tok, bool accept_IN) {
  if (tok == Token::IN && !accept_IN)
    return 0;  // 0 precedence will terminate binary expression parsing

  return Token::Precedence(tok);
}


// Precedence >= 4
Expression* Parser::ParseBinaryExpression(int prec, bool accept_IN, bool* ok) {
  ASSERT(prec >= 4);
  Expression* x = ParseUnaryExpression(CHECK_OK);
  for (int prec1 = Precedence(peek(), accept_IN); prec1 >= prec; prec1--) {
    // prec1 >= 4
    while (Precedence(peek(), accept_IN) == prec1) {
      Token::Value op = Next();
      Expression* y = ParseBinaryExpression(prec1 + 1, accept_IN, CHECK_OK);

      // Compute some expressions involving only number literals.
      if (x && x->AsLiteral() && x->AsLiteral()->handle()->IsNumber() &&
          y && y->AsLiteral() && y->AsLiteral()->handle()->IsNumber()) {
        double x_val = x->AsLiteral()->handle()->Number();
        double y_val = y->AsLiteral()->handle()->Number();

        switch (op) {
          case Token::ADD:
            x = NewNumberLiteral(x_val + y_val);
            continue;
          case Token::SUB:
            x = NewNumberLiteral(x_val - y_val);
            continue;
          case Token::MUL:
            x = NewNumberLiteral(x_val * y_val);
            continue;
          case Token::DIV:
            x = NewNumberLiteral(x_val / y_val);
            continue;
          case Token::BIT_OR:
            x = NewNumberLiteral(DoubleToInt32(x_val) | DoubleToInt32(y_val));
            continue;
          case Token::BIT_AND:
            x = NewNumberLiteral(DoubleToInt32(x_val) & DoubleToInt32(y_val));
            continue;
          case Token::BIT_XOR:
            x = NewNumberLiteral(DoubleToInt32(x_val) ^ DoubleToInt32(y_val));
            continue;
          case Token::SHL: {
            int value = DoubleToInt32(x_val) << (DoubleToInt32(y_val) & 0x1f);
            x = NewNumberLiteral(value);
            continue;
          }
          case Token::SHR: {
            uint32_t shift = DoubleToInt32(y_val) & 0x1f;
            uint32_t value = DoubleToUint32(x_val) >> shift;
            x = NewNumberLiteral(value);
            continue;
          }
          case Token::SAR: {
            uint32_t shift = DoubleToInt32(y_val) & 0x1f;
            int value = ArithmeticShiftRight(DoubleToInt32(x_val), shift);
            x = NewNumberLiteral(value);
            continue;
          }
          default:
            break;
        }
      }

      // Convert constant divisions to multiplications for speed.
      if (op == Token::DIV &&
          y && y->AsLiteral() && y->AsLiteral()->handle()->IsNumber()) {
        double y_val = y->AsLiteral()->handle()->Number();
        int64_t y_int = static_cast<int64_t>(y_val);
        // There are rounding issues with this optimization, but they don't
        // apply if the number to be divided with has a reciprocal that can be
        // precisely represented as a floating point number.  This is the case
        // if the number is an integer power of 2.  Negative integer powers of
        // 2 work too, but for -2, -1, 1 and 2 we don't do the strength
        // reduction because the inlined optimistic idiv has a reasonable
        // chance of succeeding by producing a Smi answer with no remainder.
        if (static_cast<double>(y_int) == y_val &&
            (IsPowerOf2(y_int) || IsPowerOf2(-y_int)) &&
            (y_int > 2 || y_int < -2)) {
          y = NewNumberLiteral(1 / y_val);
          op = Token::MUL;
        }
      }

      // For now we distinguish between comparisons and other binary
      // operations.  (We could combine the two and get rid of this
      // code an AST node eventually.)
      if (Token::IsCompareOp(op)) {
        // We have a comparison.
        Token::Value cmp = op;
        switch (op) {
          case Token::NE: cmp = Token::EQ; break;
          case Token::NE_STRICT: cmp = Token::EQ_STRICT; break;
          default: break;
        }
        x = NEW(CompareOperation(cmp, x, y));
        if (cmp != op) {
          // The comparison was negated - add a NOT.
          x = NEW(UnaryOperation(Token::NOT, x));
        }

      } else {
        // We have a "normal" binary operation.
        x = NEW(BinaryOperation(op, x, y));
      }
    }
  }
  return x;
}


Expression* Parser::ParseUnaryExpression(bool* ok) {
  // UnaryExpression ::
  //   PostfixExpression
  //   'delete' UnaryExpression
  //   'void' UnaryExpression
  //   'typeof' UnaryExpression
  //   '++' UnaryExpression
  //   '--' UnaryExpression
  //   '+' UnaryExpression
  //   '-' UnaryExpression
  //   '~' UnaryExpression
  //   '!' UnaryExpression

  Token::Value op = peek();
  if (Token::IsUnaryOp(op)) {
    op = Next();
    Expression* expression = ParseUnaryExpression(CHECK_OK);

    // Compute some expressions involving only number literals.
    if (expression != NULL && expression->AsLiteral() &&
        expression->AsLiteral()->handle()->IsNumber()) {
      double value = expression->AsLiteral()->handle()->Number();
      switch (op) {
        case Token::ADD:
          return expression;
        case Token::SUB:
          return NewNumberLiteral(-value);
        case Token::BIT_NOT:
          return NewNumberLiteral(~DoubleToInt32(value));
        default: break;
      }
    }

    return NEW(UnaryOperation(op, expression));

  } else if (Token::IsCountOp(op)) {
    op = Next();
    Expression* expression = ParseUnaryExpression(CHECK_OK);
    // Signal a reference error if the expression is an invalid
    // left-hand side expression.  We could report this as a syntax
    // error here but for compatibility with JSC we choose to report the
    // error at runtime.
    if (expression == NULL || !expression->IsValidLeftHandSide()) {
      Handle<String> type = Factory::invalid_lhs_in_prefix_op_symbol();
      expression = NewThrowReferenceError(type);
    }
    return NEW(CountOperation(true /* prefix */, op, expression));

  } else {
    return ParsePostfixExpression(ok);
  }
}


Expression* Parser::ParsePostfixExpression(bool* ok) {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  Expression* expression = ParseLeftHandSideExpression(CHECK_OK);
  if (!scanner_.has_line_terminator_before_next() && Token::IsCountOp(peek())) {
    // Signal a reference error if the expression is an invalid
    // left-hand side expression.  We could report this as a syntax
    // error here but for compatibility with JSC we choose to report the
    // error at runtime.
    if (expression == NULL || !expression->IsValidLeftHandSide()) {
      Handle<String> type = Factory::invalid_lhs_in_postfix_op_symbol();
      expression = NewThrowReferenceError(type);
    }
    Token::Value next = Next();
    expression = NEW(CountOperation(false /* postfix */, next, expression));
  }
  return expression;
}


Expression* Parser::ParseLeftHandSideExpression(bool* ok) {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  Expression* result;
  if (peek() == Token::NEW) {
    result = ParseNewExpression(CHECK_OK);
  } else {
    result = ParseMemberExpression(CHECK_OK);
  }

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        int pos = scanner().location().beg_pos;
        Expression* index = ParseExpression(true, CHECK_OK);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }

      case Token::LPAREN: {
        int pos = scanner().location().beg_pos;
        ZoneList<Expression*>* args = ParseArguments(CHECK_OK);

        // Keep track of eval() calls since they disable all local variable
        // optimizations.
        // The calls that need special treatment are the
        // direct (i.e. not aliased) eval calls. These calls are all of the
        // form eval(...) with no explicit receiver object where eval is not
        // declared in the current scope chain. These calls are marked as
        // potentially direct eval calls. Whether they are actually direct calls
        // to eval is determined at run time.
        if (!is_pre_parsing_) {
          VariableProxy* callee = result->AsVariableProxy();
          if (callee != NULL && callee->IsVariable(Factory::eval_symbol())) {
            Handle<String> name = callee->name();
            Variable* var = top_scope_->Lookup(name);
            if (var == NULL) {
              top_scope_->RecordEvalCall();
            }
          }
        }
        result = factory()->NewCall(result, args, pos);
        break;
      }

      case Token::PERIOD: {
        Consume(Token::PERIOD);
        int pos = scanner().location().beg_pos;
        Handle<String> name = ParseIdentifier(CHECK_OK);
        result = factory()->NewProperty(result, NEW(Literal(name)), pos);
        break;
      }

      default:
        return result;
    }
  }
}



Expression* Parser::ParseNewPrefix(PositionStack* stack, bool* ok) {
  // NewExpression ::
  //   ('new')+ MemberExpression

  // The grammar for new expressions is pretty warped. The keyword
  // 'new' can either be a part of the new expression (where it isn't
  // followed by an argument list) or a part of the member expression,
  // where it must be followed by an argument list. To accommodate
  // this, we parse the 'new' keywords greedily and keep track of how
  // many we have parsed. This information is then passed on to the
  // member expression parser, which is only allowed to match argument
  // lists as long as it has 'new' prefixes left
  Expect(Token::NEW, CHECK_OK);
  PositionStack::Element pos(stack, scanner().location().beg_pos);

  Expression* result;
  if (peek() == Token::NEW) {
    result = ParseNewPrefix(stack, CHECK_OK);
  } else {
    result = ParseMemberWithNewPrefixesExpression(stack, CHECK_OK);
  }

  if (!stack->is_empty()) {
    int last = stack->pop();
    result = NEW(CallNew(result, new ZoneList<Expression*>(0), last));
  }
  return result;
}


Expression* Parser::ParseNewExpression(bool* ok) {
  PositionStack stack(ok);
  return ParseNewPrefix(&stack, ok);
}


Expression* Parser::ParseMemberExpression(bool* ok) {
  return ParseMemberWithNewPrefixesExpression(NULL, ok);
}


Expression* Parser::ParseMemberWithNewPrefixesExpression(PositionStack* stack,
                                                         bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments)*

  // Parse the initial primary or function expression.
  Expression* result = NULL;
  if (peek() == Token::FUNCTION) {
    Expect(Token::FUNCTION, CHECK_OK);
    int function_token_position = scanner().location().beg_pos;
    Handle<String> name;
    if (peek() == Token::IDENTIFIER) name = ParseIdentifier(CHECK_OK);
    result = ParseFunctionLiteral(name, function_token_position,
                                  NESTED, CHECK_OK);
  } else {
    result = ParsePrimaryExpression(CHECK_OK);
  }

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        int pos = scanner().location().beg_pos;
        Expression* index = ParseExpression(true, CHECK_OK);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }
      case Token::PERIOD: {
        Consume(Token::PERIOD);
        int pos = scanner().location().beg_pos;
        Handle<String> name = ParseIdentifier(CHECK_OK);
        result = factory()->NewProperty(result, NEW(Literal(name)), pos);
        break;
      }
      case Token::LPAREN: {
        if ((stack == NULL) || stack->is_empty()) return result;
        // Consume one of the new prefixes (already parsed).
        ZoneList<Expression*>* args = ParseArguments(CHECK_OK);
        int last = stack->pop();
        result = NEW(CallNew(result, args, last));
        break;
      }
      default:
        return result;
    }
  }
}


DebuggerStatement* Parser::ParseDebuggerStatement(bool* ok) {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as i a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  Expect(Token::DEBUGGER, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return NEW(DebuggerStatement());
}


void Parser::ReportUnexpectedToken(Token::Value token) {
  // We don't report stack overflows here, to avoid increasing the
  // stack depth even further.  Instead we report it after parsing is
  // over, in ParseProgram/ParseJson.
  if (token == Token::ILLEGAL && scanner().stack_overflow())
    return;
  // Four of the tokens are treated specially
  switch (token) {
  case Token::EOS:
    return ReportMessage("unexpected_eos", Vector<const char*>::empty());
  case Token::NUMBER:
    return ReportMessage("unexpected_token_number",
                         Vector<const char*>::empty());
  case Token::STRING:
    return ReportMessage("unexpected_token_string",
                         Vector<const char*>::empty());
  case Token::IDENTIFIER:
    return ReportMessage("unexpected_token_identifier",
                         Vector<const char*>::empty());
  default:
    const char* name = Token::String(token);
    ASSERT(name != NULL);
    ReportMessage("unexpected_token", Vector<const char*>(&name, 1));
  }
}


void Parser::ReportInvalidPreparseData(Handle<String> name, bool* ok) {
  SmartPointer<char> name_string = name->ToCString(DISALLOW_NULLS);
  const char* element[1] = { *name_string };
  ReportMessage("invalid_preparser_data",
                Vector<const char*>(element, 1));
  *ok = false;
}


Expression* Parser::ParsePrimaryExpression(bool* ok) {
  // PrimaryExpression ::
  //   'this'
  //   'null'
  //   'true'
  //   'false'
  //   Identifier
  //   Number
  //   String
  //   ArrayLiteral
  //   ObjectLiteral
  //   RegExpLiteral
  //   '(' Expression ')'

  Expression* result = NULL;
  switch (peek()) {
    case Token::THIS: {
      Consume(Token::THIS);
      if (is_pre_parsing_) {
        result = VariableProxySentinel::this_proxy();
      } else {
        VariableProxy* recv = top_scope_->receiver();
        result = recv;
      }
      break;
    }

    case Token::NULL_LITERAL:
      Consume(Token::NULL_LITERAL);
      result = NEW(Literal(Factory::null_value()));
      break;

    case Token::TRUE_LITERAL:
      Consume(Token::TRUE_LITERAL);
      result = NEW(Literal(Factory::true_value()));
      break;

    case Token::FALSE_LITERAL:
      Consume(Token::FALSE_LITERAL);
      result = NEW(Literal(Factory::false_value()));
      break;

    case Token::IDENTIFIER: {
      Handle<String> name = ParseIdentifier(CHECK_OK);
      if (is_pre_parsing_) {
        result = VariableProxySentinel::identifier_proxy();
      } else {
        result = top_scope_->NewUnresolved(name, inside_with());
      }
      break;
    }

    case Token::NUMBER: {
      Consume(Token::NUMBER);
      double value =
        StringToDouble(scanner_.literal_string(), ALLOW_HEX | ALLOW_OCTALS);
      result = NewNumberLiteral(value);
      break;
    }

    case Token::STRING: {
      Consume(Token::STRING);
      Handle<String> symbol =
          factory()->LookupSymbol(scanner_.literal_string(),
                                  scanner_.literal_length());
      result = NEW(Literal(symbol));
      break;
    }

    case Token::ASSIGN_DIV:
      result = ParseRegExpLiteral(true, CHECK_OK);
      break;

    case Token::DIV:
      result = ParseRegExpLiteral(false, CHECK_OK);
      break;

    case Token::LBRACK:
      result = ParseArrayLiteral(CHECK_OK);
      break;

    case Token::LBRACE:
      result = ParseObjectLiteral(CHECK_OK);
      break;

    case Token::LPAREN:
      Consume(Token::LPAREN);
      result = ParseExpression(true, CHECK_OK);
      Expect(Token::RPAREN, CHECK_OK);
      break;

    case Token::MOD:
      if (allow_natives_syntax_ || extension_ != NULL) {
        result = ParseV8Intrinsic(CHECK_OK);
        break;
      }
      // If we're not allowing special syntax we fall-through to the
      // default case.

    default: {
      Token::Value tok = peek();
      // Token::Peek returns the value of the next token but
      // location() gives info about the current token.
      // Therefore, we need to read ahead to the next token
      Next();
      ReportUnexpectedToken(tok);
      *ok = false;
      return NULL;
    }
  }

  return result;
}


void Parser::BuildArrayLiteralBoilerplateLiterals(ZoneList<Expression*>* values,
                                                  Handle<FixedArray> literals,
                                                  bool* is_simple,
                                                  int* depth) {
  // Fill in the literals.
  // Accumulate output values in local variables.
  bool is_simple_acc = true;
  int depth_acc = 1;
  for (int i = 0; i < values->length(); i++) {
    MaterializedLiteral* m_literal = values->at(i)->AsMaterializedLiteral();
    if (m_literal != NULL && m_literal->depth() >= depth_acc) {
      depth_acc = m_literal->depth() + 1;
    }
    Handle<Object> boilerplate_value = GetBoilerplateValue(values->at(i));
    if (boilerplate_value->IsUndefined()) {
      literals->set_the_hole(i);
      is_simple_acc = false;
    } else {
      literals->set(i, *boilerplate_value);
    }
  }

  *is_simple = is_simple_acc;
  *depth = depth_acc;
}


Expression* Parser::ParseArrayLiteral(bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'

  ZoneListWrapper<Expression> values = factory()->NewList<Expression>(4);
  Expect(Token::LBRACK, CHECK_OK);
  while (peek() != Token::RBRACK) {
    Expression* elem;
    if (peek() == Token::COMMA) {
      elem = GetLiteralTheHole();
    } else {
      elem = ParseAssignmentExpression(true, CHECK_OK);
    }
    values.Add(elem);
    if (peek() != Token::RBRACK) {
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RBRACK, CHECK_OK);

  // Update the scope information before the pre-parsing bailout.
  int literal_index = temp_scope_->NextMaterializedLiteralIndex();

  if (is_pre_parsing_) return NULL;

  // Allocate a fixed array with all the literals.
  Handle<FixedArray> literals =
      Factory::NewFixedArray(values.length(), TENURED);

  // Fill in the literals.
  bool is_simple = true;
  int depth = 1;
  for (int i = 0; i < values.length(); i++) {
    MaterializedLiteral* m_literal = values.at(i)->AsMaterializedLiteral();
    if (m_literal != NULL && m_literal->depth() + 1 > depth) {
      depth = m_literal->depth() + 1;
    }
    Handle<Object> boilerplate_value = GetBoilerplateValue(values.at(i));
    if (boilerplate_value->IsUndefined()) {
      literals->set_the_hole(i);
      is_simple = false;
    } else {
      literals->set(i, *boilerplate_value);
    }
  }

  return NEW(ArrayLiteral(literals, values.elements(),
                          literal_index, is_simple, depth));
}


bool Parser::IsBoilerplateProperty(ObjectLiteral::Property* property) {
  return property != NULL &&
         property->kind() != ObjectLiteral::Property::PROTOTYPE;
}


bool CompileTimeValue::IsCompileTimeValue(Expression* expression) {
  MaterializedLiteral* lit = expression->AsMaterializedLiteral();
  return lit != NULL && lit->is_simple();
}

Handle<FixedArray> CompileTimeValue::GetValue(Expression* expression) {
  ASSERT(IsCompileTimeValue(expression));
  Handle<FixedArray> result = Factory::NewFixedArray(2, TENURED);
  ObjectLiteral* object_literal = expression->AsObjectLiteral();
  if (object_literal != NULL) {
    ASSERT(object_literal->is_simple());
    if (object_literal->fast_elements()) {
      result->set(kTypeSlot, Smi::FromInt(OBJECT_LITERAL_FAST_ELEMENTS));
    } else {
      result->set(kTypeSlot, Smi::FromInt(OBJECT_LITERAL_SLOW_ELEMENTS));
    }
    result->set(kElementsSlot, *object_literal->constant_properties());
  } else {
    ArrayLiteral* array_literal = expression->AsArrayLiteral();
    ASSERT(array_literal != NULL && array_literal->is_simple());
    result->set(kTypeSlot, Smi::FromInt(ARRAY_LITERAL));
    result->set(kElementsSlot, *array_literal->constant_elements());
  }
  return result;
}


CompileTimeValue::Type CompileTimeValue::GetType(Handle<FixedArray> value) {
  Smi* type_value = Smi::cast(value->get(kTypeSlot));
  return static_cast<Type>(type_value->value());
}


Handle<FixedArray> CompileTimeValue::GetElements(Handle<FixedArray> value) {
  return Handle<FixedArray>(FixedArray::cast(value->get(kElementsSlot)));
}


Handle<Object> Parser::GetBoilerplateValue(Expression* expression) {
  if (expression->AsLiteral() != NULL) {
    return expression->AsLiteral()->handle();
  }
  if (CompileTimeValue::IsCompileTimeValue(expression)) {
    return CompileTimeValue::GetValue(expression);
  }
  return Factory::undefined_value();
}


void Parser::BuildObjectLiteralConstantProperties(
    ZoneList<ObjectLiteral::Property*>* properties,
    Handle<FixedArray> constant_properties,
    bool* is_simple,
    bool* fast_elements,
    int* depth) {
  int position = 0;
  // Accumulate the value in local variables and store it at the end.
  bool is_simple_acc = true;
  int depth_acc = 1;
  uint32_t max_element_index = 0;
  uint32_t elements = 0;
  for (int i = 0; i < properties->length(); i++) {
    ObjectLiteral::Property* property = properties->at(i);
    if (!IsBoilerplateProperty(property)) {
      is_simple_acc = false;
      continue;
    }
    MaterializedLiteral* m_literal = property->value()->AsMaterializedLiteral();
    if (m_literal != NULL && m_literal->depth() >= depth_acc) {
      depth_acc = m_literal->depth() + 1;
    }

    // Add CONSTANT and COMPUTED properties to boilerplate. Use undefined
    // value for COMPUTED properties, the real value is filled in at
    // runtime. The enumeration order is maintained.
    Handle<Object> key = property->key()->handle();
    Handle<Object> value = GetBoilerplateValue(property->value());
    is_simple_acc = is_simple_acc && !value->IsUndefined();

    // Keep track of the number of elements in the object literal and
    // the largest element index.  If the largest element index is
    // much larger than the number of elements, creating an object
    // literal with fast elements will be a waste of space.
    uint32_t element_index = 0;
    if (key->IsString()
        && Handle<String>::cast(key)->AsArrayIndex(&element_index)
        && element_index > max_element_index) {
      max_element_index = element_index;
      elements++;
    } else if (key->IsSmi()) {
      int key_value = Smi::cast(*key)->value();
      if (key_value > 0
          && static_cast<uint32_t>(key_value) > max_element_index) {
        max_element_index = key_value;
      }
      elements++;
    }

    // Add name, value pair to the fixed array.
    constant_properties->set(position++, *key);
    constant_properties->set(position++, *value);
  }
  *fast_elements =
      (max_element_index <= 32) || ((2 * elements) >= max_element_index);
  *is_simple = is_simple_acc;
  *depth = depth_acc;
}


Expression* Parser::ParseObjectLiteral(bool* ok) {
  // ObjectLiteral ::
  //   '{' (
  //       ((Identifier | String | Number) ':' AssignmentExpression)
  //     | (('get' | 'set') FunctionLiteral)
  //    )*[','] '}'

  ZoneListWrapper<ObjectLiteral::Property> properties =
      factory()->NewList<ObjectLiteral::Property>(4);
  int number_of_boilerplate_properties = 0;

  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    Literal* key = NULL;
    switch (peek()) {
      case Token::IDENTIFIER: {
        // Store identifier keys as literal symbols to avoid
        // resolving them when compiling code for the object
        // literal.
        bool is_getter = false;
        bool is_setter = false;
        Handle<String> id =
            ParseIdentifierOrGetOrSet(&is_getter, &is_setter, CHECK_OK);
        if (is_getter || is_setter) {
          // Special handling of getter and setter syntax.
          if (peek() == Token::IDENTIFIER) {
            Handle<String> name = ParseIdentifier(CHECK_OK);
            FunctionLiteral* value =
                ParseFunctionLiteral(name, RelocInfo::kNoPosition,
                                     DECLARATION, CHECK_OK);
            ObjectLiteral::Property* property =
                NEW(ObjectLiteral::Property(is_getter, value));
            if (IsBoilerplateProperty(property))
              number_of_boilerplate_properties++;
            properties.Add(property);
            if (peek() != Token::RBRACE) Expect(Token::COMMA, CHECK_OK);
            continue;  // restart the while
          }
        }
        key = NEW(Literal(id));
        break;
      }

      case Token::STRING: {
        Consume(Token::STRING);
        Handle<String> string =
            factory()->LookupSymbol(scanner_.literal_string(),
                                    scanner_.literal_length());
        uint32_t index;
        if (!string.is_null() && string->AsArrayIndex(&index)) {
          key = NewNumberLiteral(index);
        } else {
          key = NEW(Literal(string));
        }
        break;
      }

      case Token::NUMBER: {
        Consume(Token::NUMBER);
        double value =
          StringToDouble(scanner_.literal_string(), ALLOW_HEX | ALLOW_OCTALS);
        key = NewNumberLiteral(value);
        break;
      }

      default:
        Expect(Token::RBRACE, CHECK_OK);
        break;
    }

    Expect(Token::COLON, CHECK_OK);
    Expression* value = ParseAssignmentExpression(true, CHECK_OK);

    ObjectLiteral::Property* property =
        NEW(ObjectLiteral::Property(key, value));

    // Count CONSTANT or COMPUTED properties to maintain the enumeration order.
    if (IsBoilerplateProperty(property)) number_of_boilerplate_properties++;
    properties.Add(property);

    // TODO(1240767): Consider allowing trailing comma.
    if (peek() != Token::RBRACE) Expect(Token::COMMA, CHECK_OK);
  }
  Expect(Token::RBRACE, CHECK_OK);
  // Computation of literal_index must happen before pre parse bailout.
  int literal_index = temp_scope_->NextMaterializedLiteralIndex();
  if (is_pre_parsing_) return NULL;

  Handle<FixedArray> constant_properties =
      Factory::NewFixedArray(number_of_boilerplate_properties * 2, TENURED);

  bool is_simple = true;
  bool fast_elements = true;
  int depth = 1;
  BuildObjectLiteralConstantProperties(properties.elements(),
                                       constant_properties,
                                       &is_simple,
                                       &fast_elements,
                                       &depth);
  return new ObjectLiteral(constant_properties,
                           properties.elements(),
                           literal_index,
                           is_simple,
                           fast_elements,
                           depth);
}


Expression* Parser::ParseRegExpLiteral(bool seen_equal, bool* ok) {
  if (!scanner_.ScanRegExpPattern(seen_equal)) {
    Next();
    ReportMessage("unterminated_regexp", Vector<const char*>::empty());
    *ok = false;
    return NULL;
  }

  int literal_index = temp_scope_->NextMaterializedLiteralIndex();

  if (is_pre_parsing_) {
    // If we're preparsing we just do all the parsing stuff without
    // building anything.
    if (!scanner_.ScanRegExpFlags()) {
      Next();
      ReportMessage("invalid_regexp_flags", Vector<const char*>::empty());
      *ok = false;
      return NULL;
    }
    Next();
    return NULL;
  }

  Handle<String> js_pattern =
      Factory::NewStringFromUtf8(scanner_.next_literal(), TENURED);
  scanner_.ScanRegExpFlags();
  Handle<String> js_flags =
      Factory::NewStringFromUtf8(scanner_.next_literal(), TENURED);
  Next();

  return new RegExpLiteral(js_pattern, js_flags, literal_index);
}


ZoneList<Expression*>* Parser::ParseArguments(bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  ZoneListWrapper<Expression> result = factory()->NewList<Expression>(4);
  Expect(Token::LPAREN, CHECK_OK);
  bool done = (peek() == Token::RPAREN);
  while (!done) {
    Expression* argument = ParseAssignmentExpression(true, CHECK_OK);
    result.Add(argument);
    done = (peek() == Token::RPAREN);
    if (!done) Expect(Token::COMMA, CHECK_OK);
  }
  Expect(Token::RPAREN, CHECK_OK);
  return result.elements();
}


FunctionLiteral* Parser::ParseFunctionLiteral(Handle<String> var_name,
                                              int function_token_position,
                                              FunctionLiteralType type,
                                              bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Reset flag used for inner loop detection.
  seen_loop_stmt_ = false;

  bool is_named = !var_name.is_null();

  // The name associated with this function. If it's a function expression,
  // this is the actual function name, otherwise this is the name of the
  // variable declared and initialized with the function (expression). In
  // that case, we don't have a function name (it's empty).
  Handle<String> name = is_named ? var_name : factory()->EmptySymbol();
  // The function name, if any.
  Handle<String> function_name = factory()->EmptySymbol();
  if (is_named && (type == EXPRESSION || type == NESTED)) {
    function_name = name;
  }

  int num_parameters = 0;
  // Parse function body.
  { Scope::Type type = Scope::FUNCTION_SCOPE;
    Scope* scope = factory()->NewScope(top_scope_, type, inside_with());
    LexicalScope lexical_scope(this, scope);
    TemporaryScope temp_scope(this);
    top_scope_->SetScopeName(name);

    //  FormalParameterList ::
    //    '(' (Identifier)*[','] ')'
    Expect(Token::LPAREN, CHECK_OK);
    int start_pos = scanner_.location().beg_pos;
    bool done = (peek() == Token::RPAREN);
    while (!done) {
      Handle<String> param_name = ParseIdentifier(CHECK_OK);
      if (!is_pre_parsing_) {
        top_scope_->AddParameter(top_scope_->DeclareLocal(param_name,
                                                          Variable::VAR));
        num_parameters++;
      }
      done = (peek() == Token::RPAREN);
      if (!done) Expect(Token::COMMA, CHECK_OK);
    }
    Expect(Token::RPAREN, CHECK_OK);

    Expect(Token::LBRACE, CHECK_OK);
    ZoneListWrapper<Statement> body = factory()->NewList<Statement>(8);

    // If we have a named function expression, we add a local variable
    // declaration to the body of the function with the name of the
    // function and let it refer to the function itself (closure).
    // NOTE: We create a proxy and resolve it here so that in the
    // future we can change the AST to only refer to VariableProxies
    // instead of Variables and Proxis as is the case now.
    if (!function_name.is_null() && function_name->length() > 0) {
      Variable* fvar = top_scope_->DeclareFunctionVar(function_name);
      VariableProxy* fproxy =
          top_scope_->NewUnresolved(function_name, inside_with());
      fproxy->BindTo(fvar);
      body.Add(new ExpressionStatement(
                   new Assignment(Token::INIT_CONST, fproxy,
                                  NEW(ThisFunction()),
                                  RelocInfo::kNoPosition)));
    }

    // Determine if the function will be lazily compiled. The mode can
    // only be PARSE_LAZILY if the --lazy flag is true.
    bool is_lazily_compiled =
        mode() == PARSE_LAZILY && top_scope_->HasTrivialOuterContext();

    int materialized_literal_count;
    int expected_property_count;
    bool only_simple_this_property_assignments;
    Handle<FixedArray> this_property_assignments;
    if (is_lazily_compiled && pre_data() != NULL) {
      FunctionEntry entry = pre_data()->GetFunctionEnd(start_pos);
      if (!entry.is_valid()) {
        ReportInvalidPreparseData(name, CHECK_OK);
      }
      int end_pos = entry.end_pos();
      if (end_pos <= start_pos) {
        // End position greater than end of stream is safe, and hard to check.
        ReportInvalidPreparseData(name, CHECK_OK);
      }
      Counters::total_preparse_skipped.Increment(end_pos - start_pos);
      scanner_.SeekForward(end_pos);
      materialized_literal_count = entry.literal_count();
      expected_property_count = entry.property_count();
      only_simple_this_property_assignments = false;
      this_property_assignments = Factory::empty_fixed_array();
    } else {
      ParseSourceElements(&body, Token::RBRACE, CHECK_OK);
      materialized_literal_count = temp_scope.materialized_literal_count();
      expected_property_count = temp_scope.expected_property_count();
      only_simple_this_property_assignments =
          temp_scope.only_simple_this_property_assignments();
      this_property_assignments = temp_scope.this_property_assignments();
    }

    Expect(Token::RBRACE, CHECK_OK);
    int end_pos = scanner_.location().end_pos;

    FunctionEntry entry = log()->LogFunction(start_pos);
    if (entry.is_valid()) {
      entry.set_end_pos(end_pos);
      entry.set_literal_count(materialized_literal_count);
      entry.set_property_count(expected_property_count);
    }

    FunctionLiteral* function_literal =
        NEW(FunctionLiteral(name,
                            top_scope_,
                            body.elements(),
                            materialized_literal_count,
                            expected_property_count,
                            only_simple_this_property_assignments,
                            this_property_assignments,
                            num_parameters,
                            start_pos,
                            end_pos,
                            function_name->length() > 0));
    if (!is_pre_parsing_) {
      function_literal->set_function_token_position(function_token_position);
    }

    // Set flag for inner loop detection. We treat loops that contain a function
    // literal not as inner loops because we avoid duplicating function literals
    // when peeling or unrolling such a loop.
    seen_loop_stmt_ = true;

    return function_literal;
  }
}


Expression* Parser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments

  Expect(Token::MOD, CHECK_OK);
  Handle<String> name = ParseIdentifier(CHECK_OK);
  Runtime::Function* function =
      Runtime::FunctionForName(scanner_.literal_string());
  ZoneList<Expression*>* args = ParseArguments(CHECK_OK);
  if (function == NULL && extension_ != NULL) {
    // The extension structures are only accessible while parsing the
    // very first time not when reparsing because of lazy compilation.
    top_scope_->ForceEagerCompilation();
  }

  // Check for built-in macros.
  if (!is_pre_parsing_) {
    if (function == Runtime::FunctionForId(Runtime::kIS_VAR)) {
      // %IS_VAR(x)
      //   evaluates to x if x is a variable,
      //   leads to a parse error otherwise
      if (args->length() == 1 && args->at(0)->AsVariableProxy() != NULL) {
        return args->at(0);
      }
      *ok = false;
    // Check here for other macros.
    // } else if (function == Runtime::FunctionForId(Runtime::kIS_VAR)) {
    //   ...
    }

    if (!*ok) {
      // We found a macro but it failed.
      ReportMessage("unable_to_parse", Vector<const char*>::empty());
      return NULL;
    }
  }

  // Check that the expected number arguments are passed to runtime functions.
  if (!is_pre_parsing_) {
    if (function != NULL
        && function->nargs != -1
        && function->nargs != args->length()) {
      ReportMessage("illegal_access", Vector<const char*>::empty());
      *ok = false;
      return NULL;
    } else if (function == NULL && !name.is_null()) {
      // If this is not a runtime function implemented in C++ it might be an
      // inlined runtime function.
      int argc = CodeGenerator::InlineRuntimeCallArgumentsCount(name);
      if (argc != -1 && argc != args->length()) {
        ReportMessage("illegal_access", Vector<const char*>::empty());
        *ok = false;
        return NULL;
      }
    }
  }

  // Otherwise we have a valid runtime call.
  return NEW(CallRuntime(name, function, args));
}


void Parser::Consume(Token::Value token) {
  Token::Value next = Next();
  USE(next);
  USE(token);
  ASSERT(next == token);
}


void Parser::Expect(Token::Value token, bool* ok) {
  Token::Value next = Next();
  if (next == token) return;
  ReportUnexpectedToken(next);
  *ok = false;
}


bool Parser::Check(Token::Value token) {
  Token::Value next = peek();
  if (next == token) {
    Consume(next);
    return true;
  }
  return false;
}


void Parser::ExpectSemicolon(bool* ok) {
  // Check for automatic semicolon insertion according to
  // the rules given in ECMA-262, section 7.9, page 21.
  Token::Value tok = peek();
  if (tok == Token::SEMICOLON) {
    Next();
    return;
  }
  if (scanner_.has_line_terminator_before_next() ||
      tok == Token::RBRACE ||
      tok == Token::EOS) {
    return;
  }
  Expect(Token::SEMICOLON, ok);
}


Literal* Parser::GetLiteralUndefined() {
  return NEW(Literal(Factory::undefined_value()));
}


Literal* Parser::GetLiteralTheHole() {
  return NEW(Literal(Factory::the_hole_value()));
}


Literal* Parser::GetLiteralNumber(double value) {
  return NewNumberLiteral(value);
}


Handle<String> Parser::ParseIdentifier(bool* ok) {
  Expect(Token::IDENTIFIER, ok);
  if (!*ok) return Handle<String>();
  return factory()->LookupSymbol(scanner_.literal_string(),
                                 scanner_.literal_length());
}

// This function reads an identifier and determines whether or not it
// is 'get' or 'set'.  The reason for not using ParseIdentifier and
// checking on the output is that this involves heap allocation which
// we can't do during preparsing.
Handle<String> Parser::ParseIdentifierOrGetOrSet(bool* is_get,
                                                 bool* is_set,
                                                 bool* ok) {
  Expect(Token::IDENTIFIER, ok);
  if (!*ok) return Handle<String>();
  if (scanner_.literal_length() == 3) {
    const char* token = scanner_.literal_string();
    *is_get = strcmp(token, "get") == 0;
    *is_set = !*is_get && strcmp(token, "set") == 0;
  }
  return factory()->LookupSymbol(scanner_.literal_string(),
                                 scanner_.literal_length());
}


// ----------------------------------------------------------------------------
// Parser support


bool Parser::TargetStackContainsLabel(Handle<String> label) {
  for (Target* t = target_stack_; t != NULL; t = t->previous()) {
    BreakableStatement* stat = t->node()->AsBreakableStatement();
    if (stat != NULL && ContainsLabel(stat->labels(), label))
      return true;
  }
  return false;
}


BreakableStatement* Parser::LookupBreakTarget(Handle<String> label, bool* ok) {
  bool anonymous = label.is_null();
  for (Target* t = target_stack_; t != NULL; t = t->previous()) {
    BreakableStatement* stat = t->node()->AsBreakableStatement();
    if (stat == NULL) continue;
    if ((anonymous && stat->is_target_for_anonymous()) ||
        (!anonymous && ContainsLabel(stat->labels(), label))) {
      RegisterTargetUse(stat->break_target(), t->previous());
      return stat;
    }
  }
  return NULL;
}


IterationStatement* Parser::LookupContinueTarget(Handle<String> label,
                                                 bool* ok) {
  bool anonymous = label.is_null();
  for (Target* t = target_stack_; t != NULL; t = t->previous()) {
    IterationStatement* stat = t->node()->AsIterationStatement();
    if (stat == NULL) continue;

    ASSERT(stat->is_target_for_anonymous());
    if (anonymous || ContainsLabel(stat->labels(), label)) {
      RegisterTargetUse(stat->continue_target(), t->previous());
      return stat;
    }
  }
  return NULL;
}


void Parser::RegisterTargetUse(BreakTarget* target, Target* stop) {
  // Register that a break target found at the given stop in the
  // target stack has been used from the top of the target stack. Add
  // the break target to any TargetCollectors passed on the stack.
  for (Target* t = target_stack_; t != stop; t = t->previous()) {
    TargetCollector* collector = t->node()->AsTargetCollector();
    if (collector != NULL) collector->AddTarget(target);
  }
}


Literal* Parser::NewNumberLiteral(double number) {
  return NEW(Literal(Factory::NewNumber(number, TENURED)));
}


Expression* Parser::NewThrowReferenceError(Handle<String> type) {
  return NewThrowError(Factory::MakeReferenceError_symbol(),
                       type, HandleVector<Object>(NULL, 0));
}


Expression* Parser::NewThrowSyntaxError(Handle<String> type,
                                        Handle<Object> first) {
  int argc = first.is_null() ? 0 : 1;
  Vector< Handle<Object> > arguments = HandleVector<Object>(&first, argc);
  return NewThrowError(Factory::MakeSyntaxError_symbol(), type, arguments);
}


Expression* Parser::NewThrowTypeError(Handle<String> type,
                                      Handle<Object> first,
                                      Handle<Object> second) {
  ASSERT(!first.is_null() && !second.is_null());
  Handle<Object> elements[] = { first, second };
  Vector< Handle<Object> > arguments =
      HandleVector<Object>(elements, ARRAY_SIZE(elements));
  return NewThrowError(Factory::MakeTypeError_symbol(), type, arguments);
}


Expression* Parser::NewThrowError(Handle<String> constructor,
                                  Handle<String> type,
                                  Vector< Handle<Object> > arguments) {
  if (is_pre_parsing_) return NULL;

  int argc = arguments.length();
  Handle<JSArray> array = Factory::NewJSArray(argc, TENURED);
  ASSERT(array->IsJSArray() && array->HasFastElements());
  for (int i = 0; i < argc; i++) {
    Handle<Object> element = arguments[i];
    if (!element.is_null()) {
      array->SetFastElement(i, *element);
    }
  }
  ZoneList<Expression*>* args = new ZoneList<Expression*>(2);
  args->Add(new Literal(type));
  args->Add(new Literal(array));
  return new Throw(new CallRuntime(constructor, NULL, args),
                   scanner().location().beg_pos);
}

// ----------------------------------------------------------------------------
// JSON

Expression* Parser::ParseJson(bool* ok) {
  Expression* result = ParseJsonValue(CHECK_OK);
  Expect(Token::EOS, CHECK_OK);
  return result;
}


// Parse any JSON value.
Expression* Parser::ParseJsonValue(bool* ok) {
  Token::Value token = peek();
  switch (token) {
    case Token::STRING: {
      Consume(Token::STRING);
      int literal_length = scanner_.literal_length();
      const char* literal_string = scanner_.literal_string();
      if (literal_length == 0) {
        return NEW(Literal(Factory::empty_string()));
      }
      Vector<const char> literal(literal_string, literal_length);
      return NEW(Literal(Factory::NewStringFromUtf8(literal, TENURED)));
    }
    case Token::NUMBER: {
      Consume(Token::NUMBER);
      ASSERT(scanner_.literal_length() > 0);
      double value = StringToDouble(scanner_.literal_string(),
                                    NO_FLAGS,  // Hex, octal or trailing junk.
                                    OS::nan_value());
      return NewNumberLiteral(value);
    }
    case Token::FALSE_LITERAL:
      Consume(Token::FALSE_LITERAL);
      return NEW(Literal(Factory::false_value()));
    case Token::TRUE_LITERAL:
      Consume(Token::TRUE_LITERAL);
      return NEW(Literal(Factory::true_value()));
    case Token::NULL_LITERAL:
      Consume(Token::NULL_LITERAL);
      return NEW(Literal(Factory::null_value()));
    case Token::LBRACE: {
      Expression* result = ParseJsonObject(CHECK_OK);
      return result;
    }
    case Token::LBRACK: {
      Expression* result = ParseJsonArray(CHECK_OK);
      return result;
    }
    default:
      *ok = false;
      ReportUnexpectedToken(token);
      return NULL;
  }
}


// Parse a JSON object. Scanner must be right after '{' token.
Expression* Parser::ParseJsonObject(bool* ok) {
  Consume(Token::LBRACE);
  ZoneListWrapper<ObjectLiteral::Property> properties =
      factory()->NewList<ObjectLiteral::Property>(4);
  int boilerplate_properties = 0;
  if (peek() != Token::RBRACE) {
    do {
      Expect(Token::STRING, CHECK_OK);
      Handle<String> key = factory()->LookupSymbol(scanner_.literal_string(),
                                                   scanner_.literal_length());
      Expect(Token::COLON, CHECK_OK);
      Expression* value = ParseJsonValue(CHECK_OK);
      Literal* key_literal;
      uint32_t index;
      if (key->AsArrayIndex(&index)) {
        key_literal = NewNumberLiteral(index);
      } else {
        key_literal = NEW(Literal(key));
      }
      ObjectLiteral::Property* property =
          NEW(ObjectLiteral::Property(key_literal, value));
      properties.Add(property);

      if (IsBoilerplateProperty(property)) {
        boilerplate_properties++;
      }
    } while (Check(Token::COMMA));
  }
  Expect(Token::RBRACE, CHECK_OK);

  int literal_index = temp_scope_->NextMaterializedLiteralIndex();
  if (is_pre_parsing_) return NULL;

  Handle<FixedArray> constant_properties =
        Factory::NewFixedArray(boilerplate_properties * 2, TENURED);
  bool is_simple = true;
  bool fast_elements = true;
  int depth = 1;
  BuildObjectLiteralConstantProperties(properties.elements(),
                                       constant_properties,
                                       &is_simple,
                                       &fast_elements,
                                       &depth);
  return new ObjectLiteral(constant_properties,
                           properties.elements(),
                           literal_index,
                           is_simple,
                           fast_elements,
                           depth);
}


// Parse a JSON array. Scanner must be right after '[' token.
Expression* Parser::ParseJsonArray(bool* ok) {
  Consume(Token::LBRACK);

  ZoneListWrapper<Expression> values = factory()->NewList<Expression>(4);
  if (peek() != Token::RBRACK) {
    do {
      Expression* exp = ParseJsonValue(CHECK_OK);
      values.Add(exp);
    } while (Check(Token::COMMA));
  }
  Expect(Token::RBRACK, CHECK_OK);

  // Update the scope information before the pre-parsing bailout.
  int literal_index = temp_scope_->NextMaterializedLiteralIndex();

  if (is_pre_parsing_) return NULL;

  // Allocate a fixed array with all the literals.
  Handle<FixedArray> literals =
      Factory::NewFixedArray(values.length(), TENURED);

  bool is_simple;
  int depth;
  BuildArrayLiteralBoilerplateLiterals(values.elements(),
                                       literals,
                                       &is_simple,
                                       &depth);
  return NEW(ArrayLiteral(literals, values.elements(),
                          literal_index, is_simple, depth));
}


// ----------------------------------------------------------------------------
// Regular expressions


RegExpParser::RegExpParser(FlatStringReader* in,
                           Handle<String>* error,
                           bool multiline)
  : current_(kEndMarker),
    has_more_(true),
    multiline_(multiline),
    next_pos_(0),
    in_(in),
    error_(error),
    simple_(false),
    contains_anchor_(false),
    captures_(NULL),
    is_scanned_for_captures_(false),
    capture_count_(0),
    failed_(false) {
  Advance(1);
}


uc32 RegExpParser::Next() {
  if (has_next()) {
    return in()->Get(next_pos_);
  } else {
    return kEndMarker;
  }
}


void RegExpParser::Advance() {
  if (next_pos_ < in()->length()) {
    StackLimitCheck check;
    if (check.HasOverflowed()) {
      ReportError(CStrVector(Top::kStackOverflowMessage));
    } else if (Zone::excess_allocation()) {
      ReportError(CStrVector("Regular expression too large"));
    } else {
      current_ = in()->Get(next_pos_);
      next_pos_++;
    }
  } else {
    current_ = kEndMarker;
    has_more_ = false;
  }
}


void RegExpParser::Reset(int pos) {
  next_pos_ = pos;
  Advance();
}


void RegExpParser::Advance(int dist) {
  for (int i = 0; i < dist; i++)
    Advance();
}


bool RegExpParser::simple() {
  return simple_;
}

RegExpTree* RegExpParser::ReportError(Vector<const char> message) {
  failed_ = true;
  *error_ = Factory::NewStringFromAscii(message, NOT_TENURED);
  // Zip to the end to make sure the no more input is read.
  current_ = kEndMarker;
  next_pos_ = in()->length();
  return NULL;
}


// Pattern ::
//   Disjunction
RegExpTree* RegExpParser::ParsePattern() {
  RegExpTree* result = ParseDisjunction(CHECK_FAILED);
  ASSERT(!has_more());
  // If the result of parsing is a literal string atom, and it has the
  // same length as the input, then the atom is identical to the input.
  if (result->IsAtom() && result->AsAtom()->length() == in()->length()) {
    simple_ = true;
  }
  return result;
}


// Disjunction ::
//   Alternative
//   Alternative | Disjunction
// Alternative ::
//   [empty]
//   Term Alternative
// Term ::
//   Assertion
//   Atom
//   Atom Quantifier
RegExpTree* RegExpParser::ParseDisjunction() {
  // Used to store current state while parsing subexpressions.
  RegExpParserState initial_state(NULL, INITIAL, 0);
  RegExpParserState* stored_state = &initial_state;
  // Cache the builder in a local variable for quick access.
  RegExpBuilder* builder = initial_state.builder();
  while (true) {
    switch (current()) {
    case kEndMarker:
      if (stored_state->IsSubexpression()) {
        // Inside a parenthesized group when hitting end of input.
        ReportError(CStrVector("Unterminated group") CHECK_FAILED);
      }
      ASSERT_EQ(INITIAL, stored_state->group_type());
      // Parsing completed successfully.
      return builder->ToRegExp();
    case ')': {
      if (!stored_state->IsSubexpression()) {
        ReportError(CStrVector("Unmatched ')'") CHECK_FAILED);
      }
      ASSERT_NE(INITIAL, stored_state->group_type());

      Advance();
      // End disjunction parsing and convert builder content to new single
      // regexp atom.
      RegExpTree* body = builder->ToRegExp();

      int end_capture_index = captures_started();

      int capture_index = stored_state->capture_index();
      SubexpressionType type = stored_state->group_type();

      // Restore previous state.
      stored_state = stored_state->previous_state();
      builder = stored_state->builder();

      // Build result of subexpression.
      if (type == CAPTURE) {
        RegExpCapture* capture = new RegExpCapture(body, capture_index);
        captures_->at(capture_index - 1) = capture;
        body = capture;
      } else if (type != GROUPING) {
        ASSERT(type == POSITIVE_LOOKAHEAD || type == NEGATIVE_LOOKAHEAD);
        bool is_positive = (type == POSITIVE_LOOKAHEAD);
        body = new RegExpLookahead(body,
                                   is_positive,
                                   end_capture_index - capture_index,
                                   capture_index);
      }
      builder->AddAtom(body);
      break;
    }
    case '|': {
      Advance();
      builder->NewAlternative();
      continue;
    }
    case '*':
    case '+':
    case '?':
      return ReportError(CStrVector("Nothing to repeat"));
    case '^': {
      Advance();
      if (multiline_) {
        builder->AddAssertion(
            new RegExpAssertion(RegExpAssertion::START_OF_LINE));
      } else {
        builder->AddAssertion(
            new RegExpAssertion(RegExpAssertion::START_OF_INPUT));
        set_contains_anchor();
      }
      continue;
    }
    case '$': {
      Advance();
      RegExpAssertion::Type type =
          multiline_ ? RegExpAssertion::END_OF_LINE :
                       RegExpAssertion::END_OF_INPUT;
      builder->AddAssertion(new RegExpAssertion(type));
      continue;
    }
    case '.': {
      Advance();
      // everything except \x0a, \x0d, \u2028 and \u2029
      ZoneList<CharacterRange>* ranges = new ZoneList<CharacterRange>(2);
      CharacterRange::AddClassEscape('.', ranges);
      RegExpTree* atom = new RegExpCharacterClass(ranges, false);
      builder->AddAtom(atom);
      break;
    }
    case '(': {
      SubexpressionType type = CAPTURE;
      Advance();
      if (current() == '?') {
        switch (Next()) {
          case ':':
            type = GROUPING;
            break;
          case '=':
            type = POSITIVE_LOOKAHEAD;
            break;
          case '!':
            type = NEGATIVE_LOOKAHEAD;
            break;
          default:
            ReportError(CStrVector("Invalid group") CHECK_FAILED);
            break;
        }
        Advance(2);
      } else {
        if (captures_ == NULL) {
          captures_ = new ZoneList<RegExpCapture*>(2);
        }
        if (captures_started() >= kMaxCaptures) {
          ReportError(CStrVector("Too many captures") CHECK_FAILED);
        }
        captures_->Add(NULL);
      }
      // Store current state and begin new disjunction parsing.
      stored_state = new RegExpParserState(stored_state,
                                           type,
                                           captures_started());
      builder = stored_state->builder();
      break;
    }
    case '[': {
      RegExpTree* atom = ParseCharacterClass(CHECK_FAILED);
      builder->AddAtom(atom);
      break;
    }
    // Atom ::
    //   \ AtomEscape
    case '\\':
      switch (Next()) {
      case kEndMarker:
        return ReportError(CStrVector("\\ at end of pattern"));
      case 'b':
        Advance(2);
        builder->AddAssertion(
            new RegExpAssertion(RegExpAssertion::BOUNDARY));
        continue;
      case 'B':
        Advance(2);
        builder->AddAssertion(
            new RegExpAssertion(RegExpAssertion::NON_BOUNDARY));
        continue;
        // AtomEscape ::
        //   CharacterClassEscape
        //
        // CharacterClassEscape :: one of
        //   d D s S w W
      case 'd': case 'D': case 's': case 'S': case 'w': case 'W': {
        uc32 c = Next();
        Advance(2);
        ZoneList<CharacterRange>* ranges = new ZoneList<CharacterRange>(2);
        CharacterRange::AddClassEscape(c, ranges);
        RegExpTree* atom = new RegExpCharacterClass(ranges, false);
        builder->AddAtom(atom);
        break;
      }
      case '1': case '2': case '3': case '4': case '5': case '6':
      case '7': case '8': case '9': {
        int index = 0;
        if (ParseBackReferenceIndex(&index)) {
          RegExpCapture* capture = NULL;
          if (captures_ != NULL && index <= captures_->length()) {
            capture = captures_->at(index - 1);
          }
          if (capture == NULL) {
            builder->AddEmpty();
            break;
          }
          RegExpTree* atom = new RegExpBackReference(capture);
          builder->AddAtom(atom);
          break;
        }
        uc32 first_digit = Next();
        if (first_digit == '8' || first_digit == '9') {
          // Treat as identity escape
          builder->AddCharacter(first_digit);
          Advance(2);
          break;
        }
      }
      // FALLTHROUGH
      case '0': {
        Advance();
        uc32 octal = ParseOctalLiteral();
        builder->AddCharacter(octal);
        break;
      }
      // ControlEscape :: one of
      //   f n r t v
      case 'f':
        Advance(2);
        builder->AddCharacter('\f');
        break;
      case 'n':
        Advance(2);
        builder->AddCharacter('\n');
        break;
      case 'r':
        Advance(2);
        builder->AddCharacter('\r');
        break;
      case 't':
        Advance(2);
        builder->AddCharacter('\t');
        break;
      case 'v':
        Advance(2);
        builder->AddCharacter('\v');
        break;
      case 'c': {
        Advance(2);
        uc32 control = ParseControlLetterEscape();
        builder->AddCharacter(control);
        break;
      }
      case 'x': {
        Advance(2);
        uc32 value;
        if (ParseHexEscape(2, &value)) {
          builder->AddCharacter(value);
        } else {
          builder->AddCharacter('x');
        }
        break;
      }
      case 'u': {
        Advance(2);
        uc32 value;
        if (ParseHexEscape(4, &value)) {
          builder->AddCharacter(value);
        } else {
          builder->AddCharacter('u');
        }
        break;
      }
      default:
        // Identity escape.
        builder->AddCharacter(Next());
        Advance(2);
        break;
      }
      break;
    case '{': {
      int dummy;
      if (ParseIntervalQuantifier(&dummy, &dummy)) {
        ReportError(CStrVector("Nothing to repeat") CHECK_FAILED);
      }
      // fallthrough
    }
    default:
      builder->AddCharacter(current());
      Advance();
      break;
    }  // end switch(current())

    int min;
    int max;
    switch (current()) {
    // QuantifierPrefix ::
    //   *
    //   +
    //   ?
    //   {
    case '*':
      min = 0;
      max = RegExpTree::kInfinity;
      Advance();
      break;
    case '+':
      min = 1;
      max = RegExpTree::kInfinity;
      Advance();
      break;
    case '?':
      min = 0;
      max = 1;
      Advance();
      break;
    case '{':
      if (ParseIntervalQuantifier(&min, &max)) {
        if (max < min) {
          ReportError(CStrVector("numbers out of order in {} quantifier.")
                      CHECK_FAILED);
        }
        break;
      } else {
        continue;
      }
    default:
      continue;
    }
    RegExpQuantifier::Type type = RegExpQuantifier::GREEDY;
    if (current() == '?') {
      type = RegExpQuantifier::NON_GREEDY;
      Advance();
    } else if (FLAG_regexp_possessive_quantifier && current() == '+') {
      // FLAG_regexp_possessive_quantifier is a debug-only flag.
      type = RegExpQuantifier::POSSESSIVE;
      Advance();
    }
    builder->AddQuantifierToAtom(min, max, type);
  }
}

class SourceCharacter {
 public:
  static bool Is(uc32 c) {
    switch (c) {
      // case ']': case '}':
      // In spidermonkey and jsc these are treated as source characters
      // so we do too.
      case '^': case '$': case '\\': case '.': case '*': case '+':
      case '?': case '(': case ')': case '[': case '{': case '|':
      case RegExpParser::kEndMarker:
        return false;
      default:
        return true;
    }
  }
};


static unibrow::Predicate<SourceCharacter> source_character;


static inline bool IsSourceCharacter(uc32 c) {
  return source_character.get(c);
}

#ifdef DEBUG
// Currently only used in an ASSERT.
static bool IsSpecialClassEscape(uc32 c) {
  switch (c) {
    case 'd': case 'D':
    case 's': case 'S':
    case 'w': case 'W':
      return true;
    default:
      return false;
  }
}
#endif


// In order to know whether an escape is a backreference or not we have to scan
// the entire regexp and find the number of capturing parentheses.  However we
// don't want to scan the regexp twice unless it is necessary.  This mini-parser
// is called when needed.  It can see the difference between capturing and
// noncapturing parentheses and can skip character classes and backslash-escaped
// characters.
void RegExpParser::ScanForCaptures() {
  // Start with captures started previous to current position
  int capture_count = captures_started();
  // Add count of captures after this position.
  int n;
  while ((n = current()) != kEndMarker) {
    Advance();
    switch (n) {
      case '\\':
        Advance();
        break;
      case '[': {
        int c;
        while ((c = current()) != kEndMarker) {
          Advance();
          if (c == '\\') {
            Advance();
          } else {
            if (c == ']') break;
          }
        }
        break;
      }
      case '(':
        if (current() != '?') capture_count++;
        break;
    }
  }
  capture_count_ = capture_count;
  is_scanned_for_captures_ = true;
}


bool RegExpParser::ParseBackReferenceIndex(int* index_out) {
  ASSERT_EQ('\\', current());
  ASSERT('1' <= Next() && Next() <= '9');
  // Try to parse a decimal literal that is no greater than the total number
  // of left capturing parentheses in the input.
  int start = position();
  int value = Next() - '0';
  Advance(2);
  while (true) {
    uc32 c = current();
    if (IsDecimalDigit(c)) {
      value = 10 * value + (c - '0');
      if (value > kMaxCaptures) {
        Reset(start);
        return false;
      }
      Advance();
    } else {
      break;
    }
  }
  if (value > captures_started()) {
    if (!is_scanned_for_captures_) {
      int saved_position = position();
      ScanForCaptures();
      Reset(saved_position);
    }
    if (value > capture_count_) {
      Reset(start);
      return false;
    }
  }
  *index_out = value;
  return true;
}


// QuantifierPrefix ::
//   { DecimalDigits }
//   { DecimalDigits , }
//   { DecimalDigits , DecimalDigits }
//
// Returns true if parsing succeeds, and set the min_out and max_out
// values. Values are truncated to RegExpTree::kInfinity if they overflow.
bool RegExpParser::ParseIntervalQuantifier(int* min_out, int* max_out) {
  ASSERT_EQ(current(), '{');
  int start = position();
  Advance();
  int min = 0;
  if (!IsDecimalDigit(current())) {
    Reset(start);
    return false;
  }
  while (IsDecimalDigit(current())) {
    int next = current() - '0';
    if (min > (RegExpTree::kInfinity - next) / 10) {
      // Overflow. Skip past remaining decimal digits and return -1.
      do {
        Advance();
      } while (IsDecimalDigit(current()));
      min = RegExpTree::kInfinity;
      break;
    }
    min = 10 * min + next;
    Advance();
  }
  int max = 0;
  if (current() == '}') {
    max = min;
    Advance();
  } else if (current() == ',') {
    Advance();
    if (current() == '}') {
      max = RegExpTree::kInfinity;
      Advance();
    } else {
      while (IsDecimalDigit(current())) {
        int next = current() - '0';
        if (max > (RegExpTree::kInfinity - next) / 10) {
          do {
            Advance();
          } while (IsDecimalDigit(current()));
          max = RegExpTree::kInfinity;
          break;
        }
        max = 10 * max + next;
        Advance();
      }
      if (current() != '}') {
        Reset(start);
        return false;
      }
      Advance();
    }
  } else {
    Reset(start);
    return false;
  }
  *min_out = min;
  *max_out = max;
  return true;
}


// Upper and lower case letters differ by one bit.
STATIC_CHECK(('a' ^ 'A') == 0x20);

uc32 RegExpParser::ParseControlLetterEscape() {
  if (!has_more())
    return 'c';
  uc32 letter = current() & ~(0x20);  // Collapse upper and lower case letters.
  if (letter < 'A' || 'Z' < letter) {
    // Non-spec error-correction: "\c" followed by non-control letter is
    // interpreted as an IdentityEscape of 'c'.
    return 'c';
  }
  Advance();
  return letter & 0x1f;  // Remainder modulo 32, per specification.
}


uc32 RegExpParser::ParseOctalLiteral() {
  ASSERT('0' <= current() && current() <= '7');
  // For compatibility with some other browsers (not all), we parse
  // up to three octal digits with a value below 256.
  uc32 value = current() - '0';
  Advance();
  if ('0' <= current() && current() <= '7') {
    value = value * 8 + current() - '0';
    Advance();
    if (value < 32 && '0' <= current() && current() <= '7') {
      value = value * 8 + current() - '0';
      Advance();
    }
  }
  return value;
}


bool RegExpParser::ParseHexEscape(int length, uc32 *value) {
  int start = position();
  uc32 val = 0;
  bool done = false;
  for (int i = 0; !done; i++) {
    uc32 c = current();
    int d = HexValue(c);
    if (d < 0) {
      Reset(start);
      return false;
    }
    val = val * 16 + d;
    Advance();
    if (i == length - 1) {
      done = true;
    }
  }
  *value = val;
  return true;
}


uc32 RegExpParser::ParseClassCharacterEscape() {
  ASSERT(current() == '\\');
  ASSERT(has_next() && !IsSpecialClassEscape(Next()));
  Advance();
  switch (current()) {
    case 'b':
      Advance();
      return '\b';
    // ControlEscape :: one of
    //   f n r t v
    case 'f':
      Advance();
      return '\f';
    case 'n':
      Advance();
      return '\n';
    case 'r':
      Advance();
      return '\r';
    case 't':
      Advance();
      return '\t';
    case 'v':
      Advance();
      return '\v';
    case 'c':
      Advance();
      return ParseControlLetterEscape();
    case '0': case '1': case '2': case '3': case '4': case '5':
    case '6': case '7':
      // For compatibility, we interpret a decimal escape that isn't
      // a back reference (and therefore either \0 or not valid according
      // to the specification) as a 1..3 digit octal character code.
      return ParseOctalLiteral();
    case 'x': {
      Advance();
      uc32 value;
      if (ParseHexEscape(2, &value)) {
        return value;
      }
      // If \x is not followed by a two-digit hexadecimal, treat it
      // as an identity escape.
      return 'x';
    }
    case 'u': {
      Advance();
      uc32 value;
      if (ParseHexEscape(4, &value)) {
        return value;
      }
      // If \u is not followed by a four-digit hexadecimal, treat it
      // as an identity escape.
      return 'u';
    }
    default: {
      // Extended identity escape. We accept any character that hasn't
      // been matched by a more specific case, not just the subset required
      // by the ECMAScript specification.
      uc32 result = current();
      Advance();
      return result;
    }
  }
  return 0;
}


CharacterRange RegExpParser::ParseClassAtom(uc16* char_class) {
  ASSERT_EQ(0, *char_class);
  uc32 first = current();
  if (first == '\\') {
    switch (Next()) {
      case 'w': case 'W': case 'd': case 'D': case 's': case 'S': {
        *char_class = Next();
        Advance(2);
        return CharacterRange::Singleton(0);  // Return dummy value.
      }
      case kEndMarker:
        return ReportError(CStrVector("\\ at end of pattern"));
      default:
        uc32 c = ParseClassCharacterEscape(CHECK_FAILED);
        return CharacterRange::Singleton(c);
    }
  } else {
    Advance();
    return CharacterRange::Singleton(first);
  }
}


RegExpTree* RegExpParser::ParseCharacterClass() {
  static const char* kUnterminated = "Unterminated character class";
  static const char* kRangeOutOfOrder = "Range out of order in character class";

  ASSERT_EQ(current(), '[');
  Advance();
  bool is_negated = false;
  if (current() == '^') {
    is_negated = true;
    Advance();
  }
  ZoneList<CharacterRange>* ranges = new ZoneList<CharacterRange>(2);
  while (has_more() && current() != ']') {
    uc16 char_class = 0;
    CharacterRange first = ParseClassAtom(&char_class CHECK_FAILED);
    if (char_class) {
      CharacterRange::AddClassEscape(char_class, ranges);
      continue;
    }
    if (current() == '-') {
      Advance();
      if (current() == kEndMarker) {
        // If we reach the end we break out of the loop and let the
        // following code report an error.
        break;
      } else if (current() == ']') {
        ranges->Add(first);
        ranges->Add(CharacterRange::Singleton('-'));
        break;
      }
      CharacterRange next = ParseClassAtom(&char_class CHECK_FAILED);
      if (char_class) {
        ranges->Add(first);
        ranges->Add(CharacterRange::Singleton('-'));
        CharacterRange::AddClassEscape(char_class, ranges);
        continue;
      }
      if (first.from() > next.to()) {
        return ReportError(CStrVector(kRangeOutOfOrder) CHECK_FAILED);
      }
      ranges->Add(CharacterRange::Range(first.from(), next.to()));
    } else {
      ranges->Add(first);
    }
  }
  if (!has_more()) {
    return ReportError(CStrVector(kUnterminated) CHECK_FAILED);
  }
  Advance();
  if (ranges->length() == 0) {
    ranges->Add(CharacterRange::Everything());
    is_negated = !is_negated;
  }
  return new RegExpCharacterClass(ranges, is_negated);
}


// ----------------------------------------------------------------------------
// The Parser interface.

// MakeAST() is just a wrapper for the corresponding Parser calls
// so we don't have to expose the entire Parser class in the .h file.

static bool always_allow_natives_syntax = false;


ParserMessage::~ParserMessage() {
  for (int i = 0; i < args().length(); i++)
    DeleteArray(args()[i]);
  DeleteArray(args().start());
}


ScriptDataImpl::~ScriptDataImpl() {
  store_.Dispose();
}


int ScriptDataImpl::Length() {
  return store_.length() * sizeof(unsigned);
}


const char* ScriptDataImpl::Data() {
  return reinterpret_cast<const char*>(store_.start());
}


bool ScriptDataImpl::HasError() {
  return has_error();
}


ScriptDataImpl* PreParse(Handle<String> source,
                         unibrow::CharacterStream* stream,
                         v8::Extension* extension) {
  Handle<Script> no_script;
  bool allow_natives_syntax =
      always_allow_natives_syntax ||
      FLAG_allow_natives_syntax ||
      Bootstrapper::IsActive();
  PreParser parser(no_script, allow_natives_syntax, extension);
  if (!parser.PreParseProgram(source, stream)) return NULL;
  // The list owns the backing store so we need to clone the vector.
  // That way, the result will be exactly the right size rather than
  // the expected 50% too large.
  Vector<unsigned> store = parser.recorder()->store()->ToVector().Clone();
  return new ScriptDataImpl(store);
}


bool ParseRegExp(FlatStringReader* input,
                 bool multiline,
                 RegExpCompileData* result) {
  ASSERT(result != NULL);
  RegExpParser parser(input, &result->error, multiline);
  RegExpTree* tree = parser.ParsePattern();
  if (parser.failed()) {
    ASSERT(tree == NULL);
    ASSERT(!result->error.is_null());
  } else {
    ASSERT(tree != NULL);
    ASSERT(result->error.is_null());
    result->tree = tree;
    int capture_count = parser.captures_started();
    result->simple = tree->IsAtom() && parser.simple() && capture_count == 0;
    result->contains_anchor = parser.contains_anchor();
    result->capture_count = capture_count;
  }
  return !parser.failed();
}


FunctionLiteral* MakeAST(bool compile_in_global_context,
                         Handle<Script> script,
                         v8::Extension* extension,
                         ScriptDataImpl* pre_data,
                         bool is_json) {
  bool allow_natives_syntax =
      always_allow_natives_syntax ||
      FLAG_allow_natives_syntax ||
      Bootstrapper::IsActive();
  AstBuildingParser parser(script, allow_natives_syntax, extension, pre_data);
  if (pre_data != NULL && pre_data->has_error()) {
    Scanner::Location loc = pre_data->MessageLocation();
    const char* message = pre_data->BuildMessage();
    Vector<const char*> args = pre_data->BuildArgs();
    parser.ReportMessageAt(loc, message, args);
    DeleteArray(message);
    for (int i = 0; i < args.length(); i++) {
      DeleteArray(args[i]);
    }
    DeleteArray(args.start());
    return NULL;
  }
  Handle<String> source = Handle<String>(String::cast(script->source()));
  FunctionLiteral* result;
  if (is_json) {
    ASSERT(compile_in_global_context);
    result = parser.ParseJson(source);
  } else {
    result = parser.ParseProgram(source, compile_in_global_context);
  }
  return result;
}


FunctionLiteral* MakeLazyAST(Handle<Script> script,
                             Handle<String> name,
                             int start_position,
                             int end_position,
                             bool is_expression) {
  bool allow_natives_syntax_before = always_allow_natives_syntax;
  always_allow_natives_syntax = true;
  AstBuildingParser parser(script, true, NULL, NULL);  // always allow
  always_allow_natives_syntax = allow_natives_syntax_before;
  // Parse the function by pointing to the function source in the script source.
  Handle<String> script_source(String::cast(script->source()));
  FunctionLiteral* result =
      parser.ParseLazy(script_source, name,
                       start_position, end_position, is_expression);
  return result;
}


#undef NEW


} }  // namespace v8::internal
