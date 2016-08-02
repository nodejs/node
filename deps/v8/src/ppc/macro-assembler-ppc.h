// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PPC_MACRO_ASSEMBLER_PPC_H_
#define V8_PPC_MACRO_ASSEMBLER_PPC_H_

#include "src/assembler.h"
#include "src/bailout-reason.h"
#include "src/frames.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Give alias names to registers for calling conventions.
const Register kReturnRegister0 = {Register::kCode_r3};
const Register kReturnRegister1 = {Register::kCode_r4};
const Register kReturnRegister2 = {Register::kCode_r5};
const Register kJSFunctionRegister = {Register::kCode_r4};
const Register kContextRegister = {Register::kCode_r30};
const Register kAllocateSizeRegister = {Register::kCode_r4};
const Register kInterpreterAccumulatorRegister = {Register::kCode_r3};
const Register kInterpreterBytecodeOffsetRegister = {Register::kCode_r15};
const Register kInterpreterBytecodeArrayRegister = {Register::kCode_r16};
const Register kInterpreterDispatchTableRegister = {Register::kCode_r17};
const Register kJavaScriptCallArgCountRegister = {Register::kCode_r3};
const Register kJavaScriptCallNewTargetRegister = {Register::kCode_r6};
const Register kRuntimeCallFunctionRegister = {Register::kCode_r4};
const Register kRuntimeCallArgCountRegister = {Register::kCode_r3};

// ----------------------------------------------------------------------------
// Static helper functions

// Generate a MemOperand for loading a field from an object.
inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}


// Flags used for AllocateHeapNumber
enum TaggingMode {
  // Tag the result.
  TAG_RESULT,
  // Don't tag
  DONT_TAG_RESULT
};


enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum PointersToHereCheck {
  kPointersToHereMaybeInteresting,
  kPointersToHereAreAlwaysInteresting
};
enum LinkRegisterStatus { kLRHasNotBeenSaved, kLRHasBeenSaved };


Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2 = no_reg,
                                   Register reg3 = no_reg,
                                   Register reg4 = no_reg,
                                   Register reg5 = no_reg,
                                   Register reg6 = no_reg);


#ifdef DEBUG
bool AreAliased(Register reg1, Register reg2, Register reg3 = no_reg,
                Register reg4 = no_reg, Register reg5 = no_reg,
                Register reg6 = no_reg, Register reg7 = no_reg,
                Register reg8 = no_reg, Register reg9 = no_reg,
                Register reg10 = no_reg);
#endif

// These exist to provide portability between 32 and 64bit
#if V8_TARGET_ARCH_PPC64
#define LoadPX ldx
#define LoadPUX ldux
#define StorePX stdx
#define StorePUX stdux
#define ShiftLeftImm sldi
#define ShiftRightImm srdi
#define ClearLeftImm clrldi
#define ClearRightImm clrrdi
#define ShiftRightArithImm sradi
#define ShiftLeft_ sld
#define ShiftRight_ srd
#define ShiftRightArith srad
#define Mul mulld
#define Div divd
#else
#define LoadPX lwzx
#define LoadPUX lwzux
#define StorePX stwx
#define StorePUX stwux
#define ShiftLeftImm slwi
#define ShiftRightImm srwi
#define ClearLeftImm clrlwi
#define ClearRightImm clrrwi
#define ShiftRightArithImm srawi
#define ShiftLeft_ slw
#define ShiftRight_ srw
#define ShiftRightArith sraw
#define Mul mullw
#define Div divw
#endif


// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler : public Assembler {
 public:
  MacroAssembler(Isolate* isolate, void* buffer, int size,
                 CodeObjectRequired create_code_object);


  // Returns the size of a call in instructions. Note, the value returned is
  // only valid as long as no entries are added to the constant pool between
  // checking the call size and emitting the actual call.
  static int CallSize(Register target);
  int CallSize(Address target, RelocInfo::Mode rmode, Condition cond = al);
  static int CallSizeNotPredictableCodeSize(Address target,
                                            RelocInfo::Mode rmode,
                                            Condition cond = al);

  // Jump, Call, and Ret pseudo instructions implementing inter-working.
  void Jump(Register target);
  void JumpToJSEntry(Register target);
  void Jump(Address target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Register target);
  void CallJSEntry(Register target);
  void Call(Address target, RelocInfo::Mode rmode, Condition cond = al);
  int CallSize(Handle<Code> code,
               RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
               TypeFeedbackId ast_id = TypeFeedbackId::None(),
               Condition cond = al);
  void Call(Handle<Code> code, RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            TypeFeedbackId ast_id = TypeFeedbackId::None(),
            Condition cond = al);
  void Ret() { blr(); }
  void Ret(Condition cond, CRegister cr = cr7) { bclr(cond, cr); }

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count);
  void Drop(Register count, Register scratch = r0);

  void Ret(int drop) {
    Drop(drop);
    blr();
  }

  void Call(Label* target);

  // Register move. May do nothing if the registers are identical.
  void Move(Register dst, Smi* smi) { LoadSmiLiteral(dst, smi); }
  void Move(Register dst, Handle<Object> value);
  void Move(Register dst, Register src, Condition cond = al);
  void Move(DoubleRegister dst, DoubleRegister src);

  void MultiPush(RegList regs, Register location = sp);
  void MultiPop(RegList regs, Register location = sp);

  void MultiPushDoubles(RegList dregs, Register location = sp);
  void MultiPopDoubles(RegList dregs, Register location = sp);

  // Load an object from the root table.
  void LoadRoot(Register destination, Heap::RootListIndex index,
                Condition cond = al);
  // Store an object to the root table.
  void StoreRoot(Register source, Heap::RootListIndex index,
                 Condition cond = al);

  // ---------------------------------------------------------------------------
  // GC Support

  void IncrementalMarkingRecordWriteHelper(Register object, Register value,
                                           Register address);

  enum RememberedSetFinalAction { kReturnAtEnd, kFallThroughAtEnd };

  // Record in the remembered set the fact that we have a pointer to new space
  // at the address pointed to by the addr register.  Only works if addr is not
  // in new space.
  void RememberedSetHelper(Register object,  // Used for debug code.
                           Register addr, Register scratch,
                           SaveFPRegsMode save_fp,
                           RememberedSetFinalAction and_then);

  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object, Register scratch, Label* branch) {
    InNewSpace(object, scratch, eq, branch);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object, Register scratch, Label* branch) {
    InNewSpace(object, scratch, ne, branch);
  }

  // Check if an object has a given incremental marking color.
  void HasColor(Register object, Register scratch0, Register scratch1,
                Label* has_color, int first_bit, int second_bit);

  void JumpIfBlack(Register object, Register scratch0, Register scratch1,
                   Label* on_black);

  // Checks the color of an object.  If the object is white we jump to the
  // incremental marker.
  void JumpIfWhite(Register value, Register scratch1, Register scratch2,
                   Register scratch3, Label* value_is_white);

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldMemOperand(reg, off).
  void RecordWriteField(
      Register object, int offset, Register value, Register scratch,
      LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // As above, but the offset has the tag presubtracted.  For use with
  // MemOperand(reg, off).
  inline void RecordWriteContextSlot(
      Register context, int offset, Register value, Register scratch,
      LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting) {
    RecordWriteField(context, offset + kHeapObjectTag, value, scratch,
                     lr_status, save_fp, remembered_set_action, smi_check,
                     pointers_to_here_check_for_value);
  }

  // Notify the garbage collector that we wrote a code entry into a
  // JSFunction. Only scratch is clobbered by the operation.
  void RecordWriteCodeEntryField(Register js_function, Register code_entry,
                                 Register scratch);

  void RecordWriteForMap(Register object, Register map, Register dst,
                         LinkRegisterStatus lr_status, SaveFPRegsMode save_fp);

  // For a given |object| notify the garbage collector that the slot |address|
  // has been written.  |value| is the object being stored. The value and
  // address registers are clobbered by the operation.
  void RecordWrite(
      Register object, Register address, Register value,
      LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  void Push(Register src) { push(src); }

  // Push a handle.
  void Push(Handle<Object> handle);
  void Push(Smi* smi) { Push(Handle<Smi>(smi, isolate())); }

  // Push two registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2) {
    StorePU(src2, MemOperand(sp, -2 * kPointerSize));
    StoreP(src1, MemOperand(sp, kPointerSize));
  }

  // Push three registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3) {
    StorePU(src3, MemOperand(sp, -3 * kPointerSize));
    StoreP(src2, MemOperand(sp, kPointerSize));
    StoreP(src1, MemOperand(sp, 2 * kPointerSize));
  }

  // Push four registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Register src4) {
    StorePU(src4, MemOperand(sp, -4 * kPointerSize));
    StoreP(src3, MemOperand(sp, kPointerSize));
    StoreP(src2, MemOperand(sp, 2 * kPointerSize));
    StoreP(src1, MemOperand(sp, 3 * kPointerSize));
  }

  // Push five registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Register src4,
            Register src5) {
    StorePU(src5, MemOperand(sp, -5 * kPointerSize));
    StoreP(src4, MemOperand(sp, kPointerSize));
    StoreP(src3, MemOperand(sp, 2 * kPointerSize));
    StoreP(src2, MemOperand(sp, 3 * kPointerSize));
    StoreP(src1, MemOperand(sp, 4 * kPointerSize));
  }

  void Pop(Register dst) { pop(dst); }

  // Pop two registers. Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2) {
    LoadP(src2, MemOperand(sp, 0));
    LoadP(src1, MemOperand(sp, kPointerSize));
    addi(sp, sp, Operand(2 * kPointerSize));
  }

  // Pop three registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3) {
    LoadP(src3, MemOperand(sp, 0));
    LoadP(src2, MemOperand(sp, kPointerSize));
    LoadP(src1, MemOperand(sp, 2 * kPointerSize));
    addi(sp, sp, Operand(3 * kPointerSize));
  }

  // Pop four registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3, Register src4) {
    LoadP(src4, MemOperand(sp, 0));
    LoadP(src3, MemOperand(sp, kPointerSize));
    LoadP(src2, MemOperand(sp, 2 * kPointerSize));
    LoadP(src1, MemOperand(sp, 3 * kPointerSize));
    addi(sp, sp, Operand(4 * kPointerSize));
  }

  // Pop five registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3, Register src4,
           Register src5) {
    LoadP(src5, MemOperand(sp, 0));
    LoadP(src4, MemOperand(sp, kPointerSize));
    LoadP(src3, MemOperand(sp, 2 * kPointerSize));
    LoadP(src2, MemOperand(sp, 3 * kPointerSize));
    LoadP(src1, MemOperand(sp, 4 * kPointerSize));
    addi(sp, sp, Operand(5 * kPointerSize));
  }

  // Push a fixed frame, consisting of lr, fp, constant pool.
  void PushCommonFrame(Register marker_reg = no_reg);

  // Push a standard frame, consisting of lr, fp, constant pool,
  // context and JS function
  void PushStandardFrame(Register function_reg);

  void PopCommonFrame(Register marker_reg = no_reg);

  // Restore caller's frame pointer and return address prior to being
  // overwritten by tail call stack preparation.
  void RestoreFrameStateForTailCall();

  // Push and pop the registers that can hold pointers, as defined by the
  // RegList constant kSafepointSavedRegisters.
  void PushSafepointRegisters();
  void PopSafepointRegisters();
  // Store value in register src in the safepoint stack slot for
  // register dst.
  void StoreToSafepointRegisterSlot(Register src, Register dst);
  // Load the value of the src register from its safepoint stack slot
  // into register dst.
  void LoadFromSafepointRegisterSlot(Register dst, Register src);

  // Flush the I-cache from asm code. You should use CpuFeatures::FlushICache
  // from C.
  // Does not handle errors.
  void FlushICache(Register address, size_t size, Register scratch);

  // If the value is a NaN, canonicalize the value else, do nothing.
  void CanonicalizeNaN(const DoubleRegister dst, const DoubleRegister src);
  void CanonicalizeNaN(const DoubleRegister value) {
    CanonicalizeNaN(value, value);
  }

  // Converts the integer (untagged smi) in |src| to a double, storing
  // the result to |dst|
  void ConvertIntToDouble(Register src, DoubleRegister dst);

  // Converts the unsigned integer (untagged smi) in |src| to
  // a double, storing the result to |dst|
  void ConvertUnsignedIntToDouble(Register src, DoubleRegister dst);

  // Converts the integer (untagged smi) in |src| to
  // a float, storing the result in |dst|
  void ConvertIntToFloat(Register src, DoubleRegister dst);

  // Converts the unsigned integer (untagged smi) in |src| to
  // a float, storing the result in |dst|
  void ConvertUnsignedIntToFloat(Register src, DoubleRegister dst);

#if V8_TARGET_ARCH_PPC64
  void ConvertInt64ToFloat(Register src, DoubleRegister double_dst);
  void ConvertInt64ToDouble(Register src, DoubleRegister double_dst);
  void ConvertUnsignedInt64ToFloat(Register src, DoubleRegister double_dst);
  void ConvertUnsignedInt64ToDouble(Register src, DoubleRegister double_dst);
#endif

  // Converts the double_input to an integer.  Note that, upon return,
  // the contents of double_dst will also hold the fixed point representation.
  void ConvertDoubleToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_PPC64
                            const Register dst_hi,
#endif
                            const Register dst, const DoubleRegister double_dst,
                            FPRoundingMode rounding_mode = kRoundToZero);

#if V8_TARGET_ARCH_PPC64
  // Converts the double_input to an unsigned integer.  Note that, upon return,
  // the contents of double_dst will also hold the fixed point representation.
  void ConvertDoubleToUnsignedInt64(
      const DoubleRegister double_input, const Register dst,
      const DoubleRegister double_dst,
      FPRoundingMode rounding_mode = kRoundToZero);
#endif

#if !V8_TARGET_ARCH_PPC64
  void ShiftLeftPair(Register dst_low, Register dst_high, Register src_low,
                     Register src_high, Register scratch, Register shift);
  void ShiftLeftPair(Register dst_low, Register dst_high, Register src_low,
                     Register src_high, uint32_t shift);
  void ShiftRightPair(Register dst_low, Register dst_high, Register src_low,
                      Register src_high, Register scratch, Register shift);
  void ShiftRightPair(Register dst_low, Register dst_high, Register src_low,
                      Register src_high, uint32_t shift);
  void ShiftRightAlgPair(Register dst_low, Register dst_high, Register src_low,
                         Register src_high, Register scratch, Register shift);
  void ShiftRightAlgPair(Register dst_low, Register dst_high, Register src_low,
                         Register src_high, uint32_t shift);
#endif

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type, Register base = no_reg,
                    int prologue_offset = 0);
  void Prologue(bool code_pre_aging, Register base, int prologue_offset = 0);

  // Enter exit frame.
  // stack_space - extra stack space, used for parameters before call to C.
  // At least one slot (for the return address) should be provided.
  void EnterExitFrame(bool save_doubles, int stack_space = 1);

  // Leave the current exit frame. Expects the return value in r0.
  // Expect the number of values, pushed prior to the exit frame, to
  // remove in a register (or no_reg, if there is nothing to remove).
  void LeaveExitFrame(bool save_doubles, Register argument_count,
                      bool restore_context,
                      bool argument_count_is_length = false);

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();

  void LoadContext(Register dst, int context_chain_length);

  // Load the global object from the current context.
  void LoadGlobalObject(Register dst) {
    LoadNativeContextSlot(Context::EXTENSION_INDEX, dst);
  }

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst) {
    LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
  }

  // Conditionally load the cached Array transitioned map of type
  // transitioned_kind from the native context if the map in register
  // map_in_out is the cached Array map in the native context of
  // expected_kind.
  void LoadTransitionedArrayMapConditional(ElementsKind expected_kind,
                                           ElementsKind transitioned_kind,
                                           Register map_in_out,
                                           Register scratch,
                                           Label* no_map_match);

  void LoadNativeContextSlot(int index, Register dst);

  // Load the initial map from the global function. The registers
  // function and map can be the same, function is then overwritten.
  void LoadGlobalFunctionInitialMap(Register function, Register map,
                                    Register scratch);

  void InitializeRootRegister() {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    mov(kRootRegister, Operand(roots_array_start));
  }

  // ----------------------------------------------------------------
  // new PPC macro-assembler interfaces that are slightly higher level
  // than assembler-ppc and may generate variable length sequences

  // load a literal signed int value <value> to GPR <dst>
  void LoadIntLiteral(Register dst, int value);

  // load an SMI value <value> to GPR <dst>
  void LoadSmiLiteral(Register dst, Smi* smi);

  // load a literal double value <value> to FPR <result>
  void LoadDoubleLiteral(DoubleRegister result, double value, Register scratch);

  void LoadWord(Register dst, const MemOperand& mem, Register scratch);
  void LoadWordArith(Register dst, const MemOperand& mem,
                     Register scratch = no_reg);
  void StoreWord(Register src, const MemOperand& mem, Register scratch);

  void LoadHalfWord(Register dst, const MemOperand& mem, Register scratch);
  void LoadHalfWordArith(Register dst, const MemOperand& mem,
                         Register scratch = no_reg);
  void StoreHalfWord(Register src, const MemOperand& mem, Register scratch);

  void LoadByte(Register dst, const MemOperand& mem, Register scratch);
  void StoreByte(Register src, const MemOperand& mem, Register scratch);

  void LoadRepresentation(Register dst, const MemOperand& mem, Representation r,
                          Register scratch = no_reg);
  void StoreRepresentation(Register src, const MemOperand& mem,
                           Representation r, Register scratch = no_reg);

  void LoadDouble(DoubleRegister dst, const MemOperand& mem,
                  Register scratch = no_reg);
  void LoadDoubleU(DoubleRegister dst, const MemOperand& mem,
                  Register scratch = no_reg);

  void LoadSingle(DoubleRegister dst, const MemOperand& mem,
                  Register scratch = no_reg);
  void LoadSingleU(DoubleRegister dst, const MemOperand& mem,
                   Register scratch = no_reg);

  void StoreDouble(DoubleRegister src, const MemOperand& mem,
                   Register scratch = no_reg);
  void StoreDoubleU(DoubleRegister src, const MemOperand& mem,
                   Register scratch = no_reg);

  void StoreSingle(DoubleRegister src, const MemOperand& mem,
                   Register scratch = no_reg);
  void StoreSingleU(DoubleRegister src, const MemOperand& mem,
                    Register scratch = no_reg);

  // Move values between integer and floating point registers.
  void MovIntToDouble(DoubleRegister dst, Register src, Register scratch);
  void MovUnsignedIntToDouble(DoubleRegister dst, Register src,
                              Register scratch);
  void MovInt64ToDouble(DoubleRegister dst,
#if !V8_TARGET_ARCH_PPC64
                        Register src_hi,
#endif
                        Register src);
#if V8_TARGET_ARCH_PPC64
  void MovInt64ComponentsToDouble(DoubleRegister dst, Register src_hi,
                                  Register src_lo, Register scratch);
#endif
  void InsertDoubleLow(DoubleRegister dst, Register src, Register scratch);
  void InsertDoubleHigh(DoubleRegister dst, Register src, Register scratch);
  void MovDoubleLowToInt(Register dst, DoubleRegister src);
  void MovDoubleHighToInt(Register dst, DoubleRegister src);
  void MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
      Register dst_hi,
#endif
      Register dst, DoubleRegister src);
  void MovIntToFloat(DoubleRegister dst, Register src);
  void MovFloatToInt(Register dst, DoubleRegister src);

  void Add(Register dst, Register src, intptr_t value, Register scratch);
  void Cmpi(Register src1, const Operand& src2, Register scratch,
            CRegister cr = cr7);
  void Cmpli(Register src1, const Operand& src2, Register scratch,
             CRegister cr = cr7);
  void Cmpwi(Register src1, const Operand& src2, Register scratch,
             CRegister cr = cr7);
  void Cmplwi(Register src1, const Operand& src2, Register scratch,
              CRegister cr = cr7);
  void And(Register ra, Register rs, const Operand& rb, RCBit rc = LeaveRC);
  void Or(Register ra, Register rs, const Operand& rb, RCBit rc = LeaveRC);
  void Xor(Register ra, Register rs, const Operand& rb, RCBit rc = LeaveRC);

  void AddSmiLiteral(Register dst, Register src, Smi* smi, Register scratch);
  void SubSmiLiteral(Register dst, Register src, Smi* smi, Register scratch);
  void CmpSmiLiteral(Register src1, Smi* smi, Register scratch,
                     CRegister cr = cr7);
  void CmplSmiLiteral(Register src1, Smi* smi, Register scratch,
                      CRegister cr = cr7);
  void AndSmiLiteral(Register dst, Register src, Smi* smi, Register scratch,
                     RCBit rc = LeaveRC);

  // Set new rounding mode RN to FPSCR
  void SetRoundingMode(FPRoundingMode RN);

  // reset rounding mode to default (kRoundToNearest)
  void ResetRoundingMode();

  // These exist to provide portability between 32 and 64bit
  void LoadP(Register dst, const MemOperand& mem, Register scratch = no_reg);
  void LoadPU(Register dst, const MemOperand& mem, Register scratch = no_reg);
  void StoreP(Register src, const MemOperand& mem, Register scratch = no_reg);
  void StorePU(Register src, const MemOperand& mem, Register scratch = no_reg);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // Both |callee_args_count| and |caller_args_count_reg| do not include
  // receiver. |callee_args_count| is not modified, |caller_args_count_reg|
  // is trashed.
  void PrepareForTailCall(const ParameterCount& callee_args_count,
                          Register caller_args_count_reg, Register scratch0,
                          Register scratch1);

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeFunctionCode(Register function, Register new_target,
                          const ParameterCount& expected,
                          const ParameterCount& actual, InvokeFlag flag,
                          const CallWrapper& call_wrapper);

  void FloodFunctionIfStepping(Register fun, Register new_target,
                               const ParameterCount& expected,
                               const ParameterCount& actual);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function, Register new_target,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Register function, const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void IsObjectJSStringType(Register object, Register scratch, Label* fail);

  void IsObjectNameType(Register object, Register scratch, Label* fail);

  // ---------------------------------------------------------------------------
  // Debugger Support

  void DebugBreak();

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new stack handler and link into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  // Must preserve the result register.
  void PopStackHandler();

  // ---------------------------------------------------------------------------
  // Inline caching support

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, whereas both scratch registers are clobbered.
  void CheckAccessGlobalProxy(Register holder_reg, Register scratch,
                              Label* miss);

  void GetNumberHash(Register t0, Register scratch);

  void LoadFromNumberDictionary(Label* miss, Register elements, Register key,
                                Register result, Register t0, Register t1,
                                Register t2);


  inline void MarkCode(NopMarkerTypes type) { nop(type); }

  // Check if the given instruction is a 'type' marker.
  // i.e. check if is is a mov r<type>, r<type> (referenced as nop(type))
  // These instructions are generated to mark special location in the code,
  // like some special IC code.
  static inline bool IsMarkedCode(Instr instr, int type) {
    DCHECK((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER));
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
                       (dst_reg == src_reg) && (FIRST_IC_MARKER <= dst_reg) &&
                       (dst_reg < LAST_CODE_MARKER)
                   ? src_reg
                   : -1;
    DCHECK((type == -1) ||
           ((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER)));
    return type;
  }


  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space or old space. The object_size is
  // specified either in bytes or in words if the allocation flag SIZE_IN_WORDS
  // is passed. If the space is exhausted control continues at the gc_required
  // label. The allocated object is returned in result. If the flag
  // tag_allocated_object is true the result is tagged as as a heap object.
  // All registers are clobbered also when control continues at the gc_required
  // label.
  void Allocate(int object_size, Register result, Register scratch1,
                Register scratch2, Label* gc_required, AllocationFlags flags);

  void Allocate(Register object_size, Register result, Register result_end,
                Register scratch, Label* gc_required, AllocationFlags flags);

  // FastAllocate is right now only used for folded allocations. It just
  // increments the top pointer without checking against limit. This can only
  // be done if it was proved earlier that the allocation will succeed.
  void FastAllocate(int object_size, Register result, Register scratch1,
                    Register scratch2, AllocationFlags flags);

  void FastAllocate(Register object_size, Register result, Register result_end,
                    Register scratch, AllocationFlags flags);

  void AllocateTwoByteString(Register result, Register length,
                             Register scratch1, Register scratch2,
                             Register scratch3, Label* gc_required);
  void AllocateOneByteString(Register result, Register length,
                             Register scratch1, Register scratch2,
                             Register scratch3, Label* gc_required);
  void AllocateTwoByteConsString(Register result, Register length,
                                 Register scratch1, Register scratch2,
                                 Label* gc_required);
  void AllocateOneByteConsString(Register result, Register length,
                                 Register scratch1, Register scratch2,
                                 Label* gc_required);
  void AllocateTwoByteSlicedString(Register result, Register length,
                                   Register scratch1, Register scratch2,
                                   Label* gc_required);
  void AllocateOneByteSlicedString(Register result, Register length,
                                   Register scratch1, Register scratch2,
                                   Label* gc_required);

  // Allocates a heap number or jumps to the gc_required label if the young
  // space is full and a scavenge is needed. All registers are clobbered also
  // when control continues at the gc_required label.
  void AllocateHeapNumber(Register result, Register scratch1, Register scratch2,
                          Register heap_number_map, Label* gc_required,
                          MutableMode mode = IMMUTABLE);
  void AllocateHeapNumberWithValue(Register result, DoubleRegister value,
                                   Register scratch1, Register scratch2,
                                   Register heap_number_map,
                                   Label* gc_required);

  // Allocate and initialize a JSValue wrapper with the specified {constructor}
  // and {value}.
  void AllocateJSValue(Register result, Register constructor, Register value,
                       Register scratch1, Register scratch2,
                       Label* gc_required);

  // Copies a number of bytes from src to dst. All registers are clobbered. On
  // exit src and dst will point to the place just after where the last byte was
  // read or written and length will be zero.
  void CopyBytes(Register src, Register dst, Register length, Register scratch);

  // Initialize fields with filler values.  |count| fields starting at
  // |current_address| are overwritten with the value in |filler|.  At the end
  // the loop, |current_address| points at the next uninitialized field.
  // |count| is assumed to be non-zero.
  void InitializeNFieldsWithFiller(Register current_address, Register count,
                                   Register filler);

  // Initialize fields with filler values.  Fields starting at |current_address|
  // not including |end_address| are overwritten with the value in |filler|.  At
  // the end the loop, |current_address| takes the value of |end_address|.
  void InitializeFieldsWithFiller(Register current_address,
                                  Register end_address, Register filler);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Machine code version of Map::GetConstructor().
  // |temp| holds |result|'s map when done, and |temp2| its instance type.
  void GetMapConstructor(Register result, Register map, Register temp,
                         Register temp2);

  // Try to get function prototype of a function and puts the value in
  // the result register. Checks that the function really is a
  // function and jumps to the miss label if the fast checks fail. The
  // function register will be untouched; the other registers may be
  // clobbered.
  void TryGetFunctionPrototype(Register function, Register result,
                               Register scratch, Label* miss);

  // Compare object type for heap object.  heap_object contains a non-Smi
  // whose object type should be compared with the given type.  This both
  // sets the flags and leaves the object type in the type_reg register.
  // It leaves the map in the map register (unless the type_reg and map register
  // are the same register).  It leaves the heap object in the heap_object
  // register unless the heap_object register is the same register as one of the
  // other registers.
  // Type_reg can be no_reg. In that case ip is used.
  void CompareObjectType(Register heap_object, Register map, Register type_reg,
                         InstanceType type);

  // Compare instance type in a map.  map contains a valid map object whose
  // object type should be compared with the given type.  This both
  // sets the flags and leaves the object type in the type_reg register.
  void CompareInstanceType(Register map, Register type_reg, InstanceType type);


  // Check if a map for a JSObject indicates that the object has fast elements.
  // Jump to the specified label if it does not.
  void CheckFastElements(Register map, Register scratch, Label* fail);

  // Check if a map for a JSObject indicates that the object can have both smi
  // and HeapObject elements.  Jump to the specified label if it does not.
  void CheckFastObjectElements(Register map, Register scratch, Label* fail);

  // Check if a map for a JSObject indicates that the object has fast smi only
  // elements.  Jump to the specified label if it does not.
  void CheckFastSmiElements(Register map, Register scratch, Label* fail);

  // Check to see if maybe_number can be stored as a double in
  // FastDoubleElements. If it can, store it at the index specified by key in
  // the FastDoubleElements array elements. Otherwise jump to fail.
  void StoreNumberToDoubleElements(Register value_reg, Register key_reg,
                                   Register elements_reg, Register scratch1,
                                   DoubleRegister double_scratch, Label* fail,
                                   int elements_offset = 0);

  // Compare an object's map with the specified map and its transitioned
  // elements maps if mode is ALLOW_ELEMENT_TRANSITION_MAPS. Condition flags are
  // set with result of map compare. If multiple map compares are required, the
  // compare sequences branches to early_success.
  void CompareMap(Register obj, Register scratch, Handle<Map> map,
                  Label* early_success);

  // As above, but the map of the object is already loaded into the register
  // which is preserved by the code generated.
  void CompareMap(Register obj_map, Handle<Map> map, Label* early_success);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specified map.
  void CheckMap(Register obj, Register scratch, Handle<Map> map, Label* fail,
                SmiCheckType smi_check_type);


  void CheckMap(Register obj, Register scratch, Heap::RootListIndex index,
                Label* fail, SmiCheckType smi_check_type);


  // Check if the map of an object is equal to a specified weak map and branch
  // to a specified target if equal. Skip the smi check if not required
  // (object is known to be a heap object)
  void DispatchWeakMap(Register obj, Register scratch1, Register scratch2,
                       Handle<WeakCell> cell, Handle<Code> success,
                       SmiCheckType smi_check_type);

  // Compare the given value and the value of weak cell.
  void CmpWeakValue(Register value, Handle<WeakCell> cell, Register scratch,
                    CRegister cr = cr7);

  void GetWeakValue(Register value, Handle<WeakCell> cell);

  // Load the value of the weak cell in the value register. Branch to the given
  // miss label if the weak cell was cleared.
  void LoadWeakValue(Register value, Handle<WeakCell> cell, Label* miss);

  // Compare the object in a register to a value from the root list.
  // Uses the ip register as scratch.
  void CompareRoot(Register obj, Heap::RootListIndex index);
  void PushRoot(Heap::RootListIndex index) {
    LoadRoot(r0, index);
    Push(r0);
  }

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(Register with, Heap::RootListIndex index, Label* if_equal) {
    CompareRoot(with, index);
    beq(if_equal);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(Register with, Heap::RootListIndex index,
                     Label* if_not_equal) {
    CompareRoot(with, index);
    bne(if_not_equal);
  }

  // Load and check the instance type of an object for being a string.
  // Loads the type into the second argument register.
  // Returns a condition that will be enabled if the object was a string.
  Condition IsObjectStringType(Register obj, Register type) {
    LoadP(type, FieldMemOperand(obj, HeapObject::kMapOffset));
    lbz(type, FieldMemOperand(type, Map::kInstanceTypeOffset));
    andi(r0, type, Operand(kIsNotStringMask));
    DCHECK_EQ(0u, kStringTag);
    return eq;
  }


  // Picks out an array index from the hash field.
  // Register use:
  //   hash - holds the index's hash. Clobbered.
  //   index - holds the overwritten index on exit.
  void IndexFromHash(Register hash, Register index);

  // Get the number of least significant bits from a register
  void GetLeastBitsFromSmi(Register dst, Register src, int num_least_bits);
  void GetLeastBitsFromInt32(Register dst, Register src, int mun_least_bits);

  // Load the value of a smi object into a double register.
  void SmiToDouble(DoubleRegister value, Register smi);

  // Check if a double can be exactly represented as a signed 32-bit integer.
  // CR_EQ in cr7 is set if true.
  void TestDoubleIsInt32(DoubleRegister double_input, Register scratch1,
                         Register scratch2, DoubleRegister double_scratch);

  // Check if a double is equal to -0.0.
  // CR_EQ in cr7 holds the result.
  void TestDoubleIsMinusZero(DoubleRegister input, Register scratch1,
                             Register scratch2);

  // Check the sign of a double.
  // CR_LT in cr7 holds the result.
  void TestDoubleSign(DoubleRegister input, Register scratch);
  void TestHeapNumberSign(Register input, Register scratch);

  // Try to convert a double to a signed 32-bit integer.
  // CR_EQ in cr7 is set and result assigned if the conversion is exact.
  void TryDoubleToInt32Exact(Register result, DoubleRegister double_input,
                             Register scratch, DoubleRegister double_scratch);

  // Floor a double and writes the value to the result register.
  // Go to exact if the conversion is exact (to be able to test -0),
  // fall through calling code if an overflow occurred, else go to done.
  // In return, input_high is loaded with high bits of input.
  void TryInt32Floor(Register result, DoubleRegister double_input,
                     Register input_high, Register scratch,
                     DoubleRegister double_scratch, Label* done, Label* exact);

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. Goes to 'done' if it
  // succeeds, otherwise falls through if result is saturated. On return
  // 'result' either holds answer, or is clobbered on fall through.
  //
  // Only public for the test code in test-code-stubs-arm.cc.
  void TryInlineTruncateDoubleToI(Register result, DoubleRegister input,
                                  Label* done);

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32.
  // Exits with 'result' holding the answer.
  void TruncateDoubleToI(Register result, DoubleRegister double_input);

  // Performs a truncating conversion of a heap number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. 'result' and 'input'
  // must be different registers.  Exits with 'result' holding the answer.
  void TruncateHeapNumberToI(Register result, Register object);

  // Converts the smi or heap number in object to an int32 using the rules
  // for ToInt32 as described in ECMAScript 9.5.: the value is truncated
  // and brought into the range -2^31 .. +2^31 - 1. 'result' and 'input' must be
  // different registers.
  void TruncateNumberToI(Register object, Register result,
                         Register heap_number_map, Register scratch1,
                         Label* not_int32);

  // Overflow handling functions.
  // Usage: call the appropriate arithmetic function and then call one of the
  // flow control functions with the corresponding label.

  // Compute dst = left + right, setting condition codes. dst may be same as
  // either left or right (or a unique register). left and right must not be
  // the same register.
  void AddAndCheckForOverflow(Register dst, Register left, Register right,
                              Register overflow_dst, Register scratch = r0);
  void AddAndCheckForOverflow(Register dst, Register left, intptr_t right,
                              Register overflow_dst, Register scratch = r0);

  // Compute dst = left - right, setting condition codes. dst may be same as
  // either left or right (or a unique register). left and right must not be
  // the same register.
  void SubAndCheckForOverflow(Register dst, Register left, Register right,
                              Register overflow_dst, Register scratch = r0);

  void BranchOnOverflow(Label* label) { blt(label, cr0); }

  void BranchOnNoOverflow(Label* label) { bge(label, cr0); }

  void RetOnOverflow(void) { Ret(lt, cr0); }

  void RetOnNoOverflow(void) { Ret(ge, cr0); }

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub, TypeFeedbackId ast_id = TypeFeedbackId::None(),
                Condition cond = al);

  // Call a code stub.
  void TailCallStub(CodeStub* stub, Condition cond = al);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f, int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);
  void CallRuntimeSaveDoubles(Runtime::FunctionId fid) {
    const Runtime::Function* function = Runtime::FunctionForId(fid);
    CallRuntime(function, function->nargs, kSaveFPRegs);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    const Runtime::Function* function = Runtime::FunctionForId(fid);
    CallRuntime(function, function->nargs, save_doubles);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid, int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    CallRuntime(Runtime::FunctionForId(fid), num_arguments, save_doubles);
  }

  // Convenience function: call an external reference.
  void CallExternalReference(const ExternalReference& ext, int num_arguments);

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid);

  int CalculateStackPassedWords(int num_reg_arguments,
                                int num_double_arguments);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, non-register arguments must be stored in
  // sp[0], sp[4], etc., not pushed. The argument count assumes all arguments
  // are word sized. If double arguments are used, this function assumes that
  // all double arguments are stored before core registers; otherwise the
  // correct alignment of the double values is not guaranteed.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  void PrepareCallCFunction(int num_reg_arguments, int num_double_registers,
                            Register scratch);
  void PrepareCallCFunction(int num_reg_arguments, Register scratch);

  // There are two ways of passing double arguments on ARM, depending on
  // whether soft or hard floating point ABI is used. These functions
  // abstract parameter passing for the three different ways we call
  // C functions from generated code.
  void MovToFloatParameter(DoubleRegister src);
  void MovToFloatParameters(DoubleRegister src1, DoubleRegister src2);
  void MovToFloatResult(DoubleRegister src);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);
  void CallCFunction(ExternalReference function, int num_reg_arguments,
                     int num_double_arguments);
  void CallCFunction(Register function, int num_reg_arguments,
                     int num_double_arguments);

  void MovFromFloatParameter(DoubleRegister dst);
  void MovFromFloatResult(DoubleRegister dst);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& builtin);

  Handle<Object> CodeObject() {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }


  // Emit code for a truncating division by a constant. The dividend register is
  // unchanged and ip gets clobbered. Dividend and result must be different.
  void TruncatingDiv(Register result, Register dividend, int32_t divisor);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value, Register scratch1,
                  Register scratch2);
  void IncrementCounter(StatsCounter* counter, int value, Register scratch1,
                        Register scratch2);
  void DecrementCounter(StatsCounter* counter, int value, Register scratch1,
                        Register scratch2);


  // ---------------------------------------------------------------------------
  // Debugging

  // Calls Abort(msg) if the condition cond is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cond, BailoutReason reason, CRegister cr = cr7);
  void AssertFastElements(Register elements);

  // Like Assert(), but always enabled.
  void Check(Condition cond, BailoutReason reason, CRegister cr = cr7);

  // Print a message to stdout and abort execution.
  void Abort(BailoutReason reason);

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() { return has_frame_; }
  inline bool AllowThisStubCall(CodeStub* stub);

  // ---------------------------------------------------------------------------
  // Number utilities

  // Check whether the value of reg is a power of two and not zero. If not
  // control continues at the label not_power_of_two. If reg is a power of two
  // the register scratch contains the value of (reg - 1) when control falls
  // through.
  void JumpIfNotPowerOfTwoOrZero(Register reg, Register scratch,
                                 Label* not_power_of_two_or_zero);
  // Check whether the value of reg is a power of two and not zero.
  // Control falls through if it is, with scratch containing the mask
  // value (reg - 1).
  // Otherwise control jumps to the 'zero_and_neg' label if the value of reg is
  // zero or negative, or jumps to the 'not_power_of_two' label if the value is
  // strictly positive but not a power of two.
  void JumpIfNotPowerOfTwoOrZeroAndNeg(Register reg, Register scratch,
                                       Label* zero_and_neg,
                                       Label* not_power_of_two);

  // ---------------------------------------------------------------------------
  // Bit testing/extraction
  //
  // Bit numbering is such that the least significant bit is bit 0
  // (for consistency between 32/64-bit).

  // Extract consecutive bits (defined by rangeStart - rangeEnd) from src
  // and, if !test, shift them into the least significant bits of dst.
  inline void ExtractBitRange(Register dst, Register src, int rangeStart,
                              int rangeEnd, RCBit rc = LeaveRC,
                              bool test = false) {
    DCHECK(rangeStart >= rangeEnd && rangeStart < kBitsPerPointer);
    int rotate = (rangeEnd == 0) ? 0 : kBitsPerPointer - rangeEnd;
    int width = rangeStart - rangeEnd + 1;
    if (rc == SetRC && rangeStart < 16 && (rangeEnd == 0 || test)) {
      // Prefer faster andi when applicable.
      andi(dst, src, Operand(((1 << width) - 1) << rangeEnd));
    } else {
#if V8_TARGET_ARCH_PPC64
      rldicl(dst, src, rotate, kBitsPerPointer - width, rc);
#else
      rlwinm(dst, src, rotate, kBitsPerPointer - width, kBitsPerPointer - 1,
             rc);
#endif
    }
  }

  inline void ExtractBit(Register dst, Register src, uint32_t bitNumber,
                         RCBit rc = LeaveRC, bool test = false) {
    ExtractBitRange(dst, src, bitNumber, bitNumber, rc, test);
  }

  // Extract consecutive bits (defined by mask) from src and place them
  // into the least significant bits of dst.
  inline void ExtractBitMask(Register dst, Register src, uintptr_t mask,
                             RCBit rc = LeaveRC, bool test = false) {
    int start = kBitsPerPointer - 1;
    int end;
    uintptr_t bit = (1L << start);

    while (bit && (mask & bit) == 0) {
      start--;
      bit >>= 1;
    }
    end = start;
    bit >>= 1;

    while (bit && (mask & bit)) {
      end--;
      bit >>= 1;
    }

    // 1-bits in mask must be contiguous
    DCHECK(bit == 0 || (mask & ((bit << 1) - 1)) == 0);

    ExtractBitRange(dst, src, start, end, rc, test);
  }

  // Test single bit in value.
  inline void TestBit(Register value, int bitNumber, Register scratch = r0) {
    ExtractBitRange(scratch, value, bitNumber, bitNumber, SetRC, true);
  }

  // Test consecutive bit range in value.  Range is defined by
  // rangeStart - rangeEnd.
  inline void TestBitRange(Register value, int rangeStart, int rangeEnd,
                           Register scratch = r0) {
    ExtractBitRange(scratch, value, rangeStart, rangeEnd, SetRC, true);
  }

  // Test consecutive bit range in value.  Range is defined by mask.
  inline void TestBitMask(Register value, uintptr_t mask,
                          Register scratch = r0) {
    ExtractBitMask(scratch, value, mask, SetRC, true);
  }


  // ---------------------------------------------------------------------------
  // Smi utilities

  // Shift left by kSmiShift
  void SmiTag(Register reg, RCBit rc = LeaveRC) { SmiTag(reg, reg, rc); }
  void SmiTag(Register dst, Register src, RCBit rc = LeaveRC) {
    ShiftLeftImm(dst, src, Operand(kSmiShift), rc);
  }

#if !V8_TARGET_ARCH_PPC64
  // Test for overflow < 0: use BranchOnOverflow() or BranchOnNoOverflow().
  void SmiTagCheckOverflow(Register reg, Register overflow);
  void SmiTagCheckOverflow(Register dst, Register src, Register overflow);

  inline void JumpIfNotSmiCandidate(Register value, Register scratch,
                                    Label* not_smi_label) {
    // High bits must be identical to fit into an Smi
    STATIC_ASSERT(kSmiShift == 1);
    addis(scratch, value, Operand(0x40000000u >> 16));
    cmpi(scratch, Operand::Zero());
    blt(not_smi_label);
  }
#endif
  inline void TestUnsignedSmiCandidate(Register value, Register scratch) {
    // The test is different for unsigned int values. Since we need
    // the value to be in the range of a positive smi, we can't
    // handle any of the high bits being set in the value.
    TestBitRange(value, kBitsPerPointer - 1, kBitsPerPointer - 1 - kSmiShift,
                 scratch);
  }
  inline void JumpIfNotUnsignedSmiCandidate(Register value, Register scratch,
                                            Label* not_smi_label) {
    TestUnsignedSmiCandidate(value, scratch);
    bne(not_smi_label, cr0);
  }

  void SmiUntag(Register reg, RCBit rc = LeaveRC) { SmiUntag(reg, reg, rc); }

  void SmiUntag(Register dst, Register src, RCBit rc = LeaveRC) {
    ShiftRightArithImm(dst, src, kSmiShift, rc);
  }

  void SmiToPtrArrayOffset(Register dst, Register src) {
#if V8_TARGET_ARCH_PPC64
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift > kPointerSizeLog2);
    ShiftRightArithImm(dst, src, kSmiShift - kPointerSizeLog2);
#else
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift < kPointerSizeLog2);
    ShiftLeftImm(dst, src, Operand(kPointerSizeLog2 - kSmiShift));
#endif
  }

  void SmiToByteArrayOffset(Register dst, Register src) { SmiUntag(dst, src); }

  void SmiToShortArrayOffset(Register dst, Register src) {
#if V8_TARGET_ARCH_PPC64
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift > 1);
    ShiftRightArithImm(dst, src, kSmiShift - 1);
#else
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift == 1);
    if (!dst.is(src)) {
      mr(dst, src);
    }
#endif
  }

  void SmiToIntArrayOffset(Register dst, Register src) {
#if V8_TARGET_ARCH_PPC64
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift > 2);
    ShiftRightArithImm(dst, src, kSmiShift - 2);
#else
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift < 2);
    ShiftLeftImm(dst, src, Operand(2 - kSmiShift));
#endif
  }

#define SmiToFloatArrayOffset SmiToIntArrayOffset

  void SmiToDoubleArrayOffset(Register dst, Register src) {
#if V8_TARGET_ARCH_PPC64
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift > kDoubleSizeLog2);
    ShiftRightArithImm(dst, src, kSmiShift - kDoubleSizeLog2);
#else
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift < kDoubleSizeLog2);
    ShiftLeftImm(dst, src, Operand(kDoubleSizeLog2 - kSmiShift));
#endif
  }

  void SmiToArrayOffset(Register dst, Register src, int elementSizeLog2) {
    if (kSmiShift < elementSizeLog2) {
      ShiftLeftImm(dst, src, Operand(elementSizeLog2 - kSmiShift));
    } else if (kSmiShift > elementSizeLog2) {
      ShiftRightArithImm(dst, src, kSmiShift - elementSizeLog2);
    } else if (!dst.is(src)) {
      mr(dst, src);
    }
  }

  void IndexToArrayOffset(Register dst, Register src, int elementSizeLog2,
                          bool isSmi) {
    if (isSmi) {
      SmiToArrayOffset(dst, src, elementSizeLog2);
    } else {
      ShiftLeftImm(dst, src, Operand(elementSizeLog2));
    }
  }

  // Untag the source value into destination and jump if source is a smi.
  // Souce and destination can be the same register.
  void UntagAndJumpIfSmi(Register dst, Register src, Label* smi_case);

  // Untag the source value into destination and jump if source is not a smi.
  // Souce and destination can be the same register.
  void UntagAndJumpIfNotSmi(Register dst, Register src, Label* non_smi_case);

  inline void TestIfSmi(Register value, Register scratch) {
    TestBitRange(value, kSmiTagSize - 1, 0, scratch);
  }

  inline void TestIfPositiveSmi(Register value, Register scratch) {
#if V8_TARGET_ARCH_PPC64
    rldicl(scratch, value, 1, kBitsPerPointer - (1 + kSmiTagSize), SetRC);
#else
    rlwinm(scratch, value, 1, kBitsPerPointer - (1 + kSmiTagSize),
           kBitsPerPointer - 1, SetRC);
#endif
  }

  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label) {
    TestIfSmi(value, r0);
    beq(smi_label, cr0);  // branch if SMI
  }
  // Jump if either of the registers contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label) {
    TestIfSmi(value, r0);
    bne(not_smi_label, cr0);
  }
  // Jump if either of the registers contain a non-smi.
  void JumpIfNotBothSmi(Register reg1, Register reg2, Label* on_not_both_smi);
  // Jump if either of the registers contain a smi.
  void JumpIfEitherSmi(Register reg1, Register reg2, Label* on_either_smi);

  // Abort execution if argument is a number, enabled via --debug-code.
  void AssertNotNumber(Register object);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);
  void AssertSmi(Register object);


#if V8_TARGET_ARCH_PPC64
  inline void TestIfInt32(Register value, Register scratch,
                          CRegister cr = cr7) {
    // High bits must be identical to fit into an 32-bit integer
    extsw(scratch, value);
    cmp(scratch, value, cr);
  }
#else
  inline void TestIfInt32(Register hi_word, Register lo_word, Register scratch,
                          CRegister cr = cr7) {
    // High bits must be identical to fit into an 32-bit integer
    srawi(scratch, lo_word, 31);
    cmp(scratch, hi_word, cr);
  }
#endif

#if V8_TARGET_ARCH_PPC64
  // Ensure it is permissable to read/write int value directly from
  // upper half of the smi.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 32);
#endif
#if V8_TARGET_ARCH_PPC64 && V8_TARGET_LITTLE_ENDIAN
#define SmiWordOffset(offset) (offset + kPointerSize / 2)
#else
#define SmiWordOffset(offset) offset
#endif

  // Abort execution if argument is not a string, enabled via --debug-code.
  void AssertString(Register object);

  // Abort execution if argument is not a name, enabled via --debug-code.
  void AssertName(Register object);

  void AssertFunction(Register object);

  // Abort execution if argument is not a JSBoundFunction,
  // enabled via --debug-code.
  void AssertBoundFunction(Register object);

  // Abort execution if argument is not a JSGeneratorObject,
  // enabled via --debug-code.
  void AssertGeneratorObject(Register object);

  // Abort execution if argument is not a JSReceiver, enabled via --debug-code.
  void AssertReceiver(Register object);

  // Abort execution if argument is not undefined or an AllocationSite, enabled
  // via --debug-code.
  void AssertUndefinedOrAllocationSite(Register object, Register scratch);

  // Abort execution if reg is not the root value with the given index,
  // enabled via --debug-code.
  void AssertIsRoot(Register reg, Heap::RootListIndex index);

  // ---------------------------------------------------------------------------
  // HeapNumber utilities

  void JumpIfNotHeapNumber(Register object, Register heap_number_map,
                           Register scratch, Label* on_not_heap_number);

  // ---------------------------------------------------------------------------
  // String utilities

  // Checks if both objects are sequential one-byte strings and jumps to label
  // if either is not. Assumes that neither object is a smi.
  void JumpIfNonSmisNotBothSequentialOneByteStrings(Register object1,
                                                    Register object2,
                                                    Register scratch1,
                                                    Register scratch2,
                                                    Label* failure);

  // Checks if both objects are sequential one-byte strings and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialOneByteStrings(Register first, Register second,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* not_flat_one_byte_strings);

  // Checks if both instance types are sequential one-byte strings and jumps to
  // label if either is not.
  void JumpIfBothInstanceTypesAreNotSequentialOneByte(
      Register first_object_instance_type, Register second_object_instance_type,
      Register scratch1, Register scratch2, Label* failure);

  // Check if instance type is sequential one-byte string and jump to label if
  // it is not.
  void JumpIfInstanceTypeIsNotSequentialOneByte(Register type, Register scratch,
                                                Label* failure);

  void JumpIfNotUniqueNameInstanceType(Register reg, Label* not_unique_name);

  void EmitSeqStringSetCharCheck(Register string, Register index,
                                 Register value, uint32_t encoding_mask);

  // ---------------------------------------------------------------------------
  // Patching helpers.

  // Decode offset from constant pool load instruction(s).
  // Caller must place the instruction word at <location> in <result>.
  void DecodeConstantPoolOffset(Register result, Register location);

  void ClampUint8(Register output_reg, Register input_reg);

  // Saturate a value into 8-bit unsigned integer
  //   if input_value < 0, output_value is 0
  //   if input_value > 255, output_value is 255
  //   otherwise output_value is the (int)input_value (round to nearest)
  void ClampDoubleToUint8(Register result_reg, DoubleRegister input_reg,
                          DoubleRegister temp_double_reg);


  void LoadInstanceDescriptors(Register map, Register descriptors);
  void EnumLength(Register dst, Register map);
  void NumberOfOwnDescriptors(Register dst, Register map);
  void LoadAccessor(Register dst, Register holder, int accessor_index,
                    AccessorComponent accessor);

  template <typename Field>
  void DecodeField(Register dst, Register src, RCBit rc = LeaveRC) {
    ExtractBitRange(dst, src, Field::kShift + Field::kSize - 1, Field::kShift,
                    rc);
  }

  template <typename Field>
  void DecodeField(Register reg, RCBit rc = LeaveRC) {
    DecodeField<Field>(reg, reg, rc);
  }

  template <typename Field>
  void DecodeFieldToSmi(Register dst, Register src) {
#if V8_TARGET_ARCH_PPC64
    DecodeField<Field>(dst, src);
    SmiTag(dst);
#else
    // 32-bit can do this in one instruction:
    int start = Field::kSize + kSmiShift - 1;
    int end = kSmiShift;
    int rotate = kSmiShift - Field::kShift;
    if (rotate < 0) {
      rotate += kBitsPerPointer;
    }
    rlwinm(dst, src, rotate, kBitsPerPointer - start - 1,
           kBitsPerPointer - end - 1);
#endif
  }

  template <typename Field>
  void DecodeFieldToSmi(Register reg) {
    DecodeFieldToSmi<Field>(reg, reg);
  }

  // Load the type feedback vector from a JavaScript frame.
  void EmitLoadTypeFeedbackVector(Register vector);

  // Activation support.
  void EnterFrame(StackFrame::Type type,
                  bool load_constant_pool_pointer_reg = false);
  // Returns the pc offset at which the frame ends.
  int LeaveFrame(StackFrame::Type type, int stack_adjustment = 0);

  // Expects object in r3 and returns map with validated enum cache
  // in r3.  Assumes that any other register can be used as a scratch.
  void CheckEnumCache(Label* call_runtime);

  // AllocationMemento support. Arrays may have an associated
  // AllocationMemento object that can be checked for in order to pretransition
  // to another type.
  // On entry, receiver_reg should point to the array object.
  // scratch_reg gets clobbered.
  // If allocation info is present, condition flags are set to eq.
  void TestJSArrayForAllocationMemento(Register receiver_reg,
                                       Register scratch_reg,
                                       Register scratch2_reg,
                                       Label* no_memento_found);

  void JumpIfJSArrayHasAllocationMemento(Register receiver_reg,
                                         Register scratch_reg,
                                         Register scratch2_reg,
                                         Label* memento_found) {
    Label no_memento_found;
    TestJSArrayForAllocationMemento(receiver_reg, scratch_reg, scratch2_reg,
                                    &no_memento_found);
    beq(memento_found);
    bind(&no_memento_found);
  }

  // Jumps to found label if a prototype map has dictionary elements.
  void JumpIfDictionaryInPrototypeChain(Register object, Register scratch0,
                                        Register scratch1, Label* found);

  // Loads the constant pool pointer (kConstantPoolRegister).
  void LoadConstantPoolPointerRegisterFromCodeTargetAddress(
      Register code_target_address);
  void LoadConstantPoolPointerRegister();
  void LoadConstantPoolPointerRegister(Register base, int code_entry_delta = 0);

  void AbortConstantPoolBuilding() {
#ifdef DEBUG
    // Avoid DCHECK(!is_linked()) failure in ~Label()
    bind(ConstantPoolPosition());
#endif
  }

 private:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  void CallCFunctionHelper(Register function, int num_reg_arguments,
                           int num_double_arguments);

  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual, Label* done,
                      bool* definitely_mismatches, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InitializeNewString(Register string, Register length,
                           Heap::RootListIndex map_index, Register scratch1,
                           Register scratch2);

  // Helper for implementing JumpIfNotInNewSpace and JumpIfInNewSpace.
  void InNewSpace(Register object, Register scratch,
                  Condition cond,  // eq for new space, ne otherwise.
                  Label* branch);

  // Helper for finding the mark bits for an address.  Afterwards, the
  // bitmap register points at the word with the mark bits and the mask
  // the position of the first bit.  Leaves addr_reg unchanged.
  inline void GetMarkBits(Register addr_reg, Register bitmap_reg,
                          Register mask_reg);

  static const RegList kSafepointSavedRegisters;
  static const int kNumSafepointSavedRegisters;

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);
  MemOperand SafepointRegisterSlot(Register reg);
  MemOperand SafepointRegistersAndDoublesSlot(Register reg);

  bool generating_stub_;
  bool has_frame_;
  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Needs access to SafepointRegisterStackIndex for compiled frame
  // traversal.
  friend class StandardFrame;
};


// The code patcher is used to patch (typically) small parts of code e.g. for
// debugging and other types of instrumentation. When using the code patcher
// the exact number of bytes specified must be emitted. It is not legal to emit
// relocation information. If any of these constraints are violated it causes
// an assertion to fail.
class CodePatcher {
 public:
  enum FlushICache { FLUSH, DONT_FLUSH };

  CodePatcher(Isolate* isolate, byte* address, int instructions,
              FlushICache flush_cache = FLUSH);
  ~CodePatcher();

  // Macro assembler to emit code.
  MacroAssembler* masm() { return &masm_; }

  // Emit an instruction directly.
  void Emit(Instr instr);

  // Emit the condition part of an instruction leaving the rest of the current
  // instruction unchanged.
  void EmitCondition(Condition cond);

 private:
  byte* address_;            // The address of the code being patched.
  int size_;                 // Number of bytes of the expected patch size.
  MacroAssembler masm_;      // Macro assembler used to generate the code.
  FlushICache flush_cache_;  // Whether to flush the I cache after patching.
};


// -----------------------------------------------------------------------------
// Static helper functions.

inline MemOperand ContextMemOperand(Register context, int index = 0) {
  return MemOperand(context, Context::SlotOffset(index));
}


inline MemOperand NativeContextMemOperand() {
  return ContextMemOperand(cp, Context::NATIVE_CONTEXT_INDEX);
}


#ifdef GENERATED_CODE_COVERAGE
#define CODE_COVERAGE_STRINGIFY(x) #x
#define CODE_COVERAGE_TOSTRING(x) CODE_COVERAGE_STRINGIFY(x)
#define __FILE_LINE__ __FILE__ ":" CODE_COVERAGE_TOSTRING(__LINE__)
#define ACCESS_MASM(masm)    \
  masm->stop(__FILE_LINE__); \
  masm->
#else
#define ACCESS_MASM(masm) masm->
#endif
}  // namespace internal
}  // namespace v8

#endif  // V8_PPC_MACRO_ASSEMBLER_PPC_H_
