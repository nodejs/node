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

#ifndef V8_ARM_MACRO_ASSEMBLER_ARM_H_
#define V8_ARM_MACRO_ASSEMBLER_ARM_H_

#include "assembler.h"

namespace v8 {
namespace internal {

// Forward declaration.
class PostCallGenerator;

// ----------------------------------------------------------------------------
// Static helper functions

// Generate a MemOperand for loading a field from an object.
static inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}


static inline Operand SmiUntagOperand(Register object) {
  return Operand(object, ASR, kSmiTagSize);
}



// Give alias names to registers
const Register cp = { 8 };  // JavaScript context pointer
const Register roots = { 10 };  // Roots array pointer.

enum InvokeJSFlags {
  CALL_JS,
  JUMP_JS
};


// Flags used for the AllocateInNewSpace functions.
enum AllocationFlags {
  // No special flags.
  NO_ALLOCATION_FLAGS = 0,
  // Return the pointer to the allocated already tagged as a heap object.
  TAG_OBJECT = 1 << 0,
  // The content of the result register already contains the allocation top in
  // new space.
  RESULT_CONTAINS_TOP = 1 << 1,
  // Specify that the requested size of the space to allocate is specified in
  // words instead of bytes.
  SIZE_IN_WORDS = 1 << 2
};


// Flags used for the ObjectToDoubleVFPRegister function.
enum ObjectToDoubleFlags {
  // No special flags.
  NO_OBJECT_TO_DOUBLE_FLAGS = 0,
  // Object is known to be a non smi.
  OBJECT_NOT_SMI = 1 << 0,
  // Don't load NaNs or infinities, branch to the non number case instead.
  AVOID_NANS_AND_INFINITIES = 1 << 1
};


// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(void* buffer, int size);

  // Jump, Call, and Ret pseudo instructions implementing inter-working.
  void Jump(Register target, Condition cond = al);
  void Jump(byte* target, RelocInfo::Mode rmode, Condition cond = al);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Register target, Condition cond = al);
  void Call(byte* target, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  void Ret(Condition cond = al);

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count, Condition cond = al);

  void Ret(int drop, Condition cond = al);

  // Swap two registers.  If the scratch register is omitted then a slightly
  // less efficient form using xor instead of mov is emitted.
  void Swap(Register reg1,
            Register reg2,
            Register scratch = no_reg,
            Condition cond = al);


  void And(Register dst, Register src1, const Operand& src2,
           Condition cond = al);
  void Ubfx(Register dst, Register src, int lsb, int width,
            Condition cond = al);
  void Sbfx(Register dst, Register src, int lsb, int width,
            Condition cond = al);
  // The scratch register is not used for ARMv7.
  // scratch can be the same register as src (in which case it is trashed), but
  // not the same as dst.
  void Bfi(Register dst,
           Register src,
           Register scratch,
           int lsb,
           int width,
           Condition cond = al);
  void Bfc(Register dst, int lsb, int width, Condition cond = al);
  void Usat(Register dst, int satpos, const Operand& src,
            Condition cond = al);

  void Call(Label* target);
  void Move(Register dst, Handle<Object> value);
  // May do nothing if the registers are identical.
  void Move(Register dst, Register src);
  // Jumps to the label at the index given by the Smi in "index".
  void SmiJumpTable(Register index, Vector<Label*> targets);
  // Load an object from the root table.
  void LoadRoot(Register destination,
                Heap::RootListIndex index,
                Condition cond = al);
  // Store an object to the root table.
  void StoreRoot(Register source,
                 Heap::RootListIndex index,
                 Condition cond = al);


  // Check if object is in new space.
  // scratch can be object itself, but it will be clobbered.
  void InNewSpace(Register object,
                  Register scratch,
                  Condition cond,  // eq for new space, ne otherwise
                  Label* branch);


  // For the page containing |object| mark the region covering [address]
  // dirty. The object address must be in the first 8K of an allocated page.
  void RecordWriteHelper(Register object,
                         Register address,
                         Register scratch);

  // For the page containing |object| mark the region covering
  // [object+offset] dirty. The object address must be in the first 8K
  // of an allocated page.  The 'scratch' registers are used in the
  // implementation and all 3 registers are clobbered by the
  // operation, as well as the ip register. RecordWrite updates the
  // write barrier even when storing smis.
  void RecordWrite(Register object,
                   Operand offset,
                   Register scratch0,
                   Register scratch1);

  // For the page containing |object| mark the region covering
  // [address] dirty. The object address must be in the first 8K of an
  // allocated page.  All 3 registers are clobbered by the operation,
  // as well as the ip register. RecordWrite updates the write barrier
  // even when storing smis.
  void RecordWrite(Register object,
                   Register address,
                   Register scratch);

  // Push two registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Condition cond = al) {
    ASSERT(!src1.is(src2));
    if (src1.code() > src2.code()) {
      stm(db_w, sp, src1.bit() | src2.bit(), cond);
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      str(src2, MemOperand(sp, 4, NegPreIndex), cond);
    }
  }

  // Push three registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Condition cond = al) {
    ASSERT(!src1.is(src2));
    ASSERT(!src2.is(src3));
    ASSERT(!src1.is(src3));
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        stm(db_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
      } else {
        stm(db_w, sp, src1.bit() | src2.bit(), cond);
        str(src3, MemOperand(sp, 4, NegPreIndex), cond);
      }
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      Push(src2, src3, cond);
    }
  }

  // Push four registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2,
            Register src3, Register src4, Condition cond = al) {
    ASSERT(!src1.is(src2));
    ASSERT(!src2.is(src3));
    ASSERT(!src1.is(src3));
    ASSERT(!src1.is(src4));
    ASSERT(!src2.is(src4));
    ASSERT(!src3.is(src4));
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        if (src3.code() > src4.code()) {
          stm(db_w,
              sp,
              src1.bit() | src2.bit() | src3.bit() | src4.bit(),
              cond);
        } else {
          stm(db_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
          str(src4, MemOperand(sp, 4, NegPreIndex), cond);
        }
      } else {
        stm(db_w, sp, src1.bit() | src2.bit(), cond);
        Push(src3, src4, cond);
      }
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      Push(src2, src3, src4, cond);
    }
  }

  // Pop two registers. Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Condition cond = al) {
    ASSERT(!src1.is(src2));
    if (src1.code() > src2.code()) {
      ldm(ia_w, sp, src1.bit() | src2.bit(), cond);
    } else {
      ldr(src2, MemOperand(sp, 4, PostIndex), cond);
      ldr(src1, MemOperand(sp, 4, PostIndex), cond);
    }
  }

  // Push and pop the registers that can hold pointers, as defined by the
  // RegList constant kSafepointSavedRegisters.
  void PushSafepointRegisters();
  void PopSafepointRegisters();
  void PushSafepointRegistersAndDoubles();
  void PopSafepointRegistersAndDoubles();
  // Store value in register src in the safepoint stack slot for
  // register dst.
  void StoreToSafepointRegisterSlot(Register src, Register dst);
  void StoreToSafepointRegistersAndDoublesSlot(Register src, Register dst);
  // Load the value of the src register from its safepoint stack slot
  // into register dst.
  void LoadFromSafepointRegisterSlot(Register dst, Register src);

  // Load two consecutive registers with two consecutive memory locations.
  void Ldrd(Register dst1,
            Register dst2,
            const MemOperand& src,
            Condition cond = al);

  // Store two consecutive registers to two consecutive memory locations.
  void Strd(Register src1,
            Register src2,
            const MemOperand& dst,
            Condition cond = al);

  // Clear specified FPSCR bits.
  void ClearFPSCRBits(const uint32_t bits_to_clear,
                      const Register scratch,
                      const Condition cond = al);

  // Compare double values and move the result to the normal condition flags.
  void VFPCompareAndSetFlags(const DwVfpRegister src1,
                             const DwVfpRegister src2,
                             const Condition cond = al);
  void VFPCompareAndSetFlags(const DwVfpRegister src1,
                             const double src2,
                             const Condition cond = al);

  // Compare double values and then load the fpscr flags to a register.
  void VFPCompareAndLoadFlags(const DwVfpRegister src1,
                              const DwVfpRegister src2,
                              const Register fpscr_flags,
                              const Condition cond = al);
  void VFPCompareAndLoadFlags(const DwVfpRegister src1,
                              const double src2,
                              const Register fpscr_flags,
                              const Condition cond = al);


  // ---------------------------------------------------------------------------
  // Activation frames

  void EnterInternalFrame() { EnterFrame(StackFrame::INTERNAL); }
  void LeaveInternalFrame() { LeaveFrame(StackFrame::INTERNAL); }

  void EnterConstructFrame() { EnterFrame(StackFrame::CONSTRUCT); }
  void LeaveConstructFrame() { LeaveFrame(StackFrame::CONSTRUCT); }

  // Enter exit frame.
  // stack_space - extra stack space, used for alignment before call to C.
  void EnterExitFrame(bool save_doubles, int stack_space = 0);

  // Leave the current exit frame. Expects the return value in r0.
  // Expect the number of values, pushed prior to the exit frame, to
  // remove in a register (or no_reg, if there is nothing to remove).
  void LeaveExitFrame(bool save_doubles, Register argument_count);

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();

  void LoadContext(Register dst, int context_chain_length);

  void LoadGlobalFunction(int index, Register function);

  // Load the initial map from the global function. The registers
  // function and map can be the same, function is then overwritten.
  void LoadGlobalFunctionInitialMap(Register function,
                                    Register map,
                                    Register scratch);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeCode(Register code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  InvokeFlag flag,
                  PostCallGenerator* post_call_generator = NULL);

  void InvokeCode(Handle<Code> code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  RelocInfo::Mode rmode,
                  InvokeFlag flag);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      PostCallGenerator* post_call_generator = NULL);

  void InvokeFunction(JSFunction* function,
                      const ParameterCount& actual,
                      InvokeFlag flag);

  void IsObjectJSObjectType(Register heap_object,
                            Register map,
                            Register scratch,
                            Label* fail);

  void IsInstanceJSObjectType(Register map,
                              Register scratch,
                              Label* fail);

  void IsObjectJSStringType(Register object,
                            Register scratch,
                            Label* fail);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // ---------------------------------------------------------------------------
  // Debugger Support

  void DebugBreak();
#endif

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link into try handler chain.
  // The return address must be passed in register lr.
  // On exit, r0 contains TOS (code slot).
  void PushTryHandler(CodeLocation try_location, HandlerType type);

  // Unlink the stack handler on top of the stack from the try handler chain.
  // Must preserve the result register.
  void PopTryHandler();

  // Passes thrown value (in r0) to the handler of top of the try handler chain.
  void Throw(Register value);

  // Propagates an uncatchable exception to the top of the current JS stack's
  // handler chain.
  void ThrowUncatchable(UncatchableExceptionType type, Register value);

  // ---------------------------------------------------------------------------
  // Inline caching support

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, whereas both scratch registers are clobbered.
  void CheckAccessGlobalProxy(Register holder_reg,
                              Register scratch,
                              Label* miss);

  inline void MarkCode(NopMarkerTypes type) {
    nop(type);
  }

  // Check if the given instruction is a 'type' marker.
  // ie. check if is is a mov r<type>, r<type> (referenced as nop(type))
  // These instructions are generated to mark special location in the code,
  // like some special IC code.
  static inline bool IsMarkedCode(Instr instr, int type) {
    ASSERT((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER));
    return IsNop(instr, type);
  }


  static inline int GetCodeMarker(Instr instr) {
    int dst_reg_offset = 12;
    int dst_mask = 0xf << dst_reg_offset;
    int src_mask = 0xf;
    int dst_reg = (instr & dst_mask) >> dst_reg_offset;
    int src_reg = instr & src_mask;
    uint32_t non_register_mask = ~(dst_mask | src_mask);
    uint32_t mov_mask = al | 13 << 21;

    // Return <n> if we have a mov rn rn, else return -1.
    int type = ((instr & non_register_mask) == mov_mask) &&
               (dst_reg == src_reg) &&
               (FIRST_IC_MARKER <= dst_reg) && (dst_reg < LAST_CODE_MARKER)
                   ? src_reg
                   : -1;
    ASSERT((type == -1) ||
           ((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER)));
    return type;
  }


  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space. The object_size is specified
  // either in bytes or in words if the allocation flag SIZE_IN_WORDS
  // is passed. If the new space is exhausted control continues at the
  // gc_required label. The allocated object is returned in result. If
  // the flag tag_allocated_object is true the result is tagged as as
  // a heap object. All registers are clobbered also when control
  // continues at the gc_required label.
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
  void AllocateTwoByteConsString(Register result,
                                 Register length,
                                 Register scratch1,
                                 Register scratch2,
                                 Label* gc_required);
  void AllocateAsciiConsString(Register result,
                               Register length,
                               Register scratch1,
                               Register scratch2,
                               Label* gc_required);

  // Allocates a heap number or jumps to the gc_required label if the young
  // space is full and a scavenge is needed. All registers are clobbered also
  // when control continues at the gc_required label.
  void AllocateHeapNumber(Register result,
                          Register scratch1,
                          Register scratch2,
                          Register heap_number_map,
                          Label* gc_required);
  void AllocateHeapNumberWithValue(Register result,
                                   DwVfpRegister value,
                                   Register scratch1,
                                   Register scratch2,
                                   Register heap_number_map,
                                   Label* gc_required);

  // Copies a fixed number of fields of heap objects from src to dst.
  void CopyFields(Register dst, Register src, RegList temps, int field_count);

  // Copies a number of bytes from src to dst. All registers are clobbered. On
  // exit src and dst will point to the place just after where the last byte was
  // read or written and length will be zero.
  void CopyBytes(Register src,
                 Register dst,
                 Register length,
                 Register scratch);

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


  // Check if the map of an object is equal to a specified map (either
  // given directly or as an index into the root list) and branch to
  // label if not. Skip the smi check if not required (object is known
  // to be a heap object)
  void CheckMap(Register obj,
                Register scratch,
                Handle<Map> map,
                Label* fail,
                bool is_heap_object);

  void CheckMap(Register obj,
                Register scratch,
                Heap::RootListIndex index,
                Label* fail,
                bool is_heap_object);


  // Load and check the instance type of an object for being a string.
  // Loads the type into the second argument register.
  // Returns a condition that will be enabled if the object was a string.
  Condition IsObjectStringType(Register obj,
                               Register type) {
    ldr(type, FieldMemOperand(obj, HeapObject::kMapOffset));
    ldrb(type, FieldMemOperand(type, Map::kInstanceTypeOffset));
    tst(type, Operand(kIsNotStringMask));
    ASSERT_EQ(0, kStringTag);
    return eq;
  }


  // Generates code for reporting that an illegal operation has
  // occurred.
  void IllegalOperation(int num_arguments);

  // Picks out an array index from the hash field.
  // Register use:
  //   hash - holds the index's hash. Clobbered.
  //   index - holds the overwritten index on exit.
  void IndexFromHash(Register hash, Register index);

  // Get the number of least significant bits from a register
  void GetLeastBitsFromSmi(Register dst, Register src, int num_least_bits);
  void GetLeastBitsFromInt32(Register dst, Register src, int mun_least_bits);

  // Uses VFP instructions to Convert a Smi to a double.
  void IntegerToDoubleConversionWithVFP3(Register inReg,
                                         Register outHighReg,
                                         Register outLowReg);

  // Load the value of a number object into a VFP double register. If the object
  // is not a number a jump to the label not_number is performed and the VFP
  // double register is unchanged.
  void ObjectToDoubleVFPRegister(
      Register object,
      DwVfpRegister value,
      Register scratch1,
      Register scratch2,
      Register heap_number_map,
      SwVfpRegister scratch3,
      Label* not_number,
      ObjectToDoubleFlags flags = NO_OBJECT_TO_DOUBLE_FLAGS);

  // Load the value of a smi object into a VFP double register. The register
  // scratch1 can be the same register as smi in which case smi will hold the
  // untagged value afterwards.
  void SmiToDoubleVFPRegister(Register smi,
                              DwVfpRegister value,
                              Register scratch1,
                              SwVfpRegister scratch2);

  // Convert the HeapNumber pointed to by source to a 32bits signed integer
  // dest. If the HeapNumber does not fit into a 32bits signed integer branch
  // to not_int32 label. If VFP3 is available double_scratch is used but not
  // scratch2.
  void ConvertToInt32(Register source,
                      Register dest,
                      Register scratch,
                      Register scratch2,
                      DwVfpRegister double_scratch,
                      Label *not_int32);

// Truncates a double using a specific rounding mode.
// Clears the z flag (ne condition) if an overflow occurs.
// If exact_conversion is true, the z flag is also cleared if the conversion
// was inexact, ie. if the double value could not be converted exactly
// to a 32bit integer.
  void EmitVFPTruncate(VFPRoundingMode rounding_mode,
                       SwVfpRegister result,
                       DwVfpRegister double_input,
                       Register scratch1,
                       Register scratch2,
                       CheckForInexactConversion check
                           = kDontCheckForInexactConversion);

  // Count leading zeros in a 32 bit word.  On ARM5 and later it uses the clz
  // instruction.  On pre-ARM5 hardware this routine gives the wrong answer
  // for 0 (31 instead of 32).  Source and scratch can be the same in which case
  // the source is clobbered.  Source and zeros can also be the same in which
  // case scratch should be a different register.
  void CountLeadingZeros(Register zeros,
                         Register source,
                         Register scratch);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub, Condition cond = al);

  // Call a code stub.
  void TailCallStub(CodeStub* stub, Condition cond = al);

  // Tail call a code stub (jump) and return the code object called.  Try to
  // generate the code if necessary.  Do not perform a GC but instead return
  // a retry after GC failure.
  MUST_USE_RESULT MaybeObject* TryTailCallStub(CodeStub* stub,
                                               Condition cond = al);

  // Call a runtime routine.
  void CallRuntime(Runtime::Function* f, int num_arguments);
  void CallRuntimeSaveDoubles(Runtime::FunctionId id);

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid, int num_arguments);

  // Convenience function: call an external reference.
  void CallExternalReference(const ExternalReference& ext,
                             int num_arguments);

  // Tail call of a runtime routine (jump).
  // Like JumpToExternalReference, but also takes care of passing the number
  // of parameters.
  void TailCallExternalReference(const ExternalReference& ext,
                                 int num_arguments,
                                 int result_size);

  // Tail call of a runtime routine (jump). Try to generate the code if
  // necessary. Do not perform a GC but instead return a retry after GC
  // failure.
  MUST_USE_RESULT MaybeObject* TryTailCallExternalReference(
      const ExternalReference& ext, int num_arguments, int result_size);

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid,
                       int num_arguments,
                       int result_size);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, non-register arguments must be stored in
  // sp[0], sp[4], etc., not pushed. The argument count assumes all arguments
  // are word sized.
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

  void GetCFunctionDoubleResult(const DoubleRegister dst);

  // Calls an API function. Allocates HandleScope, extracts returned value
  // from handle and propagates exceptions. Restores context.
  // stack_space - space to be unwound on exit (includes the call js
  // arguments space and the additional space allocated for the fast call).
  MaybeObject* TryCallApiFunctionAndReturn(ExternalReference function,
                                           int stack_space);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& builtin);

  MaybeObject* TryJumpToExternalReference(const ExternalReference& ext);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id,
                     InvokeJSFlags flags,
                     PostCallGenerator* post_call_generator = NULL);

  // Store the code object for the given builtin in the target register and
  // setup the function in r1.
  void GetBuiltinEntry(Register target, Builtins::JavaScript id);

  // Store the function for the given builtin in the target register.
  void GetBuiltinFunction(Register target, Builtins::JavaScript id);

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

  // Calls Abort(msg) if the condition cond is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cond, const char* msg);
  void AssertRegisterIsRoot(Register reg, Heap::RootListIndex index);
  void AssertFastElements(Register elements);

  // Like Assert(), but always enabled.
  void Check(Condition cond, const char* msg);

  // Print a message to stdout and abort execution.
  void Abort(const char* msg);

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_allow_stub_calls(bool value) { allow_stub_calls_ = value; }
  bool allow_stub_calls() { return allow_stub_calls_; }

  // ---------------------------------------------------------------------------
  // Number utilities

  // Check whether the value of reg is a power of two and not zero. If not
  // control continues at the label not_power_of_two. If reg is a power of two
  // the register scratch contains the value of (reg - 1) when control falls
  // through.
  void JumpIfNotPowerOfTwoOrZero(Register reg,
                                 Register scratch,
                                 Label* not_power_of_two_or_zero);

  // ---------------------------------------------------------------------------
  // Smi utilities

  void SmiTag(Register reg, SBit s = LeaveCC) {
    add(reg, reg, Operand(reg), s);
  }
  void SmiTag(Register dst, Register src, SBit s = LeaveCC) {
    add(dst, src, Operand(src), s);
  }

  // Try to convert int32 to smi. If the value is to large, preserve
  // the original value and jump to not_a_smi. Destroys scratch and
  // sets flags.
  void TrySmiTag(Register reg, Label* not_a_smi, Register scratch) {
    mov(scratch, reg);
    SmiTag(scratch, SetCC);
    b(vs, not_a_smi);
    mov(reg, scratch);
  }

  void SmiUntag(Register reg, SBit s = LeaveCC) {
    mov(reg, Operand(reg, ASR, kSmiTagSize), s);
  }
  void SmiUntag(Register dst, Register src, SBit s = LeaveCC) {
    mov(dst, Operand(src, ASR, kSmiTagSize), s);
  }

  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label) {
    tst(value, Operand(kSmiTagMask));
    b(eq, smi_label);
  }
  // Jump if either of the registers contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label) {
    tst(value, Operand(kSmiTagMask));
    b(ne, not_smi_label);
  }
  // Jump if either of the registers contain a non-smi.
  void JumpIfNotBothSmi(Register reg1, Register reg2, Label* on_not_both_smi);
  // Jump if either of the registers contain a smi.
  void JumpIfEitherSmi(Register reg1, Register reg2, Label* on_either_smi);

  // Abort execution if argument is a smi. Used in debug code.
  void AbortIfSmi(Register object);
  void AbortIfNotSmi(Register object);

  // Abort execution if argument is a string. Used in debug code.
  void AbortIfNotString(Register object);

  // Abort execution if argument is not the root value with the given index.
  void AbortIfNotRootValue(Register src,
                           Heap::RootListIndex root_value_index,
                           const char* message);

  // ---------------------------------------------------------------------------
  // HeapNumber utilities

  void JumpIfNotHeapNumber(Register object,
                           Register heap_number_map,
                           Register scratch,
                           Label* on_not_heap_number);

  // ---------------------------------------------------------------------------
  // String utilities

  // Checks if both objects are sequential ASCII strings and jumps to label
  // if either is not. Assumes that neither object is a smi.
  void JumpIfNonSmisNotBothSequentialAsciiStrings(Register object1,
                                                  Register object2,
                                                  Register scratch1,
                                                  Register scratch2,
                                                  Label* failure);

  // Checks if both objects are sequential ASCII strings and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialAsciiStrings(Register first,
                                           Register second,
                                           Register scratch1,
                                           Register scratch2,
                                           Label* not_flat_ascii_strings);

  // Checks if both instance types are sequential ASCII strings and jumps to
  // label if either is not.
  void JumpIfBothInstanceTypesAreNotSequentialAscii(
      Register first_object_instance_type,
      Register second_object_instance_type,
      Register scratch1,
      Register scratch2,
      Label* failure);

  // Check if instance type is sequential ASCII string and jump to label if
  // it is not.
  void JumpIfInstanceTypeIsNotSequentialAscii(Register type,
                                              Register scratch,
                                              Label* failure);


  // ---------------------------------------------------------------------------
  // Patching helpers.

  // Get the location of a relocated constant (its address in the constant pool)
  // from its load site.
  void GetRelocatedValueLocation(Register ldr_location,
                                 Register result);


 private:
  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al);
  void Call(intptr_t target, RelocInfo::Mode rmode, Condition cond = al);

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Handle<Code> code_constant,
                      Register code_reg,
                      Label* done,
                      InvokeFlag flag,
                      PostCallGenerator* post_call_generator = NULL);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void LeaveFrame(StackFrame::Type type);

  void InitializeNewString(Register string,
                           Register length,
                           Heap::RootListIndex map_index,
                           Register scratch1,
                           Register scratch2);

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);
  MemOperand SafepointRegisterSlot(Register reg);
  MemOperand SafepointRegistersAndDoublesSlot(Register reg);

  bool generating_stub_;
  bool allow_stub_calls_;
  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Needs access to SafepointRegisterStackIndex for optimized frame
  // traversal.
  friend class OptimizedFrame;
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
  void Emit(Instr instr);

  // Emit an address directly.
  void Emit(Address addr);

  // Emit the condition part of an instruction leaving the rest of the current
  // instruction unchanged.
  void EmitCondition(Condition cond);

 private:
  byte* address_;  // The address of the code being patched.
  int instructions_;  // Number of instructions of the expected patch size.
  int size_;  // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
};
#endif  // ENABLE_DEBUGGER_SUPPORT


// Helper class for generating code or data associated with the code
// right after a call instruction. As an example this can be used to
// generate safepoint data after calls for crankshaft.
class PostCallGenerator {
 public:
  PostCallGenerator() { }
  virtual ~PostCallGenerator() { }
  virtual void Generate() = 0;
};


// -----------------------------------------------------------------------------
// Static helper functions.

static MemOperand ContextOperand(Register context, int index) {
  return MemOperand(context, Context::SlotOffset(index));
}


static inline MemOperand GlobalObjectOperand()  {
  return ContextOperand(cp, Context::GLOBAL_INDEX);
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
