// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_ARM_MACRO_ASSEMBLER_ARM_H_
#define V8_ARM_MACRO_ASSEMBLER_ARM_H_

#include "assembler.h"

namespace v8 {
namespace internal {


// Give alias names to registers
const Register cp = { 8 };  // JavaScript context pointer


enum InvokeJSFlags {
  CALL_JS,
  JUMP_JS
};


// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(void* buffer, int size);

  // ---------------------------------------------------------------------------
  // Low-level helpers for compiler

  // Jump, Call, and Ret pseudo instructions implementing inter-working
 private:
  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al);
  void Call(intptr_t target, RelocInfo::Mode rmode, Condition cond = al);
 public:
  void Jump(Register target, Condition cond = al);
  void Jump(byte* target, RelocInfo::Mode rmode, Condition cond = al);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Register target, Condition cond = al);
  void Call(byte* target, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  void Ret(Condition cond = al);
  // Jumps to the label at the index given by the Smi in "index".
  void SmiJumpTable(Register index, Vector<Label*> targets);
  // Load an object from the root table.
  void LoadRoot(Register destination,
                Heap::RootListIndex index,
                Condition cond = al);

  // Sets the remembered set bit for [address+offset], where address is the
  // address of the heap object 'object'.  The address must be in the first 8K
  // of an allocated page. The 'scratch' register is used in the
  // implementation and all 3 registers are clobbered by the operation, as
  // well as the ip register.
  void RecordWrite(Register object, Register offset, Register scratch);

  // ---------------------------------------------------------------------------
  // Activation frames

  void EnterInternalFrame() { EnterFrame(StackFrame::INTERNAL); }
  void LeaveInternalFrame() { LeaveFrame(StackFrame::INTERNAL); }

  void EnterConstructFrame() { EnterFrame(StackFrame::CONSTRUCT); }
  void LeaveConstructFrame() { LeaveFrame(StackFrame::CONSTRUCT); }

  // Enter specific kind of exit frame; either EXIT or
  // EXIT_DEBUG. Expects the number of arguments in register r0 and
  // the builtin function to call in register r1. Exits with argc in
  // r4, argv in r6, and and the builtin function to call in r5.
  void EnterExitFrame(StackFrame::Type type);

  // Leave the current exit frame. Expects the return value in r0.
  void LeaveExitFrame(StackFrame::Type type);

  // Align the stack by optionally pushing a Smi zero.
  void AlignStack(int offset);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeCode(Register code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  InvokeFlag flag);

  void InvokeCode(Handle<Code> code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  RelocInfo::Mode rmode,
                  InvokeFlag flag);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function,
                      const ParameterCount& actual,
                      InvokeFlag flag);


#ifdef ENABLE_DEBUGGER_SUPPORT
  // ---------------------------------------------------------------------------
  // Debugger Support

  void SaveRegistersToMemory(RegList regs);
  void RestoreRegistersFromMemory(RegList regs);
  void CopyRegistersFromMemoryToStack(Register base, RegList regs);
  void CopyRegistersFromStackToMemory(Register base,
                                      Register scratch,
                                      RegList regs);
#endif

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link into try handler chain.
  // The return address must be passed in register lr.
  // On exit, r0 contains TOS (code slot).
  void PushTryHandler(CodeLocation try_location, HandlerType type);


  // ---------------------------------------------------------------------------
  // Inline caching support

  // Generates code that verifies that the maps of objects in the
  // prototype chain of object hasn't changed since the code was
  // generated and branches to the miss label if any map has. If
  // necessary the function also generates code for security check
  // in case of global object holders. The scratch and holder
  // registers are always clobbered, but the object register is only
  // clobbered if it the same as the holder register. The function
  // returns a register containing the holder - either object_reg or
  // holder_reg.
  Register CheckMaps(JSObject* object, Register object_reg,
                     JSObject* holder, Register holder_reg,
                     Register scratch, Label* miss);

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, whereas both scratch registers are clobbered.
  void CheckAccessGlobalProxy(Register holder_reg,
                              Register scratch,
                              Label* miss);


  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space. The object_size is specified in words (not
  // bytes). If the new space is exhausted control continues at the gc_required
  // label. The allocated object is returned in result. If the flag
  // tag_allocated_object is true the result is tagged as as a heap object.
  void AllocateInNewSpace(int object_size,
                          Register result,
                          Register scratch1,
                          Register scratch2,
                          Label* gc_required,
                          AllocationFlags flags);
  void AllocateInNewSpace(Register object_size,
                          Register result,
                          Register scratch1,
                          Register scratch2,
                          Label* gc_required,
                          AllocationFlags flags);

  // Undo allocation in new space. The object passed and objects allocated after
  // it will no longer be allocated. The caller must make sure that no pointers
  // are left to the object(s) no longer allocated as they would be invalid when
  // allocation is undone.
  void UndoAllocationInNewSpace(Register object, Register scratch);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Try to get function prototype of a function and puts the value in
  // the result register. Checks that the function really is a
  // function and jumps to the miss label if the fast checks fail. The
  // function register will be untouched; the other registers may be
  // clobbered.
  void TryGetFunctionPrototype(Register function,
                               Register result,
                               Register scratch,
                               Label* miss);

  // Compare object type for heap object.  heap_object contains a non-Smi
  // whose object type should be compared with the given type.  This both
  // sets the flags and leaves the object type in the type_reg register.
  // It leaves the map in the map register (unless the type_reg and map register
  // are the same register).  It leaves the heap object in the heap_object
  // register unless the heap_object register is the same register as one of the
  // other registers.
  void CompareObjectType(Register heap_object,
                         Register map,
                         Register type_reg,
                         InstanceType type);

  // Compare instance type in a map.  map contains a valid map object whose
  // object type should be compared with the given type.  This both
  // sets the flags and leaves the object type in the type_reg register.  It
  // leaves the heap object in the heap_object register unless the heap_object
  // register is the same register as type_reg.
  void CompareInstanceType(Register map,
                           Register type_reg,
                           InstanceType type);

  inline void BranchOnSmi(Register value, Label* smi_label) {
    tst(value, Operand(kSmiTagMask));
    b(eq, smi_label);
  }

  inline void BranchOnNotSmi(Register value, Label* not_smi_label) {
    tst(value, Operand(kSmiTagMask));
    b(ne, not_smi_label);
  }

  // Generates code for reporting that an illegal operation has
  // occurred.
  void IllegalOperation(int num_arguments);


  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub, Condition cond = al);

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

  // Jump to a runtime routine.
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
  void Assert(Condition cc, const char* msg);

  // Like Assert(), but always enabled.
  void Check(Condition cc, const char* msg);

  // Print a message to stdout and abort execution.
  void Abort(const char* msg);

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_allow_stub_calls(bool value) { allow_stub_calls_ = value; }
  bool allow_stub_calls() { return allow_stub_calls_; }

 private:
  List<Unresolved> unresolved_;
  bool generating_stub_;
  bool allow_stub_calls_;
  Handle<Object> code_object_;  // This handle will be patched with the code
                                // object on installation.

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Handle<Code> code_constant,
                      Register code_reg,
                      Label* done,
                      InvokeFlag flag);

  // Prepares for a call or jump to a builtin by doing two things:
  // 1. Emits code that fetches the builtin's function object from the context
  //    at runtime, and puts it in the register rdi.
  // 2. Fetches the builtin's code object, and returns it in a handle, at
  //    compile time, so that later code can emit instructions to jump or call
  //    the builtin directly.  If the code object has not yet been created, it
  //    returns the builtin code object for IllegalFunction, and sets the
  //    output parameter "resolved" to false.  Code that uses the return value
  //    should then add the address and the builtin name to the list of fixups
  //    called unresolved_, which is fixed up by the bootstrapper.
  Handle<Code> ResolveBuiltin(Builtins::JavaScript id, bool* resolved);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void LeaveFrame(StackFrame::Type type);
};


#ifdef ENABLE_DEBUGGER_SUPPORT
// The code patcher is used to patch (typically) small parts of code e.g. for
// debugging and other types of instrumentation. When using the code patcher
// the exact number of bytes specified must be emitted. It is not legal to emit
// relocation information. If any of these constraints are violated it causes
// an assertion to fail.
class CodePatcher {
 public:
  CodePatcher(byte* address, int instructions);
  virtual ~CodePatcher();

  // Macro assembler to emit code.
  MacroAssembler* masm() { return &masm_; }

  // Emit an instruction directly.
  void Emit(Instr x);

  // Emit an address directly.
  void Emit(Address addr);

 private:
  byte* address_;  // The address of the code being patched.
  int instructions_;  // Number of instructions of the expected patch size.
  int size_;  // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
};
#endif  // ENABLE_DEBUGGER_SUPPORT


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

#endif  // V8_ARM_MACRO_ASSEMBLER_ARM_H_
