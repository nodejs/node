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

#ifndef V8_IA32_MACRO_ASSEMBLER_IA32_H_
#define V8_IA32_MACRO_ASSEMBLER_IA32_H_

#include "assembler.h"

namespace v8 {
namespace internal {

// Convenience for platform-independent signatures.  We do not normally
// distinguish memory operands from other operands on ia32.
typedef Operand MemOperand;

// Forward declaration.
class JumpTarget;

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(void* buffer, int size);

  // ---------------------------------------------------------------------------
  // GC Support


  void RecordWriteHelper(Register object,
                         Register addr,
                         Register scratch);

  // Check if object is in new space.
  // scratch can be object itself, but it will be clobbered.
  void InNewSpace(Register object,
                  Register scratch,
                  Condition cc,  // equal for new space, not_equal otherwise.
                  Label* branch);

  // Set the remembered set bit for [object+offset].
  // object is the object being stored into, value is the object being stored.
  // If offset is zero, then the scratch register contains the array index into
  // the elements array represented as a Smi.
  // All registers are clobbered by the operation.
  void RecordWrite(Register object,
                   int offset,
                   Register value,
                   Register scratch);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // ---------------------------------------------------------------------------
  // Debugger Support

  void SaveRegistersToMemory(RegList regs);
  void RestoreRegistersFromMemory(RegList regs);
  void PushRegistersFromMemory(RegList regs);
  void PopRegistersToMemory(RegList regs);
  void CopyRegistersFromStackToMemory(Register base,
                                      Register scratch,
                                      RegList regs);
  void DebugBreak();
#endif

  // ---------------------------------------------------------------------------
  // Stack limit support

  // Do simple test for stack overflow. This doesn't handle an overflow.
  void StackLimitCheck(Label* on_stack_limit_hit);

  // ---------------------------------------------------------------------------
  // Activation frames

  void EnterInternalFrame() { EnterFrame(StackFrame::INTERNAL); }
  void LeaveInternalFrame() { LeaveFrame(StackFrame::INTERNAL); }

  void EnterConstructFrame() { EnterFrame(StackFrame::CONSTRUCT); }
  void LeaveConstructFrame() { LeaveFrame(StackFrame::CONSTRUCT); }

  // Enter specific kind of exit frame; either in normal or debug mode.
  // Expects the number of arguments in register eax and
  // sets up the number of arguments in register edi and the pointer
  // to the first argument in register esi.
  void EnterExitFrame(ExitFrame::Mode mode);

  void EnterApiExitFrame(ExitFrame::Mode mode, int stack_space, int argc);

  // Leave the current exit frame. Expects the return value in
  // register eax:edx (untouched) and the pointer to the first
  // argument in register esi.
  void LeaveExitFrame(ExitFrame::Mode mode);

  // Find the function context up the context chain.
  void LoadContext(Register dst, int context_chain_length);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeCode(const Operand& code,
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

  void InvokeFunction(JSFunction* function,
                      const ParameterCount& actual,
                      InvokeFlag flag);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id, InvokeFlag flag);

  // Store the code object for the given builtin in the target register.
  void GetBuiltinEntry(Register target, Builtins::JavaScript id);

  // Expression support
  void Set(Register dst, const Immediate& x);
  void Set(const Operand& dst, const Immediate& x);

  // Compare object type for heap object.
  // Incoming register is heap_object and outgoing register is map.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  void CmpInstanceType(Register map, InstanceType type);

  // Check if the map of an object is equal to a specified map and
  // branch to label if not. Skip the smi check if not required
  // (object is known to be a heap object)
  void CheckMap(Register obj,
                Handle<Map> map,
                Label* fail,
                bool is_heap_object);

  // Check if the object in register heap_object is a string. Afterwards the
  // register map contains the object map and the register instance_type
  // contains the instance_type. The registers map and instance_type can be the
  // same in which case it contains the instance type afterwards. Either of the
  // registers map and instance_type can be the same as heap_object.
  Condition IsObjectStringType(Register heap_object,
                               Register map,
                               Register instance_type);

  // FCmp is similar to integer cmp, but requires unsigned
  // jcc instructions (je, ja, jae, jb, jbe, je, and jz).
  void FCmp();

  // Smi tagging support.
  void SmiTag(Register reg) {
    ASSERT(kSmiTag == 0);
    ASSERT(kSmiTagSize == 1);
    add(reg, Operand(reg));
  }
  void SmiUntag(Register reg) {
    sar(reg, kSmiTagSize);
  }

  // Abort execution if argument is not a number. Used in debug code.
  void AbortIfNotNumber(Register object);

  // Abort execution if argument is not a smi. Used in debug code.
  void AbortIfNotSmi(Register object);

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link into try handler chain.  The return
  // address must be pushed before calling this helper.
  void PushTryHandler(CodeLocation try_location, HandlerType type);

  // Unlink the stack handler on top of the stack from the try handler chain.
  void PopTryHandler();

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
  // The function can optionally (when save_at_depth !=
  // kInvalidProtoDepth) save the object at the given depth by moving
  // it to [esp + kPointerSize].
  Register CheckMaps(JSObject* object, Register object_reg,
                     JSObject* holder, Register holder_reg,
                     Register scratch,
                     int save_at_depth,
                     Label* miss);

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, but the scratch register is clobbered.
  void CheckAccessGlobalProxy(Register holder_reg,
                              Register scratch,
                              Label* miss);


  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space. If the new space is exhausted control
  // continues at the gc_required label. The allocated object is returned in
  // result and end of the new object is returned in result_end. The register
  // scratch can be passed as no_reg in which case an additional object
  // reference will be added to the reloc info. The returned pointers in result
  // and result_end have not yet been tagged as heap objects. If
  // result_contains_top_on_entry is true the content of result is known to be
  // the allocation top on entry (could be result_end from a previous call to
  // AllocateInNewSpace). If result_contains_top_on_entry is true scratch
  // should be no_reg as it is never used.
  void AllocateInNewSpace(int object_size,
                          Register result,
                          Register result_end,
                          Register scratch,
                          Label* gc_required,
                          AllocationFlags flags);

  void AllocateInNewSpace(int header_size,
                          ScaleFactor element_size,
                          Register element_count,
                          Register result,
                          Register result_end,
                          Register scratch,
                          Label* gc_required,
                          AllocationFlags flags);

  void AllocateInNewSpace(Register object_size,
                          Register result,
                          Register result_end,
                          Register scratch,
                          Label* gc_required,
                          AllocationFlags flags);

  // Undo allocation in new space. The object passed and objects allocated after
  // it will no longer be allocated. Make sure that no pointers are left to the
  // object(s) no longer allocated as they would be invalid when allocation is
  // un-done.
  void UndoAllocationInNewSpace(Register object);

  // Allocate a heap number in new space with undefined value. The
  // register scratch2 can be passed as no_reg; the others must be
  // valid registers. Returns tagged pointer in result register, or
  // jumps to gc_required if new space is full.
  void AllocateHeapNumber(Register result,
                          Register scratch1,
                          Register scratch2,
                          Label* gc_required);

  // Allocate a sequential string. All the header fields of the string object
  // are initialized.
  void AllocateTwoByteString(Register result,
                             Register length,
                             Register scratch1,
                             Register scratch2,
                             Register scratch3,
                             Label* gc_required);
  void AllocateAsciiString(Register result,
                           Register length,
                           Register scratch1,
                           Register scratch2,
                           Register scratch3,
                           Label* gc_required);

  // Allocate a raw cons string object. Only the map field of the result is
  // initialized.
  void AllocateConsString(Register result,
                          Register scratch1,
                          Register scratch2,
                          Label* gc_required);
  void AllocateAsciiConsString(Register result,
                               Register scratch1,
                               Register scratch2,
                               Label* gc_required);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Check if result is zero and op is negative.
  void NegativeZeroTest(Register result, Register op, Label* then_label);

  // Check if result is zero and op is negative in code using jump targets.
  void NegativeZeroTest(CodeGenerator* cgen,
                        Register result,
                        Register op,
                        JumpTarget* then_target);

  // Check if result is zero and any of op1 and op2 are negative.
  // Register scratch is destroyed, and it must be different from op2.
  void NegativeZeroTest(Register result, Register op1, Register op2,
                        Register scratch, Label* then_label);

  // Try to get function prototype of a function and puts the value in
  // the result register. Checks that the function really is a
  // function and jumps to the miss label if the fast checks fail. The
  // function register will be untouched; the other registers may be
  // clobbered.
  void TryGetFunctionPrototype(Register function,
                               Register result,
                               Register scratch,
                               Label* miss);

  // Generates code for reporting that an illegal operation has
  // occurred.
  void IllegalOperation(int num_arguments);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.  Generate the code if necessary.
  void CallStub(CodeStub* stub);

  // Call a code stub and return the code object called.  Try to generate
  // the code if necessary.  Do not perform a GC but instead return a retry
  // after GC failure.
  Object* TryCallStub(CodeStub* stub);

  // Tail call a code stub (jump).  Generate the code if necessary.
  void TailCallStub(CodeStub* stub);

  // Tail call a code stub (jump) and return the code object called.  Try to
  // generate the code if necessary.  Do not perform a GC but instead return
  // a retry after GC failure.
  Object* TryTailCallStub(CodeStub* stub);

  // Return from a code stub after popping its arguments.
  void StubReturn(int argc);

  // Call a runtime routine.
  void CallRuntime(Runtime::Function* f, int num_arguments);

  // Call a runtime function, returning the CodeStub object called.
  // Try to generate the stub code if necessary.  Do not perform a GC
  // but instead return a retry after GC failure.
  Object* TryCallRuntime(Runtime::Function* f, int num_arguments);

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId id, int num_arguments);

  // Convenience function: call an external reference.
  void CallExternalReference(ExternalReference ref, int num_arguments);

  // Convenience function: Same as above, but takes the fid instead.
  Object* TryCallRuntime(Runtime::FunctionId id, int num_arguments);

  // Tail call of a runtime routine (jump).
  // Like JumpToExternalReference, but also takes care of passing the number
  // of parameters.
  void TailCallExternalReference(const ExternalReference& ext,
                                 int num_arguments,
                                 int result_size);

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid,
                       int num_arguments,
                       int result_size);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in esp[0], esp[4],
  // etc., not pushed. The argument count assumes all arguments are word sized.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  void PrepareCallCFunction(int num_arguments, Register scratch);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);

  void PushHandleScope(Register scratch);

  // Pops a handle scope using the specified scratch register and
  // ensuring that saved register, it is not no_reg, is left unchanged.
  void PopHandleScope(Register saved, Register scratch);

  // As PopHandleScope, but does not perform a GC.  Instead, returns a
  // retry after GC failure object if GC is necessary.
  Object* TryPopHandleScope(Register saved, Register scratch);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& ext);


  // ---------------------------------------------------------------------------
  // Utilities

  void Ret();

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the esp register.
  void Drop(int element_count);

  void Call(Label* target) { call(target); }

  void Move(Register target, Handle<Object> value);

  Handle<Object> CodeObject() { return code_object_; }


  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int value);
  void DecrementCounter(StatsCounter* counter, int value);
  void IncrementCounter(Condition cc, StatsCounter* counter, int value);
  void DecrementCounter(Condition cc, StatsCounter* counter, int value);


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

  // ---------------------------------------------------------------------------
  // String utilities.

  // Check whether the instance type represents a flat ascii string. Jump to the
  // label if not. If the instance type can be scratched specify same register
  // for both instance type and scratch.
  void JumpIfInstanceTypeIsNotSequentialAscii(Register instance_type,
                                              Register scratch,
                                              Label* on_not_flat_ascii_string);

  // Checks if both objects are sequential ASCII strings, and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialAsciiStrings(Register object1,
                                           Register object2,
                                           Register scratch1,
                                           Register scratch2,
                                           Label* on_not_flat_ascii_strings);

 private:
  bool generating_stub_;
  bool allow_stub_calls_;
  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Handle<Code> code_constant,
                      const Operand& code_operand,
                      Label* done,
                      InvokeFlag flag);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void LeaveFrame(StackFrame::Type type);

  void EnterExitFramePrologue(ExitFrame::Mode mode);
  void EnterExitFrameEpilogue(ExitFrame::Mode mode, int argc);

  // Allocation support helpers.
  void LoadAllocationTopHelper(Register result,
                               Register result_end,
                               Register scratch,
                               AllocationFlags flags);
  void UpdateAllocationTopHelper(Register result_end, Register scratch);

  // Helper for PopHandleScope.  Allowed to perform a GC and returns
  // NULL if gc_allowed.  Does not perform a GC if !gc_allowed, and
  // possibly returns a failure object indicating an allocation failure.
  Object* PopHandleScopeHelper(Register saved,
                               Register scratch,
                               bool gc_allowed);
};


// The code patcher is used to patch (typically) small parts of code e.g. for
// debugging and other types of instrumentation. When using the code patcher
// the exact number of bytes specified must be emitted. Is not legal to emit
// relocation information. If any of these constraints are violated it causes
// an assertion.
class CodePatcher {
 public:
  CodePatcher(byte* address, int size);
  virtual ~CodePatcher();

  // Macro assembler to emit code.
  MacroAssembler* masm() { return &masm_; }

 private:
  byte* address_;  // The address of the code being patched.
  int size_;  // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
};


// -----------------------------------------------------------------------------
// Static helper functions.

// Generate an Operand for loading a field from an object.
static inline Operand FieldOperand(Register object, int offset) {
  return Operand(object, offset - kHeapObjectTag);
}


// Generate an Operand for loading an indexed field from an object.
static inline Operand FieldOperand(Register object,
                                   Register index,
                                   ScaleFactor scale,
                                   int offset) {
  return Operand(object, index, scale, offset - kHeapObjectTag);
}


#ifdef GENERATED_CODE_COVERAGE
extern void LogGeneratedCodeCoverage(const char* file_line);
#define CODE_COVERAGE_STRINGIFY(x) #x
#define CODE_COVERAGE_TOSTRING(x) CODE_COVERAGE_STRINGIFY(x)
#define __FILE_LINE__ __FILE__ ":" CODE_COVERAGE_TOSTRING(__LINE__)
#define ACCESS_MASM(masm) {                                               \
    byte* ia32_coverage_function =                                        \
        reinterpret_cast<byte*>(FUNCTION_ADDR(LogGeneratedCodeCoverage)); \
    masm->pushfd();                                                       \
    masm->pushad();                                                       \
    masm->push(Immediate(reinterpret_cast<int>(&__FILE_LINE__)));         \
    masm->call(ia32_coverage_function, RelocInfo::RUNTIME_ENTRY);         \
    masm->pop(eax);                                                       \
    masm->popad();                                                        \
    masm->popfd();                                                        \
  }                                                                       \
  masm->
#else
#define ACCESS_MASM(masm) masm->
#endif


} }  // namespace v8::internal

#endif  // V8_IA32_MACRO_ASSEMBLER_IA32_H_
