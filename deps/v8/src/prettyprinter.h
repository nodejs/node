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

#ifndef V8_PRETTYPRINTER_H_
#define V8_PRETTYPRINTER_H_

#include "allocation.h"
#include "ast.h"

namespace v8 {
namespace internal {

#ifdef DEBUG

class PrettyPrinter: public AstVisitor {
 public:
  PrettyPrinter();
  virtual ~PrettyPrinter();

  // The following routines print a node into a string.
  // The result string is alive as long as the PrettyPrinter is alive.
  const char* Print(AstNode* node);
  const char* PrintExpression(FunctionLiteral* program);
  const char* PrintProgram(FunctionLiteral* program);

  void Print(const char* format, ...);

  // Print a node to stdout.
  static void PrintOut(AstNode* node);

  // Individual nodes
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  char* output_;  // output string buffer
  int size_;  // output_ size
  int pos_;  // current printing position

 protected:
  void Init();
  const char* Output() const { return output_; }

  virtual void PrintStatements(ZoneList<Statement*>* statements);
  void PrintLabels(ZoneStringList* labels);
  virtual void PrintArguments(ZoneList<Expression*>* arguments);
  void PrintLiteral(Handle<Object> value, bool quote);
  void PrintParameters(Scope* scope);
  void PrintDeclarations(ZoneList<Declaration*>* declarations);
  void PrintFunctionLiteral(FunctionLiteral* function);
  void PrintCaseClause(CaseClause* clause);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
};


// Prints the AST structure
class AstPrinter: public PrettyPrinter {
 public:
  AstPrinter();
  virtual ~AstPrinter();

  const char* PrintProgram(FunctionLiteral* program);

  // Individual nodes
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  friend class IndentedScope;
  void PrintIndented(const char* txt);
  void PrintIndentedVisit(const char* s, AstNode* node);

  void PrintStatements(ZoneList<Statement*>* statements);
  void PrintDeclarations(ZoneList<Declaration*>* declarations);
  void PrintParameters(Scope* scope);
  void PrintArguments(ZoneList<Expression*>* arguments);
  void PrintCaseClause(CaseClause* clause);
  void PrintLiteralIndented(const char* info, Handle<Object> value, bool quote);
  void PrintLiteralWithModeIndented(const char* info,
                                    Variable* var,
                                    Handle<Object> value);
  void PrintLabelsIndented(const char* info, ZoneStringList* labels);

  void inc_indent() { indent_++; }
  void dec_indent() { indent_--; }

  int indent_;
};

#endif  // DEBUG

} }  // namespace v8::internal

#endif  // V8_PRETTYPRINTER_H_
