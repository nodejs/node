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

#ifndef V8_X64_MACRO_ASSEMBLER_X64_H_
#define V8_X64_MACRO_ASSEMBLER_X64_H_

#include "assembler.h"

namespace v8 {
namespace internal {

// Flags used for the AllocateInNewSpace functions.
enum AllocationFlags {
  // No special flags.
  NO_ALLOCATION_FLAGS = 0,
  // Return the pointer to the allocated already tagged as a heap object.
  TAG_OBJECT = 1 << 0,
  // The content of the result register already contains the allocation top in
  // new space.
  RESULT_CONTAINS_TOP = 1 << 1
};

// Default scratch register used by MacroAssembler (and other code that needs
// a spare register). The register isn't callee save, and not used by the
// function calling convention.
static const Register kScratchRegister = { 10 };      // r10.
static const Register kSmiConstantRegister = { 15 };  // r15 (callee save).
static const Register kRootRegister = { 13 };         // r13 (callee save).
// Value of smi in kSmiConstantRegister.
static const int kSmiConstantRegisterValue = 1;

// Convenience for platform-independent signatures.
typedef Operand MemOperand;

// Forward declaration.
class JumpTarget;

struct SmiIndex {
  SmiIndex(Register index_register, ScaleFactor scale)
      : reg(index_register),
        scale(scale) {}
  Register reg;
  ScaleFactor scale;
};

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(void* buffer, int size);

  void LoadRoot(Register destination, Heap::RootListIndex index);
  void CompareRoot(Register with, Heap::RootListIndex index);
  void CompareRoot(Operand with, Heap::RootListIndex index);
  void PushRoot(Heap::RootListIndex index);
  void StoreRoot(Register source, Heap::RootListIndex index);

  // ---------------------------------------------------------------------------
  // GC Support

  // For page containing |object| mark region covering |addr| dirty.
  // RecordWriteHelper only works if the object is not in new
  // space.
  void RecordWriteHelper(Register object,
                         Register addr,
                         Register scratch);

  // Check if object is in new space. The condition cc can be equal or
  // not_equal. If it is equal a jump will be done if the object is on new
  // space. The register scratch can be object itself, but it will be clobbered.
  template <typename LabelType>
  void InNewSpace(Register object,
                  Register scratch,
                  Condition cc,
                  LabelType* branch);

  // For page containing |object| mark region covering [object+offset]
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. If |offset| is zero, then the |scratch|
  // register contains the array index into the elements array
  // represented as an untagged 32-bit integer. All registers are
  // clobbered by the operation. RecordWrite filters out smis so it
  // does not update the write barrier if the value is a smi.
  void RecordWrite(Register object,
                   int offset,
                   Register value,
                   Register scratch);

  // For page containing |object| mark region covering [address]
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. All registers are clobbered by the
  // operation.  RecordWrite filters out smis so it does not update
  // the write barrier if the value is a smi.
  void RecordWrite(Register object,
                   Register address,
                   Register value);

  // For page containing |object| mark region covering [object+offset] dirty.
  // The value is known to not be a smi.
  // object is the object being stored into, value is the object being stored.
  // If offset is zero, then the scratch register contains the array index into
  // the elements array represented as an untagged 32-bit integer.
  // All registers are clobbered by the operation.
  void RecordWriteNonSmi(Register object,
                         int offset,
                         Register value,
                         Register scratch);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // ---------------------------------------------------------------------------
  // Debugger Support

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

  // Enter specific kind of exit frame; either in normal or
  // debug mode. Expects the number of arguments in register rax and
  // sets up the number of arguments in register rdi and the pointer
  // to the first argument in register rsi.
  void EnterExitFrame(int result_size = 1);

  void EnterApiExitFrame(int stack_space,
                         int argc,
                         int result_size = 1);

  // Leave the current exit frame. Expects/provides the return value in
  // register rax:rdx (untouched) and the pointer to the first
  // argument in register rsi.
  void LeaveExitFrame(int result_size = 1);


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

  void InvokeFunction(JSFunction* function,
                      const ParameterCount& actual,
                      InvokeFlag flag);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id, InvokeFlag flag);

  // Store the function for the given builtin in the target register.
  void GetBuiltinFunction(Register target, Builtins::JavaScript id);

  // Store the code object for the given builtin in the target register.
  void GetBuiltinEntry(Register target, Builtins::JavaScript id);


  // ---------------------------------------------------------------------------
  // Smi tagging, untagging and operations on tagged smis.

  void InitializeSmiConstantRegister() {
    movq(kSmiConstantRegister,
         reinterpret_cast<uint64_t>(Smi::FromInt(kSmiConstantRegisterValue)),
         RelocInfo::NONE);
  }

  // Conversions between tagged smi values and non-tagged integer values.

  // Tag an integer value. The result must be known to be a valid smi value.
  // Only uses the low 32 bits of the src register. Sets the N and Z flags
  // based on the value of the resulting smi.
  void Integer32ToSmi(Register dst, Register src);

  // Stores an integer32 value into a memory field that already holds a smi.
  void Integer32ToSmiField(const Operand& dst, Register src);

  // Adds constant to src and tags the result as a smi.
  // Result must be a valid smi.
  void Integer64PlusConstantToSmi(Register dst, Register src, int constant);

  // Convert smi to 32-bit integer. I.e., not sign extended into
  // high 32 bits of destination.
  void SmiToInteger32(Register dst, Register src);
  void SmiToInteger32(Register dst, const Operand& src);

  // Convert smi to 64-bit integer (sign extended if necessary).
  void SmiToInteger64(Register dst, Register src);
  void SmiToInteger64(Register dst, const Operand& src);

  // Multiply a positive smi's integer value by a power of two.
  // Provides result as 64-bit integer value.
  void PositiveSmiTimesPowerOfTwoToInteger64(Register dst,
                                             Register src,
                                             int power);

  // Divide a positive smi's integer value by a power of two.
  // Provides result as 32-bit integer value.
  void PositiveSmiDivPowerOfTwoToInteger32(Register dst,
                                           Register src,
                                           int power);


  // Simple comparison of smis.
  void SmiCompare(Register dst, Register src);
  void SmiCompare(Register dst, Smi* src);
  void SmiCompare(Register dst, const Operand& src);
  void SmiCompare(const Operand& dst, Register src);
  void SmiCompare(const Operand& dst, Smi* src);
  // Compare the int32 in src register to the value of the smi stored at dst.
  void SmiCompareInteger32(const Operand& dst, Register src);
  // Sets sign and zero flags depending on value of smi in register.
  void SmiTest(Register src);

  // Functions performing a check on a known or potential smi. Returns
  // a condition that is satisfied if the check is successful.

  // Is the value a tagged smi.
  Condition CheckSmi(Register src);

  // Is the value a non-negative tagged smi.
  Condition CheckNonNegativeSmi(Register src);

  // Are both values tagged smis.
  Condition CheckBothSmi(Register first, Register second);

  // Are both values non-negative tagged smis.
  Condition CheckBothNonNegativeSmi(Register first, Register second);

  // Are either value a tagged smi.
  Condition CheckEitherSmi(Register first,
                           Register second,
                           Register scratch = kScratchRegister);

  // Is the value the minimum smi value (since we are using
  // two's complement numbers, negating the value is known to yield
  // a non-smi value).
  Condition CheckIsMinSmi(Register src);

  // Checks whether an 32-bit integer value is a valid for conversion
  // to a smi.
  Condition CheckInteger32ValidSmiValue(Register src);

  // Checks whether an 32-bit unsigned integer value is a valid for
  // conversion to a smi.
  Condition CheckUInteger32ValidSmiValue(Register src);

  // Test-and-jump functions. Typically combines a check function
  // above with a conditional jump.

  // Jump if the value cannot be represented by a smi.
  template <typename LabelType>
  void JumpIfNotValidSmiValue(Register src, LabelType* on_invalid);

  // Jump if the unsigned integer value cannot be represented by a smi.
  template <typename LabelType>
  void JumpIfUIntNotValidSmiValue(Register src, LabelType* on_invalid);

  // Jump to label if the value is a tagged smi.
  template <typename LabelType>
  void JumpIfSmi(Register src, LabelType* on_smi);

  // Jump to label if the value is not a tagged smi.
  template <typename LabelType>
  void JumpIfNotSmi(Register src, LabelType* on_not_smi);

  // Jump to label if the value is not a non-negative tagged smi.
  template <typename LabelType>
  void JumpUnlessNonNegativeSmi(Register src, LabelType* on_not_smi);

  // Jump to label if the value, which must be a tagged smi, has value equal
  // to the constant.
  template <typename LabelType>
  void JumpIfSmiEqualsConstant(Register src,
                               Smi* constant,
                               LabelType* on_equals);

  // Jump if either or both register are not smi values.
  template <typename LabelType>
  void JumpIfNotBothSmi(Register src1,
                        Register src2,
                        LabelType* on_not_both_smi);

  // Jump if either or both register are not non-negative smi values.
  template <typename LabelType>
  void JumpUnlessBothNonNegativeSmi(Register src1, Register src2,
                                    LabelType* on_not_both_smi);

  // Operations on tagged smi values.

  // Smis represent a subset of integers. The subset is always equivalent to
  // a two's complement interpretation of a fixed number of bits.

  // Optimistically adds an integer constant to a supposed smi.
  // If the src is not a smi, or the result is not a smi, jump to
  // the label.
  template <typename LabelType>
  void SmiTryAddConstant(Register dst,
                         Register src,
                         Smi* constant,
                         LabelType* on_not_smi_result);

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  void SmiAddConstant(Register dst, Register src, Smi* constant);

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  void SmiAddConstant(const Operand& dst, Smi* constant);

  // Add an integer constant to a tagged smi, giving a tagged smi as result,
  // or jumping to a label if the result cannot be represented by a smi.
  template <typename LabelType>
  void SmiAddConstant(Register dst,
                      Register src,
                      Smi* constant,
                      LabelType* on_not_smi_result);

  // Subtract an integer constant from a tagged smi, giving a tagged smi as
  // result. No testing on the result is done. Sets the N and Z flags
  // based on the value of the resulting integer.
  void SmiSubConstant(Register dst, Register src, Smi* constant);

  // Subtract an integer constant from a tagged smi, giving a tagged smi as
  // result, or jumping to a label if the result cannot be represented by a smi.
  template <typename LabelType>
  void SmiSubConstant(Register dst,
                      Register src,
                      Smi* constant,
                      LabelType* on_not_smi_result);

  // Negating a smi can give a negative zero or too large positive value.
  // NOTICE: This operation jumps on success, not failure!
  template <typename LabelType>
  void SmiNeg(Register dst,
              Register src,
              LabelType* on_smi_result);

  // Adds smi values and return the result as a smi.
  // If dst is src1, then src1 will be destroyed, even if
  // the operation is unsuccessful.
  template <typename LabelType>
  void SmiAdd(Register dst,
              Register src1,
              Register src2,
              LabelType* on_not_smi_result);

  void SmiAdd(Register dst,
              Register src1,
              Register src2);

  // Subtracts smi values and return the result as a smi.
  // If dst is src1, then src1 will be destroyed, even if
  // the operation is unsuccessful.
  template <typename LabelType>
  void SmiSub(Register dst,
              Register src1,
              Register src2,
              LabelType* on_not_smi_result);

  void SmiSub(Register dst,
              Register src1,
              Register src2);

  template <typename LabelType>
  void SmiSub(Register dst,
              Register src1,
              const Operand& src2,
              LabelType* on_not_smi_result);

  void SmiSub(Register dst,
              Register src1,
              const Operand& src2);

  // Multiplies smi values and return the result as a smi,
  // if possible.
  // If dst is src1, then src1 will be destroyed, even if
  // the operation is unsuccessful.
  template <typename LabelType>
  void SmiMul(Register dst,
              Register src1,
              Register src2,
              LabelType* on_not_smi_result);

  // Divides one smi by another and returns the quotient.
  // Clobbers rax and rdx registers.
  template <typename LabelType>
  void SmiDiv(Register dst,
              Register src1,
              Register src2,
              LabelType* on_not_smi_result);

  // Divides one smi by another and returns the remainder.
  // Clobbers rax and rdx registers.
  template <typename LabelType>
  void SmiMod(Register dst,
              Register src1,
              Register src2,
              LabelType* on_not_smi_result);

  // Bitwise operations.
  void SmiNot(Register dst, Register src);
  void SmiAnd(Register dst, Register src1, Register src2);
  void SmiOr(Register dst, Register src1, Register src2);
  void SmiXor(Register dst, Register src1, Register src2);
  void SmiAndConstant(Register dst, Register src1, Smi* constant);
  void SmiOrConstant(Register dst, Register src1, Smi* constant);
  void SmiXorConstant(Register dst, Register src1, Smi* constant);

  void SmiShiftLeftConstant(Register dst,
                            Register src,
                            int shift_value);
  template <typename LabelType>
  void SmiShiftLogicalRightConstant(Register dst,
                                  Register src,
                                  int shift_value,
                                  LabelType* on_not_smi_result);
  void SmiShiftArithmeticRightConstant(Register dst,
                                       Register src,
                                       int shift_value);

  // Shifts a smi value to the left, and returns the result if that is a smi.
  // Uses and clobbers rcx, so dst may not be rcx.
  void SmiShiftLeft(Register dst,
                    Register src1,
                    Register src2);
  // Shifts a smi value to the right, shifting in zero bits at the top, and
  // returns the unsigned intepretation of the result if that is a smi.
  // Uses and clobbers rcx, so dst may not be rcx.
  template <typename LabelType>
  void SmiShiftLogicalRight(Register dst,
                            Register src1,
                            Register src2,
                            LabelType* on_not_smi_result);
  // Shifts a smi value to the right, sign extending the top, and
  // returns the signed intepretation of the result. That will always
  // be a valid smi value, since it's numerically smaller than the
  // original.
  // Uses and clobbers rcx, so dst may not be rcx.
  void SmiShiftArithmeticRight(Register dst,
                               Register src1,
                               Register src2);

  // Specialized operations

  // Select the non-smi register of two registers where exactly one is a
  // smi. If neither are smis, jump to the failure label.
  template <typename LabelType>
  void SelectNonSmi(Register dst,
                    Register src1,
                    Register src2,
                    LabelType* on_not_smis);

  // Converts, if necessary, a smi to a combination of number and
  // multiplier to be used as a scaled index.
  // The src register contains a *positive* smi value. The shift is the
  // power of two to multiply the index value by (e.g.
  // to index by smi-value * kPointerSize, pass the smi and kPointerSizeLog2).
  // The returned index register may be either src or dst, depending
  // on what is most efficient. If src and dst are different registers,
  // src is always unchanged.
  SmiIndex SmiToIndex(Register dst, Register src, int shift);

  // Converts a positive smi to a negative index.
  SmiIndex SmiToNegativeIndex(Register dst, Register src, int shift);

  // Basic Smi operations.
  void Move(Register dst, Smi* source) {
    LoadSmiConstant(dst, source);
  }

  void Move(const Operand& dst, Smi* source) {
    Register constant = GetSmiConstant(source);
    movq(dst, constant);
  }

  void Push(Smi* smi);
  void Test(const Operand& dst, Smi* source);

  // ---------------------------------------------------------------------------
  // String macros.
  template <typename LabelType>
  void JumpIfNotBothSequentialAsciiStrings(Register first_object,
                                           Register second_object,
                                           Register scratch1,
                                           Register scratch2,
                                           LabelType* on_not_both_flat_ascii);

  // Check whether the instance type represents a flat ascii string. Jump to the
  // label if not. If the instance type can be scratched specify same register
  // for both instance type and scratch.
  template <typename LabelType>
  void JumpIfInstanceTypeIsNotSequentialAscii(
      Register instance_type,
      Register scratch,
      LabelType *on_not_flat_ascii_string);

  template <typename LabelType>
  void JumpIfBothInstanceTypesAreNotSequentialAscii(
      Register first_object_instance_type,
      Register second_object_instance_type,
      Register scratch1,
      Register scratch2,
      LabelType* on_fail);

  // ---------------------------------------------------------------------------
  // Macro instructions.

  // Load a register with a long value as efficiently as possible.
  void Set(Register dst, int64_t x);
  void Set(const Operand& dst, int64_t x);

  // Move if the registers are not identical.
  void Move(Register target, Register source);

  // Handle support
  void Move(Register dst, Handle<Object> source);
  void Move(const Operand& dst, Handle<Object> source);
  void Cmp(Register dst, Handle<Object> source);
  void Cmp(const Operand& dst, Handle<Object> source);
  void Push(Handle<Object> source);

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the rsp register.
  void Drop(int stack_elements);

  void Call(Label* target) { call(target); }

  // Control Flow
  void Jump(Address destination, RelocInfo::Mode rmode);
  void Jump(ExternalReference ext);
  void Jump(Handle<Code> code_object, RelocInfo::Mode rmode);

  void Call(Address destination, RelocInfo::Mode rmode);
  void Call(ExternalReference ext);
  void Call(Handle<Code> code_object, RelocInfo::Mode rmode);

  // Compare object type for heap object.
  // Always use unsigned comparisons: above and below, not less and greater.
  // Incoming register is heap_object and outgoing register is map.
  // They may be the same register, and may be kScratchRegister.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  // Always use unsigned comparisons: above and below, not less and greater.
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

  // FCmp compares and pops the two values on top of the FPU stack.
  // The flag results are similar to integer cmp, but requires unsigned
  // jcc instructions (je, ja, jae, jb, jbe, je, and jz).
  void FCmp();

  // Abort execution if argument is not a number. Used in debug code.
  void AbortIfNotNumber(Register object);

  // Abort execution if argument is a smi. Used in debug code.
  void AbortIfSmi(Register object);

  // Abort execution if argument is not a smi. Used in debug code.
  void AbortIfNotSmi(Register object);

  // Abort execution if argument is not the root value with the given index.
  void AbortIfNotRootValue(Register src,
                           Heap::RootListIndex root_value_index,
                           const char* message);

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link into try handler chain.  The return
  // address must be pushed before calling this helper.
  void PushTryHandler(CodeLocation try_location, HandlerType type);

  // Unlink the stack handler on top of the stack from the try handler chain.
  void PopTryHandler();

  // ---------------------------------------------------------------------------
  // Inline caching support

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, but the scratch register and kScratchRegister,
  // which must be different, are clobbered.
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

  // Allocate a heap number in new space with undefined value. Returns
  // tagged pointer in result register, or jumps to gc_required if new
  // space is full.
  void AllocateHeapNumber(Register result,
                          Register scratch,
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
  // function register will be untouched; the other register may be
  // clobbered.
  void TryGetFunctionPrototype(Register function,
                               Register result,
                               Label* miss);

  // Generates code for reporting that an illegal operation has
  // occurred.
  void IllegalOperation(int num_arguments);

  // Picks out an array index from the hash field.
  // Register use:
  //   hash - holds the index's hash. Clobbered.
  //   index - holds the overwritten index on exit.
  void IndexFromHash(Register hash, Register index);

  // Find the function context up the context chain.
  void LoadContext(Register dst, int context_chain_length);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub);

  // Call a code stub and return the code object called.  Try to generate
  // the code if necessary.  Do not perform a GC but instead return a retry
  // after GC failure.
  MUST_USE_RESULT MaybeObject* TryCallStub(CodeStub* stub);

  // Tail call a code stub (jump).
  void TailCallStub(CodeStub* stub);

  // Tail call a code stub (jump) and return the code object called.  Try to
  // generate the code if necessary.  Do not perform a GC but instead return
  // a retry after GC failure.
  MUST_USE_RESULT MaybeObject* TryTailCallStub(CodeStub* stub);

  // Return from a code stub after popping its arguments.
  void StubReturn(int argc);

  // Call a runtime routine.
  void CallRuntime(Runtime::Function* f, int num_arguments);

  // Call a runtime function, returning the CodeStub object called.
  // Try to generate the stub code if necessary.  Do not perform a GC
  // but instead return a retry after GC failure.
  MUST_USE_RESULT MaybeObject* TryCallRuntime(Runtime::Function* f,
                                              int num_arguments);

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId id, int num_arguments);

  // Convenience function: Same as above, but takes the fid instead.
  MUST_USE_RESULT MaybeObject* TryCallRuntime(Runtime::FunctionId id,
                                              int num_arguments);

  // Convenience function: call an external reference.
  void CallExternalReference(const ExternalReference& ext,
                             int num_arguments);

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

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& ext, int result_size);

  // Prepares stack to put arguments (aligns and so on).
  // Uses calle-saved esi to restore stack state after call.
  void PrepareCallApiFunction(int stack_space);

  // Tail call an API function (jump). Allocates HandleScope, extracts
  // returned value from handle and propogates exceptions.
  // Clobbers ebx, edi and caller-save registers.
  void CallApiFunctionAndReturn(ApiFunction* function);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in esp[0], esp[4],
  // etc., not pushed. The argument count assumes all arguments are word sized.
  // The number of slots reserved for arguments depends on platform. On Windows
  // stack slots are reserved for the arguments passed in registers. On other
  // platforms stack slots are only reserved for the arguments actually passed
  // on the stack.
  void PrepareCallCFunction(int num_arguments);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);

  // Calculate the number of stack slots to reserve for arguments when calling a
  // C function.
  int ArgumentStackSlotsForCFunctionCall(int num_arguments);

  // ---------------------------------------------------------------------------
  // Utilities

  void Ret();

  Handle<Object> CodeObject() { return code_object_; }


  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int value);
  void DecrementCounter(StatsCounter* counter, int value);


  // ---------------------------------------------------------------------------
  // Debugging

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, const char* msg);

  void AssertFastElements(Register elements);

  // Like Assert(), but always enabled.
  void Check(Condition cc, const char* msg);

  // Print a message to stdout and abort execution.
  void Abort(const char* msg);

  // Check that the stack is aligned.
  void CheckStackAlignment();

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_allow_stub_calls(bool value) { allow_stub_calls_ = value; }
  bool allow_stub_calls() { return allow_stub_calls_; }

 private:
  bool generating_stub_;
  bool allow_stub_calls_;

  // Returns a register holding the smi value. The register MUST NOT be
  // modified. It may be the "smi 1 constant" register.
  Register GetSmiConstant(Smi* value);

  // Moves the smi value to the destination register.
  void LoadSmiConstant(Register dst, Smi* value);

  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Helper functions for generating invokes.
  template <typename LabelType>
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Handle<Code> code_constant,
                      Register code_register,
                      LabelType* done,
                      InvokeFlag flag);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void LeaveFrame(StackFrame::Type type);

  void EnterExitFramePrologue(bool save_rax);
  void EnterExitFrameEpilogue(int result_size, int argc);

  // Allocation support helpers.
  // Loads the top of new-space into the result register.
  // If flags contains RESULT_CONTAINS_TOP then result_end is valid and
  // already contains the top of new-space, and scratch is invalid.
  // Otherwise the address of the new-space top is loaded into scratch (if
  // scratch is valid), and the new-space top is loaded into result.
  void LoadAllocationTopHelper(Register result,
                               Register result_end,
                               Register scratch,
                               AllocationFlags flags);
  // Update allocation top with value in result_end register.
  // If scratch is valid, it contains the address of the allocation top.
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
    byte* x64_coverage_function =                                         \
        reinterpret_cast<byte*>(FUNCTION_ADDR(LogGeneratedCodeCoverage)); \
    masm->pushfd();                                                       \
    masm->pushad();                                                       \
    masm->push(Immediate(reinterpret_cast<int>(&__FILE_LINE__)));         \
    masm->call(x64_coverage_function, RelocInfo::RUNTIME_ENTRY);          \
    masm->pop(rax);                                                       \
    masm->popad();                                                        \
    masm->popfd();                                                        \
  }                                                                       \
  masm->
#else
#define ACCESS_MASM(masm) masm->
#endif

// -----------------------------------------------------------------------------
// Template implementations.

static int kSmiShift = kSmiTagSize + kSmiShiftSize;


template <typename LabelType>
void MacroAssembler::SmiNeg(Register dst,
                            Register src,
                            LabelType* on_smi_result) {
  if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    movq(kScratchRegister, src);
    neg(dst);  // Low 32 bits are retained as zero by negation.
    // Test if result is zero or Smi::kMinValue.
    cmpq(dst, kScratchRegister);
    j(not_equal, on_smi_result);
    movq(src, kScratchRegister);
  } else {
    movq(dst, src);
    neg(dst);
    cmpq(dst, src);
    // If the result is zero or Smi::kMinValue, negation failed to create a smi.
    j(not_equal, on_smi_result);
  }
}


template <typename LabelType>
void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            Register src2,
                            LabelType* on_not_smi_result) {
  ASSERT_NOT_NULL(on_not_smi_result);
  ASSERT(!dst.is(src2));
  if (dst.is(src1)) {
    movq(kScratchRegister, src1);
    addq(kScratchRegister, src2);
    j(overflow, on_not_smi_result);
    movq(dst, kScratchRegister);
  } else {
    movq(dst, src1);
    addq(dst, src2);
    j(overflow, on_not_smi_result);
  }
}


template <typename LabelType>
void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            Register src2,
                            LabelType* on_not_smi_result) {
  ASSERT_NOT_NULL(on_not_smi_result);
  ASSERT(!dst.is(src2));
  if (dst.is(src1)) {
    cmpq(dst, src2);
    j(overflow, on_not_smi_result);
    subq(dst, src2);
  } else {
    movq(dst, src1);
    subq(dst, src2);
    j(overflow, on_not_smi_result);
  }
}


template <typename LabelType>
void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            const Operand& src2,
                            LabelType* on_not_smi_result) {
  ASSERT_NOT_NULL(on_not_smi_result);
  if (dst.is(src1)) {
    movq(kScratchRegister, src2);
    cmpq(src1, kScratchRegister);
    j(overflow, on_not_smi_result);
    subq(src1, kScratchRegister);
  } else {
    movq(dst, src1);
    subq(dst, src2);
    j(overflow, on_not_smi_result);
  }
}


template <typename LabelType>
void MacroAssembler::SmiMul(Register dst,
                            Register src1,
                            Register src2,
                            LabelType* on_not_smi_result) {
  ASSERT(!dst.is(src2));
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));

  if (dst.is(src1)) {
    NearLabel failure, zero_correct_result;
    movq(kScratchRegister, src1);  // Create backup for later testing.
    SmiToInteger64(dst, src1);
    imul(dst, src2);
    j(overflow, &failure);

    // Check for negative zero result.  If product is zero, and one
    // argument is negative, go to slow case.
    NearLabel correct_result;
    testq(dst, dst);
    j(not_zero, &correct_result);

    movq(dst, kScratchRegister);
    xor_(dst, src2);
    j(positive, &zero_correct_result);  // Result was positive zero.

    bind(&failure);  // Reused failure exit, restores src1.
    movq(src1, kScratchRegister);
    jmp(on_not_smi_result);

    bind(&zero_correct_result);
    xor_(dst, dst);

    bind(&correct_result);
  } else {
    SmiToInteger64(dst, src1);
    imul(dst, src2);
    j(overflow, on_not_smi_result);
    // Check for negative zero result.  If product is zero, and one
    // argument is negative, go to slow case.
    NearLabel correct_result;
    testq(dst, dst);
    j(not_zero, &correct_result);
    // One of src1 and src2 is zero, the check whether the other is
    // negative.
    movq(kScratchRegister, src1);
    xor_(kScratchRegister, src2);
    j(negative, on_not_smi_result);
    bind(&correct_result);
  }
}


template <typename LabelType>
void MacroAssembler::SmiTryAddConstant(Register dst,
                                       Register src,
                                       Smi* constant,
                                       LabelType* on_not_smi_result) {
  // Does not assume that src is a smi.
  ASSERT_EQ(static_cast<int>(1), static_cast<int>(kSmiTagMask));
  ASSERT_EQ(0, kSmiTag);
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src.is(kScratchRegister));

  JumpIfNotSmi(src, on_not_smi_result);
  Register tmp = (dst.is(src) ? kScratchRegister : dst);
  LoadSmiConstant(tmp, constant);
  addq(tmp, src);
  j(overflow, on_not_smi_result);
  if (dst.is(src)) {
    movq(dst, tmp);
  }
}


template <typename LabelType>
void MacroAssembler::SmiAddConstant(Register dst,
                                    Register src,
                                    Smi* constant,
                                    LabelType* on_not_smi_result) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));

    LoadSmiConstant(kScratchRegister, constant);
    addq(kScratchRegister, src);
    j(overflow, on_not_smi_result);
    movq(dst, kScratchRegister);
  } else {
    LoadSmiConstant(dst, constant);
    addq(dst, src);
    j(overflow, on_not_smi_result);
  }
}


template <typename LabelType>
void MacroAssembler::SmiSubConstant(Register dst,
                                    Register src,
                                    Smi* constant,
                                    LabelType* on_not_smi_result) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    if (constant->value() == Smi::kMinValue) {
      // Subtracting min-value from any non-negative value will overflow.
      // We test the non-negativeness before doing the subtraction.
      testq(src, src);
      j(not_sign, on_not_smi_result);
      LoadSmiConstant(kScratchRegister, constant);
      subq(dst, kScratchRegister);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(kScratchRegister, Smi::FromInt(-constant->value()));
      addq(kScratchRegister, dst);
      j(overflow, on_not_smi_result);
      movq(dst, kScratchRegister);
    }
  } else {
    if (constant->value() == Smi::kMinValue) {
      // Subtracting min-value from any non-negative value will overflow.
      // We test the non-negativeness before doing the subtraction.
      testq(src, src);
      j(not_sign, on_not_smi_result);
      LoadSmiConstant(dst, constant);
      // Adding and subtracting the min-value gives the same result, it only
      // differs on the overflow bit, which we don't check here.
      addq(dst, src);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(dst, Smi::FromInt(-(constant->value())));
      addq(dst, src);
      j(overflow, on_not_smi_result);
    }
  }
}


template <typename LabelType>
void MacroAssembler::SmiDiv(Register dst,
                            Register src1,
                            Register src2,
                            LabelType* on_not_smi_result) {
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src2.is(rax));
  ASSERT(!src2.is(rdx));
  ASSERT(!src1.is(rdx));

  // Check for 0 divisor (result is +/-Infinity).
  NearLabel positive_divisor;
  testq(src2, src2);
  j(zero, on_not_smi_result);

  if (src1.is(rax)) {
    movq(kScratchRegister, src1);
  }
  SmiToInteger32(rax, src1);
  // We need to rule out dividing Smi::kMinValue by -1, since that would
  // overflow in idiv and raise an exception.
  // We combine this with negative zero test (negative zero only happens
  // when dividing zero by a negative number).

  // We overshoot a little and go to slow case if we divide min-value
  // by any negative value, not just -1.
  NearLabel safe_div;
  testl(rax, Immediate(0x7fffffff));
  j(not_zero, &safe_div);
  testq(src2, src2);
  if (src1.is(rax)) {
    j(positive, &safe_div);
    movq(src1, kScratchRegister);
    jmp(on_not_smi_result);
  } else {
    j(negative, on_not_smi_result);
  }
  bind(&safe_div);

  SmiToInteger32(src2, src2);
  // Sign extend src1 into edx:eax.
  cdq();
  idivl(src2);
  Integer32ToSmi(src2, src2);
  // Check that the remainder is zero.
  testl(rdx, rdx);
  if (src1.is(rax)) {
    NearLabel smi_result;
    j(zero, &smi_result);
    movq(src1, kScratchRegister);
    jmp(on_not_smi_result);
    bind(&smi_result);
  } else {
    j(not_zero, on_not_smi_result);
  }
  if (!dst.is(src1) && src1.is(rax)) {
    movq(src1, kScratchRegister);
  }
  Integer32ToSmi(dst, rax);
}


template <typename LabelType>
void MacroAssembler::SmiMod(Register dst,
                            Register src1,
                            Register src2,
                            LabelType* on_not_smi_result) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!src2.is(rax));
  ASSERT(!src2.is(rdx));
  ASSERT(!src1.is(rdx));
  ASSERT(!src1.is(src2));

  testq(src2, src2);
  j(zero, on_not_smi_result);

  if (src1.is(rax)) {
    movq(kScratchRegister, src1);
  }
  SmiToInteger32(rax, src1);
  SmiToInteger32(src2, src2);

  // Test for the edge case of dividing Smi::kMinValue by -1 (will overflow).
  NearLabel safe_div;
  cmpl(rax, Immediate(Smi::kMinValue));
  j(not_equal, &safe_div);
  cmpl(src2, Immediate(-1));
  j(not_equal, &safe_div);
  // Retag inputs and go slow case.
  Integer32ToSmi(src2, src2);
  if (src1.is(rax)) {
    movq(src1, kScratchRegister);
  }
  jmp(on_not_smi_result);
  bind(&safe_div);

  // Sign extend eax into edx:eax.
  cdq();
  idivl(src2);
  // Restore smi tags on inputs.
  Integer32ToSmi(src2, src2);
  if (src1.is(rax)) {
    movq(src1, kScratchRegister);
  }
  // Check for a negative zero result.  If the result is zero, and the
  // dividend is negative, go slow to return a floating point negative zero.
  NearLabel smi_result;
  testl(rdx, rdx);
  j(not_zero, &smi_result);
  testq(src1, src1);
  j(negative, on_not_smi_result);
  bind(&smi_result);
  Integer32ToSmi(dst, rdx);
}


template <typename LabelType>
void MacroAssembler::SmiShiftLogicalRightConstant(
    Register dst, Register src, int shift_value, LabelType* on_not_smi_result) {
  // Logic right shift interprets its result as an *unsigned* number.
  if (dst.is(src)) {
    UNIMPLEMENTED();  // Not used.
  } else {
    movq(dst, src);
    if (shift_value == 0) {
      testq(dst, dst);
      j(negative, on_not_smi_result);
    }
    shr(dst, Immediate(shift_value + kSmiShift));
    shl(dst, Immediate(kSmiShift));
  }
}


template <typename LabelType>
void MacroAssembler::SmiShiftLogicalRight(Register dst,
                                          Register src1,
                                          Register src2,
                                          LabelType* on_not_smi_result) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(rcx));
  NearLabel result_ok;
  if (src1.is(rcx) || src2.is(rcx)) {
    movq(kScratchRegister, rcx);
  }
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  SmiToInteger32(rcx, src2);
  orl(rcx, Immediate(kSmiShift));
  shr_cl(dst);  // Shift is rcx modulo 0x1f + 32.
  shl(dst, Immediate(kSmiShift));
  testq(dst, dst);
  if (src1.is(rcx) || src2.is(rcx)) {
    NearLabel positive_result;
    j(positive, &positive_result);
    if (src1.is(rcx)) {
      movq(src1, kScratchRegister);
    } else {
      movq(src2, kScratchRegister);
    }
    jmp(on_not_smi_result);
    bind(&positive_result);
  } else {
    j(negative, on_not_smi_result);  // src2 was zero and src1 negative.
  }
}


template <typename LabelType>
void MacroAssembler::SelectNonSmi(Register dst,
                                  Register src1,
                                  Register src2,
                                  LabelType* on_not_smis) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(src1));
  ASSERT(!dst.is(src2));
  // Both operands must not be smis.
#ifdef DEBUG
  if (allow_stub_calls()) {  // Check contains a stub call.
    Condition not_both_smis = NegateCondition(CheckBothSmi(src1, src2));
    Check(not_both_smis, "Both registers were smis in SelectNonSmi.");
  }
#endif
  ASSERT_EQ(0, kSmiTag);
  ASSERT_EQ(0, Smi::FromInt(0));
  movl(kScratchRegister, Immediate(kSmiTagMask));
  and_(kScratchRegister, src1);
  testl(kScratchRegister, src2);
  // If non-zero then both are smis.
  j(not_zero, on_not_smis);

  // Exactly one operand is a smi.
  ASSERT_EQ(1, static_cast<int>(kSmiTagMask));
  // kScratchRegister still holds src1 & kSmiTag, which is either zero or one.
  subq(kScratchRegister, Immediate(1));
  // If src1 is a smi, then scratch register all 1s, else it is all 0s.
  movq(dst, src1);
  xor_(dst, src2);
  and_(dst, kScratchRegister);
  // If src1 is a smi, dst holds src1 ^ src2, else it is zero.
  xor_(dst, src1);
  // If src1 is a smi, dst is src2, else it is src1, i.e., the non-smi.
}


template <typename LabelType>
void MacroAssembler::JumpIfSmi(Register src, LabelType* on_smi) {
  ASSERT_EQ(0, kSmiTag);
  Condition smi = CheckSmi(src);
  j(smi, on_smi);
}


template <typename LabelType>
void MacroAssembler::JumpIfNotSmi(Register src, LabelType* on_not_smi) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi);
}


template <typename LabelType>
void MacroAssembler::JumpUnlessNonNegativeSmi(
    Register src, LabelType* on_not_smi_or_negative) {
  Condition non_negative_smi = CheckNonNegativeSmi(src);
  j(NegateCondition(non_negative_smi), on_not_smi_or_negative);
}


template <typename LabelType>
void MacroAssembler::JumpIfSmiEqualsConstant(Register src,
                                             Smi* constant,
                                             LabelType* on_equals) {
  SmiCompare(src, constant);
  j(equal, on_equals);
}


template <typename LabelType>
void MacroAssembler::JumpIfNotValidSmiValue(Register src,
                                            LabelType* on_invalid) {
  Condition is_valid = CheckInteger32ValidSmiValue(src);
  j(NegateCondition(is_valid), on_invalid);
}


template <typename LabelType>
void MacroAssembler::JumpIfUIntNotValidSmiValue(Register src,
                                                LabelType* on_invalid) {
  Condition is_valid = CheckUInteger32ValidSmiValue(src);
  j(NegateCondition(is_valid), on_invalid);
}


template <typename LabelType>
void MacroAssembler::JumpIfNotBothSmi(Register src1,
                                      Register src2,
                                      LabelType* on_not_both_smi) {
  Condition both_smi = CheckBothSmi(src1, src2);
  j(NegateCondition(both_smi), on_not_both_smi);
}


template <typename LabelType>
void MacroAssembler::JumpUnlessBothNonNegativeSmi(Register src1,
                                                  Register src2,
                                                  LabelType* on_not_both_smi) {
  Condition both_smi = CheckBothNonNegativeSmi(src1, src2);
  j(NegateCondition(both_smi), on_not_both_smi);
}


template <typename LabelType>
void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register first_object,
                                                         Register second_object,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         LabelType* on_fail) {
  // Check that both objects are not smis.
  Condition either_smi = CheckEitherSmi(first_object, second_object);
  j(either_smi, on_fail);

  // Load instance type for both strings.
  movq(scratch1, FieldOperand(first_object, HeapObject::kMapOffset));
  movq(scratch2, FieldOperand(second_object, HeapObject::kMapOffset));
  movzxbl(scratch1, FieldOperand(scratch1, Map::kInstanceTypeOffset));
  movzxbl(scratch2, FieldOperand(scratch2, Map::kInstanceTypeOffset));

  // Check that both are flat ascii strings.
  ASSERT(kNotStringTag != 0);
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatAsciiStringTag = ASCII_STRING_TYPE;

  andl(scratch1, Immediate(kFlatAsciiStringMask));
  andl(scratch2, Immediate(kFlatAsciiStringMask));
  // Interleave the bits to check both scratch1 and scratch2 in one test.
  ASSERT_EQ(0, kFlatAsciiStringMask & (kFlatAsciiStringMask << 3));
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmpl(scratch1,
       Immediate(kFlatAsciiStringTag + (kFlatAsciiStringTag << 3)));
  j(not_equal, on_fail);
}


template <typename LabelType>
void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(
    Register instance_type,
    Register scratch,
    LabelType *failure) {
  if (!scratch.is(instance_type)) {
    movl(scratch, instance_type);
  }

  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;

  andl(scratch, Immediate(kFlatAsciiStringMask));
  cmpl(scratch, Immediate(kStringTag | kSeqStringTag | kAsciiStringTag));
  j(not_equal, failure);
}


template <typename LabelType>
void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first_object_instance_type,
    Register second_object_instance_type,
    Register scratch1,
    Register scratch2,
    LabelType* on_fail) {
  // Load instance type for both strings.
  movq(scratch1, first_object_instance_type);
  movq(scratch2, second_object_instance_type);

  // Check that both are flat ascii strings.
  ASSERT(kNotStringTag != 0);
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatAsciiStringTag = ASCII_STRING_TYPE;

  andl(scratch1, Immediate(kFlatAsciiStringMask));
  andl(scratch2, Immediate(kFlatAsciiStringMask));
  // Interleave the bits to check both scratch1 and scratch2 in one test.
  ASSERT_EQ(0, kFlatAsciiStringMask & (kFlatAsciiStringMask << 3));
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmpl(scratch1,
       Immediate(kFlatAsciiStringTag + (kFlatAsciiStringTag << 3)));
  j(not_equal, on_fail);
}


template <typename LabelType>
void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cc,
                                LabelType* branch) {
  if (Serializer::enabled()) {
    // Can't do arithmetic on external references if it might get serialized.
    // The mask isn't really an address.  We load it as an external reference in
    // case the size of the new space is different between the snapshot maker
    // and the running system.
    if (scratch.is(object)) {
      movq(kScratchRegister, ExternalReference::new_space_mask());
      and_(scratch, kScratchRegister);
    } else {
      movq(scratch, ExternalReference::new_space_mask());
      and_(scratch, object);
    }
    movq(kScratchRegister, ExternalReference::new_space_start());
    cmpq(scratch, kScratchRegister);
    j(cc, branch);
  } else {
    ASSERT(is_int32(static_cast<int64_t>(Heap::NewSpaceMask())));
    intptr_t new_space_start =
        reinterpret_cast<intptr_t>(Heap::NewSpaceStart());
    movq(kScratchRegister, -new_space_start, RelocInfo::NONE);
    if (scratch.is(object)) {
      addq(scratch, kScratchRegister);
    } else {
      lea(scratch, Operand(object, kScratchRegister, times_1, 0));
    }
    and_(scratch, Immediate(static_cast<int32_t>(Heap::NewSpaceMask())));
    j(cc, branch);
  }
}


template <typename LabelType>
void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_register,
                                    LabelType* done,
                                    InvokeFlag flag) {
  bool definitely_matches = false;
  NearLabel invoke;
  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      Set(rax, actual.immediate());
      if (expected.immediate() ==
              SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
        // Don't worry about adapting arguments for built-ins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        Set(rbx, expected.immediate());
      }
    }
  } else {
    if (actual.is_immediate()) {
      // Expected is in register, actual is immediate. This is the
      // case when we invoke function values without going through the
      // IC mechanism.
      cmpq(expected.reg(), Immediate(actual.immediate()));
      j(equal, &invoke);
      ASSERT(expected.reg().is(rbx));
      Set(rax, actual.immediate());
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmpq(expected.reg(), actual.reg());
      j(equal, &invoke);
      ASSERT(actual.reg().is(rax));
      ASSERT(expected.reg().is(rbx));
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor =
        Handle<Code>(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
    if (!code_constant.is_null()) {
      movq(rdx, code_constant, RelocInfo::EMBEDDED_OBJECT);
      addq(rdx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    } else if (!code_register.is(rdx)) {
      movq(rdx, code_register);
    }

    if (flag == CALL_FUNCTION) {
      Call(adaptor, RelocInfo::CODE_TARGET);
      jmp(done);
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}


} }  // namespace v8::internal

#endif  // V8_X64_MACRO_ASSEMBLER_X64_H_
