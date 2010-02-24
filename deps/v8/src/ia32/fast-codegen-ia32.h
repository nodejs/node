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

#ifndef V8_FAST_CODEGEN_IA32_H_
#define V8_FAST_CODEGEN_IA32_H_

#include "v8.h"

#include "ast.h"
#include "compiler.h"
#include "list.h"

namespace v8 {
namespace internal {

class FastCodeGenSyntaxChecker: public AstVisitor {
 public:
  explicit FastCodeGenSyntaxChecker()
      : info_(NULL), has_supported_syntax_(true) {
  }

  void Check(CompilationInfo* info);

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
  explicit FastCodeGenerator(MacroAssembler* masm)
      : masm_(masm), info_(NULL), destination_(no_reg), smi_bits_(0) {
  }

  static Handle<Code> MakeCode(CompilationInfo* info);

  void Generate(CompilationInfo* compilation_info);

 private:
  MacroAssembler* masm() { return masm_; }
  CompilationInfo* info() { return info_; }

  Register destination() { return destination_; }
  void set_destination(Register reg) { destination_ = reg; }

  FunctionLiteral* function() { return info_->function(); }
  Scope* scope() { return info_->scope(); }

  // Platform-specific fixed registers, all guaranteed distinct.
  Register accumulator0();
  Register accumulator1();
  Register scratch0();
  Register scratch1();
  Register receiver_reg();
  Register context_reg();

  Register other_accumulator(Register reg) {
    ASSERT(reg.is(accumulator0()) || reg.is(accumulator1()));
    return (reg.is(accumulator0())) ? accumulator1() : accumulator0();
  }

  // Flags are true if the respective register is statically known to hold a
  // smi.  We do not track every register, only the accumulator registers.
  bool is_smi(Register reg) {
    ASSERT(!reg.is(no_reg));
    return (smi_bits_ & reg.bit()) != 0;
  }
  void set_as_smi(Register reg) {
    ASSERT(!reg.is(no_reg));
    smi_bits_ = smi_bits_ | reg.bit();
  }
  void clear_as_smi(Register reg) {
    ASSERT(!reg.is(no_reg));
    smi_bits_ = smi_bits_ & ~reg.bit();
  }

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Emit code to load the receiver from the stack into receiver_reg.
  void EmitLoadReceiver();

  // Emit code to load a global variable directly from a global property
  // cell into the destination register.
  void EmitGlobalVariableLoad(Handle<Object> cell);

  // Emit a store to an own property of this.  The stored value is expected
  // in accumulator0 and the receiver in receiver_reg.  The receiver
  // register is preserved and the result (the stored value) is left in the
  // destination register.
  void EmitThisPropertyStore(Handle<String> name);

  // Emit a load from an own property of this.  The receiver is expected in
  // receiver_reg.  The receiver register is preserved and the result is
  // left in the destination register.
  void EmitThisPropertyLoad(Handle<String> name);

  // Emit a bitwise or operation.  The left operand is in accumulator1 and
  // the right is in accumulator0.  The result should be left in the
  // destination register.
  void EmitBitOr();

  MacroAssembler* masm_;
  CompilationInfo* info_;

  Register destination_;
  uint32_t smi_bits_;

  DISALLOW_COPY_AND_ASSIGN(FastCodeGenerator);
};


} }  // namespace v8::internal

#endif  // V8_FAST_CODEGEN_IA32_H_
