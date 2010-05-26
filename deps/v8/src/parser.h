// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "scanner.h"
#include "allocation.h"

namespace v8 {
namespace internal {


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
  void set_property_count(int value) { backing_[kPropertyCountOffset] = value; }

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
        last_entry_(0) { }
  virtual ~ScriptDataImpl();
  virtual int Length();
  virtual const char* Data();
  virtual bool HasError();
  FunctionEntry GetFunctionEnd(int start);
  bool SanityCheck();

  Scanner::Location MessageLocation();
  const char* BuildMessage();
  Vector<const char*> BuildArgs();

  bool has_error() { return store_[kHasErrorOffset]; }
  unsigned magic() { return store_[kMagicOffset]; }
  unsigned version() { return store_[kVersionOffset]; }

  static const unsigned kMagicNumber = 0xBadDead;
  static const unsigned kCurrentVersion = 1;

  static const unsigned kMagicOffset = 0;
  static const unsigned kVersionOffset = 1;
  static const unsigned kHasErrorOffset = 2;
  static const unsigned kSizeOffset = 3;
  static const unsigned kHeaderSize = 4;

 private:
  unsigned Read(int position);
  unsigned* ReadAddress(int position);
  int EntryCount();
  FunctionEntry nth(int n);

  Vector<unsigned> store_;

  // The last entry returned.  This is used to make lookup faster:
  // the next entry to return is typically the next entry so lookup
  // will usually be much faster if we start from the last entry.
  int last_entry_;
};


// The parser: Takes a script and and context information, and builds a
// FunctionLiteral AST node. Returns NULL and deallocates any allocated
// AST nodes if parsing failed.
FunctionLiteral* MakeAST(bool compile_in_global_context,
                         Handle<Script> script,
                         v8::Extension* extension,
                         ScriptDataImpl* pre_data,
                         bool is_json = false);


ScriptDataImpl* PreParse(Handle<String> source,
                         unibrow::CharacterStream* stream,
                         v8::Extension* extension);


bool ParseRegExp(FlatStringReader* input,
                 bool multiline,
                 RegExpCompileData* result);


// Support for doing lazy compilation. The script is the script containing full
// source of the script where the function is declared. The start_position and
// end_position specifies the part of the script source which has the source
// for the function declaration in the form:
//
//    (<formal parameters>) { <function body> }
//
// without any function keyword or name.
//
FunctionLiteral* MakeLazyAST(Handle<Script> script,
                             Handle<String> name,
                             int start_position,
                             int end_position,
                             bool is_expression);


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
