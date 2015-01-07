// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PRETTYPRINTER_H_
#define V8_PRETTYPRINTER_H_

#include "src/allocation.h"
#include "src/ast.h"

namespace v8 {
namespace internal {

#ifdef DEBUG

class PrettyPrinter: public AstVisitor {
 public:
  explicit PrettyPrinter(Zone* zone);
  virtual ~PrettyPrinter();

  // The following routines print a node into a string.
  // The result string is alive as long as the PrettyPrinter is alive.
  const char* Print(AstNode* node);
  const char* PrintExpression(FunctionLiteral* program);
  const char* PrintProgram(FunctionLiteral* program);

  void Print(const char* format, ...);

  // Print a node to stdout.
  static void PrintOut(Zone* zone, AstNode* node);

  // Individual nodes
#define DECLARE_VISIT(type) void Visit##type(type* node) OVERRIDE;
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
  void PrintLabels(ZoneList<const AstRawString*>* labels);
  virtual void PrintArguments(ZoneList<Expression*>* arguments);
  void PrintLiteral(Handle<Object> value, bool quote);
  void PrintLiteral(const AstRawString* value, bool quote);
  void PrintParameters(Scope* scope);
  void PrintDeclarations(ZoneList<Declaration*>* declarations);
  void PrintFunctionLiteral(FunctionLiteral* function);
  void PrintCaseClause(CaseClause* clause);
  void PrintObjectLiteralProperty(ObjectLiteralProperty* property);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
};


// Prints the AST structure
class AstPrinter: public PrettyPrinter {
 public:
  explicit AstPrinter(Zone* zone);
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
  void PrintLabelsIndented(ZoneList<const AstRawString*>* labels);

  void inc_indent() { indent_++; }
  void dec_indent() { indent_--; }

  int indent_;
};

#endif  // DEBUG

} }  // namespace v8::internal

#endif  // V8_PRETTYPRINTER_H_
