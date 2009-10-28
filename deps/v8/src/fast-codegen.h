// Copyright 2009 the V8 project authors. All rights reserved.
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

#ifndef V8_FAST_CODEGEN_H_
#define V8_FAST_CODEGEN_H_

#include "v8.h"

#include "ast.h"

namespace v8 {
namespace internal {


class FastCodeGenerator: public AstVisitor {
 public:
  FastCodeGenerator(MacroAssembler* masm, Handle<Script> script, bool is_eval)
    : masm_(masm), function_(NULL), script_(script), is_eval_(is_eval) {
  }

  static Handle<Code> MakeCode(FunctionLiteral* fun,
                               Handle<Script> script,
                               bool is_eval);

  void Generate(FunctionLiteral* fun);

 private:
  int SlotOffset(Slot* slot);

  void Move(Location destination, Location source);

  void Move(Location destination, Register source);
  void Move(Location destination, Slot* source);
  void Move(Location destination, Literal* source);

  void Move(Register destination, Location source);
  void Move(Slot* destination, Location source);

  // Drop the TOS, and store source to destination.
  // If destination is TOS, just overwrite TOS with source.
  void DropAndMove(Location destination, Register source);

  void VisitDeclarations(ZoneList<Declaration*>* declarations);
  Handle<JSFunction> BuildBoilerplate(FunctionLiteral* fun);
  void DeclareGlobals(Handle<FixedArray> pairs);

  void SetFunctionPosition(FunctionLiteral* fun);
  void SetReturnPosition(FunctionLiteral* fun);
  void SetStatementPosition(Statement* stmt);
  void SetSourcePosition(int pos);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  MacroAssembler* masm_;
  FunctionLiteral* function_;
  Handle<Script> script_;
  bool is_eval_;

  DISALLOW_COPY_AND_ASSIGN(FastCodeGenerator);
};


} }  // namespace v8::internal

#endif  // V8_FAST_CODEGEN_H_
