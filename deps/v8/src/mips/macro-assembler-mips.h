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

#ifndef V8_MIPS_MACRO_ASSEMBLER_MIPS_H_
#define V8_MIPS_MACRO_ASSEMBLER_MIPS_H_

#include "assembler.h"
#include "mips/assembler-mips.h"

namespace v8 {
namespace internal {

// Forward declaration.
class JumpTarget;

// Register at is used for instruction generation. So it is not safe to use it
// unless we know exactly what we do.

// Registers aliases
const Register cp = s7;     // JavaScript context pointer
const Register fp = s8_fp;  // Alias fp

enum InvokeJSFlags {
  CALL_JS,
  JUMP_JS
};

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(void* buffer, int size);

  // Jump, Call, and Ret pseudo instructions implementing inter-working.
  void Jump(const Operand& target,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Call(const Operand& target,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Jump(Register target,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Jump(byte* target, RelocInfo::Mode rmode,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Jump(Handle<Code> code, RelocInfo::Mode rmode,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Call(Register target,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Call(byte* target, RelocInfo::Mode rmode,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Call(Handle<Code> code, RelocInfo::Mode rmode,
            Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Ret(Condition cond = cc_always,
           Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Branch(Condition cond, int16_t offset, Register rs = zero_reg,
              const Operand& rt = Operand(zero_reg), Register scratch = at);
  void Branch(Condition cond, Label* L, Register rs = zero_reg,
              const Operand& rt = Operand(zero_reg), Register scratch = at);
  // conditionnal branch and link
  void BranchAndLink(Condition cond, int16_t offset, Register rs = zero_reg,
                     const Operand& rt = Operand(zero_reg),
                     Register scratch = at);
  void BranchAndLink(Condition cond, Label* L, Register rs = zero_reg,
                     const Operand& rt = Operand(zero_reg),
                     Register scratch = at);

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count, Condition cond = cc_always);

  void Call(Label* target);

  // Jump unconditionally to given label.
  // We NEED a nop in the branch delay slot, as it used by v8, for example in
  // CodeGenerator::ProcessDeferred().
  // Use rather b(Label) for code generation.
  void jmp(Label* L) {
    Branch(cc_always, L);
    nop();
  }

  // Load an object from the root table.
  void LoadRoot(Register destination,
                Heap::RootListIndex index);
  void LoadRoot(Register destination,
                Heap::RootListIndex index,
                Condition cond, Register src1, const Operand& src2);

  // Sets the remembered set bit for [address+offset], where address is the
  // address of the heap object 'object'.  The address must be in the first 8K
  // of an allocated page. The 'scratch' register is used in the
  // implementation and all 3 registers are clobbered by the operation, as
  // well as the ip register.
  void RecordWrite(Register object, Register offset, Register scratch);


  // ---------------------------------------------------------------------------
  // Instruction macros

#define DEFINE_INSTRUCTION(instr)                                       \
  void instr(Register rd, Register rs, const Operand& rt);                     \
  void instr(Register rd, Register rs, Register rt) {                          \
    instr(rd, rs, Operand(rt));                                                \
  }                                                                            \
  void instr(Register rs, Register rt, int32_t j) {                            \
    instr(rs, rt, Operand(j));                                                 \
  }

#define DEFINE_INSTRUCTION2(instr)                                      \
  void instr(Register rs, const Operand& rt);                                  \
  void instr(Register rs, Register rt) {                                       \
    instr(rs, Operand(rt));                                                    \
  }                                                                            \
  void instr(Register rs, int32_t j) {                                         \
    instr(rs, Operand(j));                                                     \
  }

  DEFINE_INSTRUCTION(Add);
  DEFINE_INSTRUCTION(Addu);
  DEFINE_INSTRUCTION(Mul);
  DEFINE_INSTRUCTION2(Mult);
  DEFINE_INSTRUCTION2(Multu);
  DEFINE_INSTRUCTION2(Div);
  DEFINE_INSTRUCTION2(Divu);

  DEFINE_INSTRUCTION(And);
  DEFINE_INSTRUCTION(Or);
  DEFINE_INSTRUCTION(Xor);
  DEFINE_INSTRUCTION(Nor);

  DEFINE_INSTRUCTION(Slt);
  DEFINE_INSTRUCTION(Sltu);

#undef DEFINE_INSTRUCTION
#undef DEFINE_INSTRUCTION2


  //------------Pseudo-instructions-------------

  void mov(Register rd, Register rt) { or_(rd, rt, zero_reg); }
  // Move the logical ones complement of source to dest.
  void movn(Register rd, Register rt);


  // load int32 in the rd register
  void li(Register rd, Operand j, bool gen2instr = false);
  inline void li(Register rd, int32_t j, bool gen2instr = false) {
    li(rd, Operand(j), gen2instr);
  }

  // Exception-generating instructions and debugging support
  void stop(const char* msg);


  // Push multiple registers on the stack.
  // With MultiPush, lower registers are pushed first on the stack.
  // For example if you push t0, t1, s0, and ra you get:
  // |                       |
  // |-----------------------|
  // |         t0            |                     +
  // |-----------------------|                    |
  // |         t1            |                    |
  // |-----------------------|                    |
  // |         s0            |                    v
  // |-----------------------|                     -
  // |         ra            |
  // |-----------------------|
  // |                       |
  void MultiPush(RegList regs);
  void MultiPushReversed(RegList regs);
  void Push(Register src) {
    Addu(sp, sp, Operand(-kPointerSize));
    sw(src, MemOperand(sp, 0));
  }
  inline void push(Register src) { Push(src); }

  void Push(Register src, Condition cond, Register tst1, Register tst2) {
    // Since we don't have conditionnal execution we use a Branch.
    Branch(cond, 3, tst1, Operand(tst2));
    nop();
    Addu(sp, sp, Operand(-kPointerSize));
    sw(src, MemOperand(sp, 0));
  }

  // Pops multiple values from the stack and load them in the
  // registers specified in regs. Pop order is the opposite as in MultiPush.
  void MultiPop(RegList regs);
  void MultiPopReversed(RegList regs);
  void Pop(Register dst) {
    lw(dst, MemOperand(sp, 0));
    Addu(sp, sp, Operand(kPointerSize));
  }
  void Pop() {
    Add(sp, sp, Operand(kPointerSize));
  }


  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link into try handler chain.
  // The return address must be passed in register lr.
  // On exit, r0 contains TOS (code slot).
  void PushTryHandler(CodeLocation try_location, HandlerType type);

  // Unlink the stack handler on top of the stack from the try handler chain.
  // Must preserve the result register.
  void PopTryHandler();


  // ---------------------------------------------------------------------------
  // Support functions.

  inline void BranchOnSmi(Register value, Label* smi_label,
                          Register scratch = at) {
    ASSERT_EQ(0, kSmiTag);
    andi(scratch, value, kSmiTagMask);
    Branch(eq, smi_label, scratch, Operand(zero_reg));
  }


  inline void BranchOnNotSmi(Register value, Label* not_smi_label,
                             Register scratch = at) {
    ASSERT_EQ(0, kSmiTag);
    andi(scratch, value, kSmiTagMask);
    Branch(ne, not_smi_label, scratch, Operand(zero_reg));
  }


  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub, Condition cond = cc_always,
                Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void CallJSExitStub(CodeStub* stub);

  // Return from a code stub after popping its arguments.
  void StubReturn(int argc);

  // Call a runtime routine.
  // Eventually this should be used for all C calls.
  void CallRuntime(Runtime::Function* f, int num_arguments);

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid, int num_arguments);

  // Tail call of a runtime routine (jump).
  // Like JumpToRuntime, but also takes care of passing the number
  // of parameters.
  void TailCallRuntime(const ExternalReference& ext,
                       int num_arguments,
                       int result_size);

  // Jump to the builtin routine.
  void JumpToRuntime(const ExternalReference& builtin);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id, InvokeJSFlags flags);

  // Store the code object for the given builtin in the target register and
  // setup the function in r1.
  void GetBuiltinEntry(Register target, Builtins::JavaScript id);

  struct Unresolved {
    int pc;
    uint32_t flags;  // see Bootstrapper::FixupFlags decoders/encoders.
    const char* name;
  };
  List<Unresolved>* unresolved() { return &unresolved_; }

  Handle<Object> CodeObject() { return code_object_; }


  // ---------------------------------------------------------------------------
  // Stack limit support

  void StackLimitCheck(Label* on_stack_limit_hit);


  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value,
                  Register scratch1, Register scratch2);
  void IncrementCounter(StatsCounter* counter, int value,
                        Register scratch1, Register scratch2);
  void DecrementCounter(StatsCounter* counter, int value,
                        Register scratch1, Register scratch2);


  // ---------------------------------------------------------------------------
  // Debugging

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, const char* msg, Register rs, Operand rt);

  // Like Assert(), but always enabled.
  void Check(Condition cc, const char* msg, Register rs, Operand rt);

  // Print a message to stdout and abort execution.
  void Abort(const char* msg);

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_allow_stub_calls(bool value) { allow_stub_calls_ = value; }
  bool allow_stub_calls() { return allow_stub_calls_; }

 private:
  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));
  void Call(intptr_t target, RelocInfo::Mode rmode, Condition cond = cc_always,
            Register r1 = zero_reg, const Operand& r2 = Operand(zero_reg));

  // Get the code for the given builtin. Returns if able to resolve
  // the function in the 'resolved' flag.
  Handle<Code> ResolveBuiltin(Builtins::JavaScript id, bool* resolved);

  List<Unresolved> unresolved_;
  bool generating_stub_;
  bool allow_stub_calls_;
  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;
};


// -----------------------------------------------------------------------------
// Static helper functions.

// Generate a MemOperand for loading a field from an object.
static inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}



#ifdef GENERATED_CODE_COVERAGE
#define CODE_COVERAGE_STRINGIFY(x) #x
#define CODE_COVERAGE_TOSTRING(x) CODE_COVERAGE_STRINGIFY(x)
#define __FILE_LINE__ __FILE__ ":" CODE_COVERAGE_TOSTRING(__LINE__)
#define ACCESS_MASM(masm) masm->stop(__FILE_LINE__); masm->
#else
#define ACCESS_MASM(masm) masm->
#endif

} }  // namespace v8::internal

#endif  // V8_MIPS_MACRO_ASSEMBLER_MIPS_H_

