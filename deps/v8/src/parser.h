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

namespace v8 {
namespace internal {

class FuncNameInferrer;
class ParserFactory;
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
    return (store_.length() > kHeaderSize) ? store_[kSymbolCountOffset] : 0;
  }
  // The following functions should only be called if SanityCheck has
  // returned true.
  bool has_error() { return store_[kHasErrorOffset]; }
  unsigned magic() { return store_[kMagicOffset]; }
  unsigned version() { return store_[kVersionOffset]; }

  static const unsigned kMagicNumber = 0xBadDead;
  static const unsigned kCurrentVersion = 4;

  static const int kMagicOffset = 0;
  static const int kVersionOffset = 1;
  static const int kHasErrorOffset = 2;
  static const int kFunctionsSizeOffset = 3;
  static const int kSymbolCountOffset = 4;
  static const int kSizeOffset = 5;
  static const int kHeaderSize = 6;

  // If encoding a message, the following positions are fixed.
  static const int kMessageStartPos = 0;
  static const int kMessageEndPos = 1;
  static const int kMessageArgCountPos = 2;
  static const int kMessageTextPos = 3;

  static const byte kNumberTerminator = 0x80u;

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


class Parser {
 public:
  Parser(Handle<Script> script, bool allow_natives_syntax,
         v8::Extension* extension, ParserMode is_pre_parsing,
         ParserFactory* factory, ParserLog* log, ScriptDataImpl* pre_data);
  virtual ~Parser() { }

  // Takes a script and and context information, and builds a
  // FunctionLiteral AST node. Returns NULL and deallocates any allocated
  // AST nodes if parsing failed.
  static FunctionLiteral* MakeAST(bool compile_in_global_context,
                                  Handle<Script> script,
                                  v8::Extension* extension,
                                  ScriptDataImpl* pre_data,
                                  bool is_json = false);

  // Support for doing lazy compilation.
  static FunctionLiteral* MakeLazyAST(Handle<SharedFunctionInfo> info);

  // Generic preparser generating full preparse data.
  static ScriptDataImpl* PreParse(Handle<String> source,
                                  unibrow::CharacterStream* stream,
                                  v8::Extension* extension);

  // Preparser that only does preprocessing that makes sense if only used
  // immediately after.
  static ScriptDataImpl* PartialPreParse(Handle<String> source,
                                         unibrow::CharacterStream* stream,
                                         v8::Extension* extension);

  static bool ParseRegExp(FlatStringReader* input,
                          bool multiline,
                          RegExpCompileData* result);

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
  FunctionLiteral* ParseLazy(Handle<SharedFunctionInfo> info);
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
  FuncNameInferrer* fni_;

  bool inside_with() const { return with_nesting_level_ > 0; }
  ParserFactory* factory() const { return factory_; }
  ParserLog* log() const { return log_; }
  Scanner& scanner()  { return scanner_; }
  Mode mode() const { return mode_; }
  ScriptDataImpl* pre_data() const { return pre_data_; }

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

  INLINE(Token::Value peek()) { return scanner_.peek(); }
  INLINE(Token::Value Next()) { return scanner_.Next(); }
  INLINE(void Consume(Token::Value token));
  void Expect(Token::Value token, bool* ok);
  bool Check(Token::Value token);
  void ExpectSemicolon(bool* ok);

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


} }  // namespace v8::internal

#endif  // V8_PARSER_H_
