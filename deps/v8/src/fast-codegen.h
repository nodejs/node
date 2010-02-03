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

#ifndef V8_FAST_CODEGEN_H_
#define V8_FAST_CODEGEN_H_

#include "v8.h"

#include "ast.h"
#include "compiler.h"

namespace v8 {
namespace internal {

class FastCodeGenSyntaxChecker: public AstVisitor {
 public:
  explicit FastCodeGenSyntaxChecker()
      : info_(NULL), has_supported_syntax_(true) {
  }

  void Check(FunctionLiteral* fun, CompilationInfo* info);

  CompilationInfo* info() { return info_; }
  bool has_supported_syntax() { return has_supported_syntax_; }

 private:
  void VisitDeclarations(ZoneList<Declaration*>* decls);
  void VisitStatements(ZoneList<Statement*>* stmts);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  CompilationInfo* info_;
  bool has_supported_syntax_;

  DISALLOW_COPY_AND_ASSIGN(FastCodeGenSyntaxChecker);
};


class FastCodeGenerator: public AstVisitor {
 public:
  FastCodeGenerator(MacroAssembler* masm, Handle<Script> script, bool is_eval)
      : masm_(masm),
        script_(script),
        is_eval_(is_eval),
        function_(NULL),
        info_(NULL) {
  }

  static Handle<Code> MakeCode(FunctionLiteral* fun,
                               Handle<Script> script,
                               bool is_eval,
                               CompilationInfo* info);

  void Generate(FunctionLiteral* fun, CompilationInfo* info);

 private:
  MacroAssembler* masm() { return masm_; }
  FunctionLiteral* function() { return function_; }
  Label* bailout() { return &bailout_; }

  bool has_receiver() { return !info_->receiver().is_null(); }
  Handle<Object> receiver() { return info_->receiver(); }
  bool has_this_properties() { return info_->has_this_properties(); }

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Emit code to load the receiver from the stack into a given register.
  void EmitLoadReceiver(Register reg);

  // Emit code to check that the receiver has the same map as the
  // compile-time receiver.  Receiver is expected in {ia32-edx, x64-rdx,
  // arm-r1}.  Emit a branch to the (single) bailout label if check fails.
  void EmitReceiverMapCheck();

  // Emit code to load a global variable value into {is32-eax, x64-rax,
  // arm-r0}.  Register {ia32-edx, x64-rdx, arm-r1} is preserved if it is
  // holding the receiver and {is32-ecx, x64-rcx, arm-r2} is always
  // clobbered.
  void EmitGlobalVariableLoad(Handle<String> name);

  // Emit a store to an own property of this.  The stored value is expected
  // in {ia32-eax, x64-rax, arm-r0} and the receiver in {is32-edx, x64-rdx,
  // arm-r1}.  Both are preserve.
  void EmitThisPropertyStore(Handle<String> name);

  MacroAssembler* masm_;
  Handle<Script> script_;
  bool is_eval_;

  FunctionLiteral* function_;
  CompilationInfo* info_;

  Label bailout_;

  DISALLOW_COPY_AND_ASSIGN(FastCodeGenerator);
};


} }  // namespace v8::internal

#endif  // V8_FAST_CODEGEN_H_
