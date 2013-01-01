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

#ifndef V8_X64_MACRO_ASSEMBLER_X64_H_
#define V8_X64_MACRO_ASSEMBLER_X64_H_

#include "assembler.h"
#include "frames.h"
#include "v8globals.h"

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
const Register kScratchRegister = { 10 };      // r10.
const Register kSmiConstantRegister = { 12 };  // r12 (callee save).
const Register kRootRegister = { 13 };         // r13 (callee save).
// Value of smi in kSmiConstantRegister.
const int kSmiConstantRegisterValue = 1;
// Actual value of root register is offset from the root array's start
// to take advantage of negitive 8-bit displacement values.
const int kRootRegisterBias = 128;

// Convenience for platform-independent signatures.
typedef Operand MemOperand;

enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };

bool AreAliased(Register r1, Register r2, Register r3, Register r4);

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
  // The isolate parameter can be NULL if the macro assembler should
  // not use isolate-dependent functionality. In this case, it's the
  // responsibility of the caller to never invoke such function on the
  // macro assembler.
  MacroAssembler(Isolate* isolate, void* buffer, int size);

  // Prevent the use of the RootArray during the lifetime of this
  // scope object.
  class NoRootArrayScope BASE_EMBEDDED {
   public:
    explicit NoRootArrayScope(MacroAssembler* assembler)
        : variable_(&assembler->root_array_available_),
          old_value_(assembler->root_array_available_) {
      assembler->root_array_available_ = false;
    }
    ~NoRootArrayScope() {
      *variable_ = old_value_;
    }
   private:
    bool* variable_;
    bool old_value_;
  };

  // Operand pointing to an external reference.
  // May emit code to set up the scratch register. The operand is
  // only guaranteed to be correct as long as the scratch register
  // isn't changed.
  // If the operand is used more than once, use a scratch register
  // that is guaranteed not to be clobbered.
  Operand ExternalOperand(ExternalReference reference,
                          Register scratch = kScratchRegister);
  // Loads and stores the value of an external reference.
  // Special case code for load and store to take advantage of
  // load_rax/store_rax if possible/necessary.
  // For other operations, just use:
  //   Operand operand = ExternalOperand(extref);
  //   operation(operand, ..);
  void Load(Register destination, ExternalReference source);
  void Store(ExternalReference destination, Register source);
  // Loads the address of the external reference into the destination
  // register.
  void LoadAddress(Register destination, ExternalReference source);
  // Returns the size of the code generated by LoadAddress.
  // Used by CallSize(ExternalReference) to find the size of a call.
  int LoadAddressSize(ExternalReference source);
  // Pushes the address of the external reference onto the stack.
  void PushAddress(ExternalReference source);

  // Operations on roots in the root-array.
  void LoadRoot(Register destination, Heap::RootListIndex index);
  void StoreRoot(Register source, Heap::RootListIndex index);
  // Load a root value where the index (or part of it) is variable.
  // The variable_offset register is added to the fixed_offset value
  // to get the index into the root-array.
  void LoadRootIndexed(Register destination,
                       Register variable_offset,
                       int fixed_offset);
  void CompareRoot(Register with, Heap::RootListIndex index);
  void CompareRoot(const Operand& with, Heap::RootListIndex index);
  void PushRoot(Heap::RootListIndex index);

  // These functions do not arrange the registers in any particular order so
  // they are not useful for calls that can cause a GC.  The caller can
  // exclude up to 3 registers that do not need to be saved and restored.
  void PushCallerSaved(SaveFPRegsMode fp_mode,
                       Register exclusion1 = no_reg,
                       Register exclusion2 = no_reg,
                       Register exclusion3 = no_reg);
  void PopCallerSaved(SaveFPRegsMode fp_mode,
                      Register exclusion1 = no_reg,
                      Register exclusion2 = no_reg,
                      Register exclusion3 = no_reg);

// ---------------------------------------------------------------------------
// GC Support


  enum RememberedSetFinalAction {
    kReturnAtEnd,
    kFallThroughAtEnd
  };

  // Record in the remembered set the fact that we have a pointer to new space
  // at the address pointed to by the addr register.  Only works if addr is not
  // in new space.
  void RememberedSetHelper(Register object,  // Used for debug code.
                           Register addr,
                           Register scratch,
                           SaveFPRegsMode save_fp,
                           RememberedSetFinalAction and_then);

  void CheckPageFlag(Register object,
                     Register scratch,
                     int mask,
                     Condition cc,
                     Label* condition_met,
                     Label::Distance condition_met_distance = Label::kFar);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object,
                           Register scratch,
                           Label* branch,
                           Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, not_equal, branch, distance);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object,
                        Register scratch,
                        Label* branch,
                        Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, equal, branch, distance);
  }

  // Check if an object has the black incremental marking color.  Also uses rcx!
  void JumpIfBlack(Register object,
                   Register scratch0,
                   Register scratch1,
                   Label* on_black,
                   Label::Distance on_black_distance = Label::kFar);

  // Detects conservatively whether an object is data-only, i.e. it does need to
  // be scanned by the garbage collector.
  void JumpIfDataObject(Register value,
                        Register scratch,
                        Label* not_data_object,
                        Label::Distance not_data_object_distance);

  // Checks the color of an object.  If the object is already grey or black
  // then we just fall through, since it is already live.  If it is white and
  // we can determine that it doesn't need to be scanned, then we just mark it
  // black and fall through.  For the rest we jump to the label so the
  // incremental marker can fix its assumptions.
  void EnsureNotWhite(Register object,
                      Register scratch1,
                      Register scratch2,
                      Label* object_is_white_and_not_data,
                      Label::Distance distance);

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldOperand(reg, off).
  void RecordWriteField(
      Register object,
      int offset,
      Register value,
      Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

  // As above, but the offset has the tag presubtracted.  For use with
  // Operand(reg, off).
  void RecordWriteContextSlot(
      Register context,
      int offset,
      Register value,
      Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK) {
    RecordWriteField(context,
                     offset + kHeapObjectTag,
                     value,
                     scratch,
                     save_fp,
                     remembered_set_action,
                     smi_check);
  }

  // Notify the garbage collector that we wrote a pointer into a fixed array.
  // |array| is the array being stored into, |value| is the
  // object being stored.  |index| is the array index represented as a non-smi.
  // All registers are clobbered by the operation RecordWriteArray
  // filters out smis so it does not update the write barrier if the
  // value is a smi.
  void RecordWriteArray(
      Register array,
      Register value,
      Register index,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

  // For page containing |object| mark region covering |address|
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. The address and value registers are clobbered by the
  // operation.  RecordWrite filters out smis so it does not update
  // the write barrier if the value is a smi.
  void RecordWrite(
      Register object,
      Register address,
      Register value,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // ---------------------------------------------------------------------------
  // Debugger Support

  void DebugBreak();
#endif

  // Enter specific kind of exit frame; either in normal or
  // debug mode. Expects the number of arguments in register rax and
  // sets up the number of arguments in register rdi and the pointer
  // to the first argument in register rsi.
  //
  // Allocates arg_stack_space * kPointerSize memory (not GCed) on the stack
  // accessible via StackSpaceOperand.
  void EnterExitFrame(int arg_stack_space = 0, bool save_doubles = false);

  // Enter specific kind of exit frame. Allocates arg_stack_space * kPointerSize
  // memory (not GCed) on the stack accessible via StackSpaceOperand.
  void EnterApiExitFrame(int arg_stack_space);

  // Leave the current exit frame. Expects/provides the return value in
  // register rax:rdx (untouched) and the pointer to the first
  // argument in register rsi.
  void LeaveExitFrame(bool save_doubles = false);

  // Leave the current exit frame. Expects/provides the return value in
  // register rax (untouched).
  void LeaveApiExitFrame();

  // Push and pop the registers that can hold pointers.
  void PushSafepointRegisters() { Pushad(); }
  void PopSafepointRegisters() { Popad(); }
  // Store the value in register src in the safepoint register stack
  // slot for register dst.
  void StoreToSafepointRegisterSlot(Register dst, const Immediate& imm);
  void StoreToSafepointRegisterSlot(Register dst, Register src);
  void LoadFromSafepointRegisterSlot(Register dst, Register src);

  void InitializeRootRegister() {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    movq(kRootRegister, roots_array_start);
    addq(kRootRegister, Immediate(kRootRegisterBias));
  }

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Set up call kind marking in rcx. The method takes rcx as an
  // explicit first parameter to make the code more readable at the
  // call sites.
  void SetCallKind(Register dst, CallKind kind);

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeCode(Register code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  InvokeFlag flag,
                  const CallWrapper& call_wrapper,
                  CallKind call_kind);

  void InvokeCode(Handle<Code> code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  RelocInfo::Mode rmode,
                  InvokeFlag flag,
                  const CallWrapper& call_wrapper,
                  CallKind call_kind);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper,
                      CallKind call_kind);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper,
                      CallKind call_kind);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id,
                     InvokeFlag flag,
                     const CallWrapper& call_wrapper = NullCallWrapper());

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

  // Perform the logical or of two smi values and return a smi value.
  // If either argument is not a smi, jump to on_not_smis and retain
  // the original values of source registers. The destination register
  // may be changed if it's not one of the source registers.
  void SmiOrIfSmis(Register dst,
                   Register src1,
                   Register src2,
                   Label* on_not_smis,
                   Label::Distance near_jump = Label::kFar);


  // Simple comparison of smis.  Both sides must be known smis to use these,
  // otherwise use Cmp.
  void SmiCompare(Register smi1, Register smi2);
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
  Condition CheckSmi(const Operand& src);

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

  // Check whether src is a Smi, and set dst to zero if it is a smi,
  // and to one if it isn't.
  void CheckSmiToIndicator(Register dst, Register src);
  void CheckSmiToIndicator(Register dst, const Operand& src);

  // Test-and-jump functions. Typically combines a check function
  // above with a conditional jump.

  // Jump if the value cannot be represented by a smi.
  void JumpIfNotValidSmiValue(Register src, Label* on_invalid,
                              Label::Distance near_jump = Label::kFar);

  // Jump if the unsigned integer value cannot be represented by a smi.
  void JumpIfUIntNotValidSmiValue(Register src, Label* on_invalid,
                                  Label::Distance near_jump = Label::kFar);

  // Jump to label if the value is a tagged smi.
  void JumpIfSmi(Register src,
                 Label* on_smi,
                 Label::Distance near_jump = Label::kFar);

  // Jump to label if the value is not a tagged smi.
  void JumpIfNotSmi(Register src,
                    Label* on_not_smi,
                    Label::Distance near_jump = Label::kFar);

  // Jump to label if the value is not a non-negative tagged smi.
  void JumpUnlessNonNegativeSmi(Register src,
                                Label* on_not_smi,
                                Label::Distance near_jump = Label::kFar);

  // Jump to label if the value, which must be a tagged smi, has value equal
  // to the constant.
  void JumpIfSmiEqualsConstant(Register src,
                               Smi* constant,
                               Label* on_equals,
                               Label::Distance near_jump = Label::kFar);

  // Jump if either or both register are not smi values.
  void JumpIfNotBothSmi(Register src1,
                        Register src2,
                        Label* on_not_both_smi,
                        Label::Distance near_jump = Label::kFar);

  // Jump if either or both register are not non-negative smi values.
  void JumpUnlessBothNonNegativeSmi(Register src1, Register src2,
                                    Label* on_not_both_smi,
                                    Label::Distance near_jump = Label::kFar);

  // Operations on tagged smi values.

  // Smis represent a subset of integers. The subset is always equivalent to
  // a two's complement interpretation of a fixed number of bits.

  // Optimistically adds an integer constant to a supposed smi.
  // If the src is not a smi, or the result is not a smi, jump to
  // the label.
  void SmiTryAddConstant(Register dst,
                         Register src,
                         Smi* constant,
                         Label* on_not_smi_result,
                         Label::Distance near_jump = Label::kFar);

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  void SmiAddConstant(Register dst, Register src, Smi* constant);

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  void SmiAddConstant(const Operand& dst, Smi* constant);

  // Add an integer constant to a tagged smi, giving a tagged smi as result,
  // or jumping to a label if the result cannot be represented by a smi.
  void SmiAddConstant(Register dst,
                      Register src,
                      Smi* constant,
                      Label* on_not_smi_result,
                      Label::Distance near_jump = Label::kFar);

  // Subtract an integer constant from a tagged smi, giving a tagged smi as
  // result. No testing on the result is done. Sets the N and Z flags
  // based on the value of the resulting integer.
  void SmiSubConstant(Register dst, Register src, Smi* constant);

  // Subtract an integer constant from a tagged smi, giving a tagged smi as
  // result, or jumping to a label if the result cannot be represented by a smi.
  void SmiSubConstant(Register dst,
                      Register src,
                      Smi* constant,
                      Label* on_not_smi_result,
                      Label::Distance near_jump = Label::kFar);

  // Negating a smi can give a negative zero or too large positive value.
  // NOTICE: This operation jumps on success, not failure!
  void SmiNeg(Register dst,
              Register src,
              Label* on_smi_result,
              Label::Distance near_jump = Label::kFar);

  // Adds smi values and return the result as a smi.
  // If dst is src1, then src1 will be destroyed, even if
  // the operation is unsuccessful.
  void SmiAdd(Register dst,
              Register src1,
              Register src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);
  void SmiAdd(Register dst,
              Register src1,
              const Operand& src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

  void SmiAdd(Register dst,
              Register src1,
              Register src2);

  // Subtracts smi values and return the result as a smi.
  // If dst is src1, then src1 will be destroyed, even if
  // the operation is unsuccessful.
  void SmiSub(Register dst,
              Register src1,
              Register src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

  void SmiSub(Register dst,
              Register src1,
              Register src2);

  void SmiSub(Register dst,
              Register src1,
              const Operand& src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

  void SmiSub(Register dst,
              Register src1,
              const Operand& src2);

  // Multiplies smi values and return the result as a smi,
  // if possible.
  // If dst is src1, then src1 will be destroyed, even if
  // the operation is unsuccessful.
  void SmiMul(Register dst,
              Register src1,
              Register src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

  // Divides one smi by another and returns the quotient.
  // Clobbers rax and rdx registers.
  void SmiDiv(Register dst,
              Register src1,
              Register src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

  // Divides one smi by another and returns the remainder.
  // Clobbers rax and rdx registers.
  void SmiMod(Register dst,
              Register src1,
              Register src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

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
  void SmiShiftLogicalRightConstant(Register dst,
                                  Register src,
                                  int shift_value,
                                  Label* on_not_smi_result,
                                  Label::Distance near_jump = Label::kFar);
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
  void SmiShiftLogicalRight(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump = Label::kFar);
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
  void SelectNonSmi(Register dst,
                    Register src1,
                    Register src2,
                    Label* on_not_smis,
                    Label::Distance near_jump = Label::kFar);

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

  // Add the value of a smi in memory to an int32 register.
  // Sets flags as a normal add.
  void AddSmiField(Register dst, const Operand& src);

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

  // If object is a string, its map is loaded into object_map.
  void JumpIfNotString(Register object,
                       Register object_map,
                       Label* not_string,
                       Label::Distance near_jump = Label::kFar);


  void JumpIfNotBothSequentialAsciiStrings(
      Register first_object,
      Register second_object,
      Register scratch1,
      Register scratch2,
      Label* on_not_both_flat_ascii,
      Label::Distance near_jump = Label::kFar);

  // Check whether the instance type represents a flat ASCII string. Jump to the
  // label if not. If the instance type can be scratched specify same register
  // for both instance type and scratch.
  void JumpIfInstanceTypeIsNotSequentialAscii(
      Register instance_type,
      Register scratch,
      Label*on_not_flat_ascii_string,
      Label::Distance near_jump = Label::kFar);

  void JumpIfBothInstanceTypesAreNotSequentialAscii(
      Register first_object_instance_type,
      Register second_object_instance_type,
      Register scratch1,
      Register scratch2,
      Label* on_fail,
      Label::Distance near_jump = Label::kFar);

  // ---------------------------------------------------------------------------
  // Macro instructions.

  // Load a register with a long value as efficiently as possible.
  void Set(Register dst, int64_t x);
  void Set(const Operand& dst, int64_t x);

  // Move if the registers are not identical.
  void Move(Register target, Register source);

  // Support for constant splitting.
  bool IsUnsafeInt(const int x);
  void SafeMove(Register dst, Smi* src);
  void SafePush(Smi* src);

  // Bit-field support.
  void TestBit(const Operand& dst, int bit_index);

  // Handle support
  void Move(Register dst, Handle<Object> source);
  void Move(const Operand& dst, Handle<Object> source);
  void Cmp(Register dst, Handle<Object> source);
  void Cmp(const Operand& dst, Handle<Object> source);
  void Cmp(Register dst, Smi* src);
  void Cmp(const Operand& dst, Smi* src);
  void Push(Handle<Object> source);

  // Load a heap object and handle the case of new-space objects by
  // indirecting via a global cell.
  void LoadHeapObject(Register result, Handle<HeapObject> object);
  void PushHeapObject(Handle<HeapObject> object);

  void LoadObject(Register result, Handle<Object> object) {
    if (object->IsHeapObject()) {
      LoadHeapObject(result, Handle<HeapObject>::cast(object));
    } else {
      Move(result, object);
    }
  }

  // Load a global cell into a register.
  void LoadGlobalCell(Register dst, Handle<JSGlobalPropertyCell> cell);

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
  void Call(Handle<Code> code_object,
            RelocInfo::Mode rmode,
            TypeFeedbackId ast_id = TypeFeedbackId::None());

  // The size of the code generated for different call instructions.
  int CallSize(Address destination, RelocInfo::Mode rmode) {
    return kCallInstructionLength;
  }
  int CallSize(ExternalReference ext);
  int CallSize(Handle<Code> code_object) {
    // Code calls use 32-bit relative addressing.
    return kShortCallInstructionLength;
  }
  int CallSize(Register target) {
    // Opcode: REX_opt FF /2 m64
    return (target.high_bit() != 0) ? 3 : 2;
  }
  int CallSize(const Operand& target) {
    // Opcode: REX_opt FF /2 m64
    return (target.requires_rex() ? 2 : 1) + target.operand_size();
  }

  // Emit call to the code we are currently generating.
  void CallSelf() {
    Handle<Code> self(reinterpret_cast<Code**>(CodeObject().location()));
    Call(self, RelocInfo::CODE_TARGET);
  }

  // Non-x64 instructions.
  // Push/pop all general purpose registers.
  // Does not push rsp/rbp nor any of the assembler's special purpose registers
  // (kScratchRegister, kSmiConstantRegister, kRootRegister).
  void Pushad();
  void Popad();
  // Sets the stack as after performing Popad, without actually loading the
  // registers.
  void Dropad();

  // Compare object type for heap object.
  // Always use unsigned comparisons: above and below, not less and greater.
  // Incoming register is heap_object and outgoing register is map.
  // They may be the same register, and may be kScratchRegister.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  // Always use unsigned comparisons: above and below, not less and greater.
  void CmpInstanceType(Register map, InstanceType type);

  // Check if a map for a JSObject indicates that the object has fast elements.
  // Jump to the specified label if it does not.
  void CheckFastElements(Register map,
                         Label* fail,
                         Label::Distance distance = Label::kFar);

  // Check if a map for a JSObject indicates that the object can have both smi
  // and HeapObject elements.  Jump to the specified label if it does not.
  void CheckFastObjectElements(Register map,
                               Label* fail,
                               Label::Distance distance = Label::kFar);

  // Check if a map for a JSObject indicates that the object has fast smi only
  // elements.  Jump to the specified label if it does not.
  void CheckFastSmiElements(Register map,
                            Label* fail,
                            Label::Distance distance = Label::kFar);

  // Check to see if maybe_number can be stored as a double in
  // FastDoubleElements. If it can, store it at the index specified by index in
  // the FastDoubleElements array elements, otherwise jump to fail.  Note that
  // index must not be smi-tagged.
  void StoreNumberToDoubleElements(Register maybe_number,
                                   Register elements,
                                   Register index,
                                   XMMRegister xmm_scratch,
                                   Label* fail,
                                   int elements_offset = 0);

  // Compare an object's map with the specified map and its transitioned
  // elements maps if mode is ALLOW_ELEMENT_TRANSITION_MAPS. FLAGS are set with
  // result of map compare. If multiple map compares are required, the compare
  // sequences branches to early_success.
  void CompareMap(Register obj,
                  Handle<Map> map,
                  Label* early_success,
                  CompareMapMode mode = REQUIRE_EXACT_MAP);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specified map.
  void CheckMap(Register obj,
                Handle<Map> map,
                Label* fail,
                SmiCheckType smi_check_type,
                CompareMapMode mode = REQUIRE_EXACT_MAP);

  // Check if the map of an object is equal to a specified map and branch to a
  // specified target if equal. Skip the smi check if not required (object is
  // known to be a heap object)
  void DispatchMap(Register obj,
                   Handle<Map> map,
                   Handle<Code> success,
                   SmiCheckType smi_check_type);

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

  void ClampUint8(Register reg);

  void ClampDoubleToUint8(XMMRegister input_reg,
                          XMMRegister temp_xmm_reg,
                          Register result_reg);

  void LoadUint32(XMMRegister dst, Register src, XMMRegister scratch);

  void LoadInstanceDescriptors(Register map, Register descriptors);
  void EnumLength(Register dst, Register map);
  void NumberOfOwnDescriptors(Register dst, Register map);

  template<typename Field>
  void DecodeField(Register reg) {
    static const int shift = Field::kShift + kSmiShift;
    static const int mask = Field::kMask >> Field::kShift;
    shr(reg, Immediate(shift));
    and_(reg, Immediate(mask));
    shl(reg, Immediate(kSmiShift));
  }

  // Abort execution if argument is not a number, enabled via --debug-code.
  void AssertNumber(Register object);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);

  // Abort execution if argument is not a smi, enabled via --debug-code.
  void AssertSmi(Register object);
  void AssertSmi(const Operand& object);

  // Abort execution if a 64 bit register containing a 32 bit payload does not
  // have zeros in the top 32 bits, enabled via --debug-code.
  void AssertZeroExtended(Register reg);

  // Abort execution if argument is not a string, enabled via --debug-code.
  void AssertString(Register object);

  // Abort execution if argument is not the root value with the given index,
  // enabled via --debug-code.
  void AssertRootValue(Register src,
                       Heap::RootListIndex root_value_index,
                       const char* message);

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link it into try handler chain.
  void PushTryHandler(StackHandler::Kind kind, int handler_index);

  // Unlink the stack handler on top of the stack from the try handler chain.
  void PopTryHandler();

  // Activate the top handler in the try hander chain and pass the
  // thrown value.
  void Throw(Register value);

  // Propagate an uncatchable exception out of the current JS stack.
  void ThrowUncatchable(Register value);

  // ---------------------------------------------------------------------------
  // Inline caching support

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, but the scratch register and kScratchRegister,
  // which must be different, are clobbered.
  void CheckAccessGlobalProxy(Register holder_reg,
                              Register scratch,
                              Label* miss);

  void GetNumberHash(Register r0, Register scratch);

  void LoadFromNumberDictionary(Label* miss,
                                Register elements,
                                Register key,
                                Register r0,
                                Register r1,
                                Register r2,
                                Register result);


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
  void AllocateTwoByteConsString(Register result,
                          Register scratch1,
                          Register scratch2,
                          Label* gc_required);
  void AllocateAsciiConsString(Register result,
                               Register scratch1,
                               Register scratch2,
                               Label* gc_required);

  // Allocate a raw sliced string object. Only the map field of the result is
  // initialized.
  void AllocateTwoByteSlicedString(Register result,
                            Register scratch1,
                            Register scratch2,
                            Label* gc_required);
  void AllocateAsciiSlicedString(Register result,
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
                               Label* miss,
                               bool miss_on_bound_function = false);

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

  // Conditionally load the cached Array transitioned map of type
  // transitioned_kind from the native context if the map in register
  // map_in_out is the cached Array map in the native context of
  // expected_kind.
  void LoadTransitionedArrayMapConditional(
      ElementsKind expected_kind,
      ElementsKind transitioned_kind,
      Register map_in_out,
      Register scratch,
      Label* no_map_match);

  // Load the initial map for new Arrays from a JSFunction.
  void LoadInitialArrayMap(Register function_in,
                           Register scratch,
                           Register map_out,
                           bool can_have_holes);

  // Load the global function with the given index.
  void LoadGlobalFunction(int index, Register function);

  // Load the initial map from the global function. The registers
  // function and map can be the same.
  void LoadGlobalFunctionInitialMap(Register function, Register map);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub, TypeFeedbackId ast_id = TypeFeedbackId::None());

  // Tail call a code stub (jump).
  void TailCallStub(CodeStub* stub);

  // Return from a code stub after popping its arguments.
  void StubReturn(int argc);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f, int num_arguments);

  // Call a runtime function and save the value of XMM registers.
  void CallRuntimeSaveDoubles(Runtime::FunctionId id);

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId id, int num_arguments);

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

  // Prepares stack to put arguments (aligns and so on).  WIN64 calling
  // convention requires to put the pointer to the return value slot into
  // rcx (rcx must be preserverd until CallApiFunctionAndReturn).  Saves
  // context (rsi).  Clobbers rax.  Allocates arg_stack_space * kPointerSize
  // inside the exit frame (not GCed) accessible via StackSpaceOperand.
  void PrepareCallApiFunction(int arg_stack_space);

  // Calls an API function.  Allocates HandleScope, extracts returned value
  // from handle and propagates exceptions.  Clobbers r14, r15, rbx and
  // caller-save registers.  Restores context.  On return removes
  // stack_space * kPointerSize (GCed).
  void CallApiFunctionAndReturn(Address function_address, int stack_space);

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

  // Return and drop arguments from stack, where the number of arguments
  // may be bigger than 2^16 - 1.  Requires a scratch register.
  void Ret(int bytes_dropped, Register scratch);

  Handle<Object> CodeObject() {
    ASSERT(!code_object_.is_null());
    return code_object_;
  }

  // Copy length bytes from source to destination.
  // Uses scratch register internally (if you have a low-eight register
  // free, do use it, otherwise kScratchRegister will be used).
  // The min_length is a minimum limit on the value that length will have.
  // The algorithm has some special cases that might be omitted if the string
  // is known to always be long.
  void CopyBytes(Register destination,
                 Register source,
                 Register length,
                 int min_length = 0,
                 Register scratch = kScratchRegister);

  // Initialize fields with filler values.  Fields starting at |start_offset|
  // not including end_offset are overwritten with the value in |filler|.  At
  // the end the loop, |start_offset| takes the value of |end_offset|.
  void InitializeFieldsWithFiller(Register start_offset,
                                  Register end_offset,
                                  Register filler);


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
  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() { return has_frame_; }
  inline bool AllowThisStubCall(CodeStub* stub);

  static int SafepointRegisterStackIndex(Register reg) {
    return SafepointRegisterStackIndex(reg.code());
  }

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void LeaveFrame(StackFrame::Type type);

  // Expects object in rax and returns map with validated enum cache
  // in rax.  Assumes that any other register can be used as a scratch.
  void CheckEnumCache(Register null_value,
                      Label* call_runtime);

 private:
  // Order general registers are pushed by Pushad.
  // rax, rcx, rdx, rbx, rsi, rdi, r8, r9, r11, r14, r15.
  static const int kSafepointPushRegisterIndices[Register::kNumRegisters];
  static const int kNumSafepointSavedRegisters = 11;
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  bool generating_stub_;
  bool allow_stub_calls_;
  bool has_frame_;
  bool root_array_available_;

  // Returns a register holding the smi value. The register MUST NOT be
  // modified. It may be the "smi 1 constant" register.
  Register GetSmiConstant(Smi* value);

  intptr_t RootRegisterDelta(ExternalReference other);

  // Moves the smi value to the destination register.
  void LoadSmiConstant(Register dst, Smi* value);

  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Handle<Code> code_constant,
                      Register code_register,
                      Label* done,
                      bool* definitely_mismatches,
                      InvokeFlag flag,
                      Label::Distance near_jump = Label::kFar,
                      const CallWrapper& call_wrapper = NullCallWrapper(),
                      CallKind call_kind = CALL_AS_METHOD);

  void EnterExitFramePrologue(bool save_rax);

  // Allocates arg_stack_space * kPointerSize memory (not GCed) on the stack
  // accessible via StackSpaceOperand.
  void EnterExitFrameEpilogue(int arg_stack_space, bool save_doubles);

  void LeaveExitFrameEpilogue();

  // Allocation support helpers.
  // Loads the top of new-space into the result register.
  // Otherwise the address of the new-space top is loaded into scratch (if
  // scratch is valid), and the new-space top is loaded into result.
  void LoadAllocationTopHelper(Register result,
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

  // Helper for implementing JumpIfNotInNewSpace and JumpIfInNewSpace.
  void InNewSpace(Register object,
                  Register scratch,
                  Condition cc,
                  Label* branch,
                  Label::Distance distance = Label::kFar);

  // Helper for finding the mark bits for an address.  Afterwards, the
  // bitmap register points at the word with the mark bits and the mask
  // the position of the first bit.  Uses rcx as scratch and leaves addr_reg
  // unchanged.
  inline void GetMarkBits(Register addr_reg,
                          Register bitmap_reg,
                          Register mask_reg);

  // Helper for throwing exceptions.  Compute a handler address and jump to
  // it.  See the implementation for register usage.
  void JumpToHandlerEntry();

  // Compute memory operands for safepoint stack slots.
  Operand SafepointRegisterSlot(Register reg);
  static int SafepointRegisterStackIndex(int reg_code) {
    return kNumSafepointRegisters - kSafepointPushRegisterIndices[reg_code] - 1;
  }

  // Needs access to SafepointRegisterStackIndex for optimized frame
  // traversal.
  friend class OptimizedFrame;
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
inline Operand FieldOperand(Register object, int offset) {
  return Operand(object, offset - kHeapObjectTag);
}


// Generate an Operand for loading an indexed field from an object.
inline Operand FieldOperand(Register object,
                            Register index,
                            ScaleFactor scale,
                            int offset) {
  return Operand(object, index, scale, offset - kHeapObjectTag);
}


inline Operand ContextOperand(Register context, int index) {
  return Operand(context, Context::SlotOffset(index));
}


inline Operand GlobalObjectOperand() {
  return ContextOperand(rsi, Context::GLOBAL_OBJECT_INDEX);
}


// Provides access to exit frame stack space (not GCed).
inline Operand StackSpaceOperand(int index) {
#ifdef _WIN64
  const int kShaddowSpace = 4;
  return Operand(rsp, (index + kShaddowSpace) * kPointerSize);
#else
  return Operand(rsp, index * kPointerSize);
#endif
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

} }  // namespace v8::internal

#endif  // V8_X64_MACRO_ASSEMBLER_X64_H_
