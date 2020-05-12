// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDED_FROM_MACRO_ASSEMBLER_H
#error This header must be included via macro-assembler.h
#endif

#ifndef V8_CODEGEN_PPC_MACRO_ASSEMBLER_PPC_H_
#define V8_CODEGEN_PPC_MACRO_ASSEMBLER_PPC_H_

#include "src/codegen/bailout-reason.h"
#include "src/codegen/ppc/assembler-ppc.h"
#include "src/common/globals.h"
#include "src/numbers/double.h"
#include "src/objects/contexts.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Static helper functions

// Generate a MemOperand for loading a field from an object.
inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}

enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum LinkRegisterStatus { kLRHasNotBeenSaved, kLRHasBeenSaved };

Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2 = no_reg,
                                   Register reg3 = no_reg,
                                   Register reg4 = no_reg,
                                   Register reg5 = no_reg,
                                   Register reg6 = no_reg);

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
#endif

class V8_EXPORT_PRIVATE TurboAssembler : public TurboAssemblerBase {
 public:
  using TurboAssemblerBase::TurboAssemblerBase;

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

  // Activation support.
  void EnterFrame(StackFrame::Type type,
                  bool load_constant_pool_pointer_reg = false);

  // Returns the pc offset at which the frame ends.
  int LeaveFrame(StackFrame::Type type, int stack_adjustment = 0);

  // Push a fixed frame, consisting of lr, fp, constant pool.
  void PushCommonFrame(Register marker_reg = no_reg);

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue();

  // Push a standard frame, consisting of lr, fp, constant pool,
  // context and JS function
  void PushStandardFrame(Register function_reg);

  // Restore caller's frame pointer and return address prior to being
  // overwritten by tail call stack preparation.
  void RestoreFrameStateForTailCall();

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();

  void InitializeRootRegister() {
    ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
    mov(kRootRegister, Operand(isolate_root));
  }

  // These exist to provide portability between 32 and 64bit
  void LoadP(Register dst, const MemOperand& mem, Register scratch = no_reg);
  void LoadPU(Register dst, const MemOperand& mem, Register scratch = no_reg);
  void LoadWordArith(Register dst, const MemOperand& mem,
                     Register scratch = no_reg);
  void StoreP(Register src, const MemOperand& mem, Register scratch = no_reg);
  void StorePU(Register src, const MemOperand& mem, Register scratch = no_reg);

  void LoadDouble(DoubleRegister dst, const MemOperand& mem,
                  Register scratch = no_reg);
  void LoadFloat32(DoubleRegister dst, const MemOperand& mem,
                   Register scratch = no_reg);
  void LoadDoubleLiteral(DoubleRegister result, Double value, Register scratch);

  // load a literal signed int value <value> to GPR <dst>
  void LoadIntLiteral(Register dst, int value);
  // load an SMI value <value> to GPR <dst>
  void LoadSmiLiteral(Register dst, Smi smi);

  void LoadSingle(DoubleRegister dst, const MemOperand& mem,
                  Register scratch = no_reg);
  void LoadSingleU(DoubleRegister dst, const MemOperand& mem,
                   Register scratch = no_reg);
  void LoadPC(Register dst);
  void ComputeCodeStartAddress(Register dst);

  void StoreDouble(DoubleRegister src, const MemOperand& mem,
                   Register scratch = no_reg);
  void StoreDoubleU(DoubleRegister src, const MemOperand& mem,
                    Register scratch = no_reg);

  void StoreSingle(DoubleRegister src, const MemOperand& mem,
                   Register scratch = no_reg);
  void StoreSingleU(DoubleRegister src, const MemOperand& mem,
                    Register scratch = no_reg);

  void Cmpi(Register src1, const Operand& src2, Register scratch,
            CRegister cr = cr7);
  void Cmpli(Register src1, const Operand& src2, Register scratch,
             CRegister cr = cr7);
  void Cmpwi(Register src1, const Operand& src2, Register scratch,
             CRegister cr = cr7);
  // Set new rounding mode RN to FPSCR
  void SetRoundingMode(FPRoundingMode RN);

  // reset rounding mode to default (kRoundToNearest)
  void ResetRoundingMode();
  void Add(Register dst, Register src, intptr_t value, Register scratch);

  void Push(Register src) { push(src); }
  // Push a handle.
  void Push(Handle<HeapObject> handle);
  void Push(Smi smi);

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

  void SaveRegisters(RegList registers);
  void RestoreRegisters(RegList registers);

  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
                           SaveFPRegsMode fp_mode);
  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
                           SaveFPRegsMode fp_mode, Address wasm_target);
  void CallEphemeronKeyBarrier(Register object, Register address,
                               SaveFPRegsMode fp_mode);

  void MultiPush(RegList regs, Register location = sp);
  void MultiPop(RegList regs, Register location = sp);

  void MultiPushDoubles(RegList dregs, Register location = sp);
  void MultiPopDoubles(RegList dregs, Register location = sp);

  // Calculate how much stack space (in bytes) are required to store caller
  // registers excluding those specified in the arguments.
  int RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                      Register exclusion1 = no_reg,
                                      Register exclusion2 = no_reg,
                                      Register exclusion3 = no_reg) const;

  // Push caller saved registers on the stack, and return the number of bytes
  // stack pointer is adjusted.
  int PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                      Register exclusion2 = no_reg,
                      Register exclusion3 = no_reg);
  // Restore caller saved registers from the stack, and return the number of
  // bytes stack pointer is adjusted.
  int PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                     Register exclusion2 = no_reg,
                     Register exclusion3 = no_reg);

  // Load an object from the root table.
  void LoadRoot(Register destination, RootIndex index) override {
    LoadRoot(destination, index, al);
  }
  void LoadRoot(Register destination, RootIndex index, Condition cond);

  void SwapP(Register src, Register dst, Register scratch);
  void SwapP(Register src, MemOperand dst, Register scratch);
  void SwapP(MemOperand src, MemOperand dst, Register scratch_0,
             Register scratch_1);
  void SwapFloat32(DoubleRegister src, DoubleRegister dst,
                   DoubleRegister scratch);
  void SwapFloat32(DoubleRegister src, MemOperand dst, DoubleRegister scratch);
  void SwapFloat32(MemOperand src, MemOperand dst, DoubleRegister scratch_0,
                   DoubleRegister scratch_1);
  void SwapDouble(DoubleRegister src, DoubleRegister dst,
                  DoubleRegister scratch);
  void SwapDouble(DoubleRegister src, MemOperand dst, DoubleRegister scratch);
  void SwapDouble(MemOperand src, MemOperand dst, DoubleRegister scratch_0,
                  DoubleRegister scratch_1);

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

  void PrepareForTailCall(Register callee_args_count,
                          Register caller_args_count, Register scratch0,
                          Register scratch1);

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
  void CallCFunction(ExternalReference function, int num_arguments,
                     bool has_function_descriptor = true);
  void CallCFunction(Register function, int num_arguments,
                     bool has_function_descriptor = true);
  void CallCFunction(ExternalReference function, int num_reg_arguments,
                     int num_double_arguments,
                     bool has_function_descriptor = true);
  void CallCFunction(Register function, int num_reg_arguments,
                     int num_double_arguments,
                     bool has_function_descriptor = true);

  void MovFromFloatParameter(DoubleRegister dst);
  void MovFromFloatResult(DoubleRegister dst);

  void Trap() override;
  void DebugBreak() override;

  // Calls Abort(msg) if the condition cond is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cond, AbortReason reason, CRegister cr = cr7);

  // Like Assert(), but always enabled.
  void Check(Condition cond, AbortReason reason, CRegister cr = cr7);

  // Print a message to stdout and abort execution.
  void Abort(AbortReason reason);

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

  void LoadFromConstantsTable(Register destination,
                              int constant_index) override;
  void LoadRootRegisterOffset(Register destination, intptr_t offset) override;
  void LoadRootRelative(Register destination, int32_t offset) override;

  // Jump, Call, and Ret pseudo instructions implementing inter-working.
  void Jump(Register target);
  void Jump(Address target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);
  void Jump(const ExternalReference& reference) override;
  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);
  void Call(Register target);
  void Call(Address target, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Handle<Code> code, RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            Condition cond = al);
  void Call(Label* target);

  // Load the builtin given by the Smi in |builtin_index| into the same
  // register.
  void LoadEntryFromBuiltinIndex(Register builtin_index);
  void LoadCodeObjectEntry(Register destination, Register code_object) override;
  void CallCodeObject(Register code_object) override;
  void JumpCodeObject(Register code_object) override;

  void CallBuiltinByIndex(Register builtin_index) override;
  void CallForDeoptimization(Address target, int deopt_id, Label* exit,
                             DeoptimizeKind kind);

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count);
  void Drop(Register count, Register scratch = r0);

  void Ret() { blr(); }
  void Ret(Condition cond, CRegister cr = cr7) { bclr(cond, cr); }
  void Ret(int drop) {
    Drop(drop);
    blr();
  }

  // If the value is a NaN, canonicalize the value else, do nothing.
  void CanonicalizeNaN(const DoubleRegister dst, const DoubleRegister src);
  void CanonicalizeNaN(const DoubleRegister value) {
    CanonicalizeNaN(value, value);
  }
  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met);

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
  // Register move. May do nothing if the registers are identical.
  void Move(Register dst, Smi smi) { LoadSmiLiteral(dst, smi); }
  void Move(Register dst, Handle<HeapObject> value);
  void Move(Register dst, ExternalReference reference);
  void Move(Register dst, Register src, Condition cond = al);
  void Move(DoubleRegister dst, DoubleRegister src);

  void SmiUntag(Register reg, RCBit rc = LeaveRC, int scale = 0) {
    SmiUntag(reg, reg, rc, scale);
  }

  void SmiUntag(Register dst, Register src, RCBit rc = LeaveRC, int scale = 0) {
    if (scale > kSmiShift) {
      ShiftLeftImm(dst, src, Operand(scale - kSmiShift), rc);
    } else if (scale < kSmiShift) {
      ShiftRightArithImm(dst, src, kSmiShift - scale, rc);
    } else {
      // do nothing
    }
  }

  void ZeroExtByte(Register dst, Register src);
  void ZeroExtHalfWord(Register dst, Register src);
  void ZeroExtWord32(Register dst, Register src);

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
    DCHECK(rangeStart >= rangeEnd && rangeStart < kBitsPerSystemPointer);
    int rotate = (rangeEnd == 0) ? 0 : kBitsPerSystemPointer - rangeEnd;
    int width = rangeStart - rangeEnd + 1;
    if (rc == SetRC && rangeStart < 16 && (rangeEnd == 0 || test)) {
      // Prefer faster andi when applicable.
      andi(dst, src, Operand(((1 << width) - 1) << rangeEnd));
    } else {
#if V8_TARGET_ARCH_PPC64
      rldicl(dst, src, rotate, kBitsPerSystemPointer - width, rc);
#else
      rlwinm(dst, src, rotate, kBitsPerSystemPointer - width,
             kBitsPerSystemPointer - 1, rc);
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
    int start = kBitsPerSystemPointer - 1;
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

  // Test consecutive bit range in value.  Range is defined by mask.
  inline void TestBitMask(Register value, uintptr_t mask,
                          Register scratch = r0) {
    ExtractBitMask(scratch, value, mask, SetRC, true);
  }
  // Test consecutive bit range in value.  Range is defined by
  // rangeStart - rangeEnd.
  inline void TestBitRange(Register value, int rangeStart, int rangeEnd,
                           Register scratch = r0) {
    ExtractBitRange(scratch, value, rangeStart, rangeEnd, SetRC, true);
  }

  inline void TestIfSmi(Register value, Register scratch) {
    TestBitRange(value, kSmiTagSize - 1, 0, scratch);
  }
  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label) {
    TestIfSmi(value, r0);
    beq(smi_label, cr0);  // branch if SMI
  }
  void JumpIfEqual(Register x, int32_t y, Label* dest);
  void JumpIfLessThan(Register x, int32_t y, Label* dest);

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

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. Goes to 'done' if it
  // succeeds, otherwise falls through if result is saturated. On return
  // 'result' either holds answer, or is clobbered on fall through.
  void TryInlineTruncateDoubleToI(Register result, DoubleRegister input,
                                  Label* done);
  void TruncateDoubleToI(Isolate* isolate, Zone* zone, Register result,
                         DoubleRegister double_input, StubCallMode stub_mode);

  void LoadConstantPoolPointerRegister();

  // Loads the constant pool pointer (kConstantPoolRegister).
  void LoadConstantPoolPointerRegisterFromCodeTargetAddress(
      Register code_target_address);
  void AbortConstantPoolBuilding() {
#ifdef DEBUG
    // Avoid DCHECK(!is_linked()) failure in ~Label()
    bind(ConstantPoolPosition());
#endif
  }

  // Generates an instruction sequence s.t. the return address points to the
  // instruction following the call.
  // The return address on the stack is used by frame iteration.
  void StoreReturnAddressAndCall(Register target);

  void ResetSpeculationPoisonRegister();

  // Control-flow integrity:

  // Define a function entrypoint. This doesn't emit any code for this
  // architecture, as control-flow integrity is not supported for it.
  void CodeEntry() {}
  // Define an exception handler.
  void ExceptionHandler() {}
  // Define an exception handler and bind a label.
  void BindExceptionHandler(Label* label) { bind(label); }

 private:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  int CalculateStackPassedWords(int num_reg_arguments,
                                int num_double_arguments);
  void CallCFunctionHelper(Register function, int num_reg_arguments,
                           int num_double_arguments,
                           bool has_function_descriptor);
  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
                           SaveFPRegsMode fp_mode, Handle<Code> code_target,
                           Address wasm_target);
};

// MacroAssembler implements a collection of frequently used acros.
class V8_EXPORT_PRIVATE MacroAssembler : public TurboAssembler {
 public:
  using TurboAssembler::TurboAssembler;

  // ---------------------------------------------------------------------------
  // GC Support

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldMemOperand(reg, off).
  void RecordWriteField(
      Register object, int offset, Register value, Register scratch,
      LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

  // For a given |object| notify the garbage collector that the slot |address|
  // has been written.  |value| is the object being stored. The value and
  // address registers are clobbered by the operation.
  void RecordWrite(
      Register object, Register address, Register value,
      LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK);

  // Enter exit frame.
  // stack_space - extra stack space, used for parameters before call to C.
  // At least one slot (for the return address) should be provided.
  void EnterExitFrame(bool save_doubles, int stack_space = 1,
                      StackFrame::Type frame_type = StackFrame::EXIT);

  // Leave the current exit frame. Expects the return value in r0.
  // Expect the number of values, pushed prior to the exit frame, to
  // remove in a register (or no_reg, if there is nothing to remove).
  void LeaveExitFrame(bool save_doubles, Register argument_count,
                      bool argument_count_is_length = false);

  void LoadMap(Register destination, Register object);

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst) {
    LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
  }

  void LoadNativeContextSlot(int index, Register dst);

  // ----------------------------------------------------------------
  // new PPC macro-assembler interfaces that are slightly higher level
  // than assembler-ppc and may generate variable length sequences

  // load a literal double value <value> to FPR <result>
  void LoadWord(Register dst, const MemOperand& mem, Register scratch);
  void StoreWord(Register src, const MemOperand& mem, Register scratch);

  void LoadHalfWord(Register dst, const MemOperand& mem,
                    Register scratch = no_reg);
  void LoadHalfWordArith(Register dst, const MemOperand& mem,
                         Register scratch = no_reg);
  void StoreHalfWord(Register src, const MemOperand& mem, Register scratch);

  void LoadByte(Register dst, const MemOperand& mem, Register scratch);
  void StoreByte(Register src, const MemOperand& mem, Register scratch);

  void LoadDoubleU(DoubleRegister dst, const MemOperand& mem,
                   Register scratch = no_reg);

  void Cmplwi(Register src1, const Operand& src2, Register scratch,
              CRegister cr = cr7);
  void And(Register ra, Register rs, const Operand& rb, RCBit rc = LeaveRC);
  void Or(Register ra, Register rs, const Operand& rb, RCBit rc = LeaveRC);
  void Xor(Register ra, Register rs, const Operand& rb, RCBit rc = LeaveRC);

  void AddSmiLiteral(Register dst, Register src, Smi smi, Register scratch);
  void SubSmiLiteral(Register dst, Register src, Smi smi, Register scratch);
  void CmpSmiLiteral(Register src1, Smi smi, Register scratch,
                     CRegister cr = cr7);
  void CmplSmiLiteral(Register src1, Smi smi, Register scratch,
                      CRegister cr = cr7);
  void AndSmiLiteral(Register dst, Register src, Smi smi, Register scratch,
                     RCBit rc = LeaveRC);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // Both |callee_args_count| and |caller_args_countg| do not include
  // receiver. |callee_args_count| is not modified. |caller_args_count|
  // is trashed.

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeFunctionCode(Register function, Register new_target,
                          Register expected_parameter_count,
                          Register actual_parameter_count, InvokeFlag flag);

  // On function call, call into the debugger if necessary.
  void CheckDebugHook(Register fun, Register new_target,
                      Register expected_parameter_count,
                      Register actual_parameter_count);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunctionWithNewTarget(Register function, Register new_target,
                                   Register actual_parameter_count,
                                   InvokeFlag flag);
  void InvokeFunction(Register function, Register expected_parameter_count,
                      Register actual_parameter_count, InvokeFlag flag);

  // Frame restart support
  void MaybeDropFrames();

  // Exception handling

  // Push a new stack handler and link into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  // Must preserve the result register.
  void PopStackHandler();

  // ---------------------------------------------------------------------------
  // Support functions.

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

  // Compare the object in a register to a value from the root list.
  // Uses the ip register as scratch.
  void CompareRoot(Register obj, RootIndex index);
  void PushRoot(RootIndex index) {
    LoadRoot(r0, index);
    Push(r0);
  }

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(Register with, RootIndex index, Label* if_equal) {
    CompareRoot(with, index);
    beq(if_equal);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(Register with, RootIndex index, Label* if_not_equal) {
    CompareRoot(with, index);
    bne(if_not_equal);
  }

  // Checks if value is in range [lower_limit, higher_limit] using a single
  // comparison.
  void JumpIfIsInRange(Register value, unsigned lower_limit,
                       unsigned higher_limit, Label* on_in_range);

  // ---------------------------------------------------------------------------
  // Runtime calls

  static int CallSizeNotPredictableCodeSize(Address target,
                                            RelocInfo::Mode rmode,
                                            Condition cond = al);
  void CallJSEntry(Register target);

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

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& builtin,
                               bool builtin_exit_frame = false);

  // Generates a trampoline to jump to the off-heap instruction stream.
  void JumpToInstructionStream(Address entry);

  // ---------------------------------------------------------------------------
  // In-place weak references.
  void LoadWeakValue(Register out, Register in, Label* target_if_cleared);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void IncrementCounter(StatsCounter* counter, int value, Register scratch1,
                        Register scratch2);
  void DecrementCounter(StatsCounter* counter, int value, Register scratch1,
                        Register scratch2);

  // ---------------------------------------------------------------------------
  // Smi utilities

  // Shift left by kSmiShift
  void SmiTag(Register reg, RCBit rc = LeaveRC) { SmiTag(reg, reg, rc); }
  void SmiTag(Register dst, Register src, RCBit rc = LeaveRC) {
    ShiftLeftImm(dst, src, Operand(kSmiShift), rc);
  }

  void SmiToPtrArrayOffset(Register dst, Register src) {
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift < kPointerSizeLog2);
    ShiftLeftImm(dst, src, Operand(kPointerSizeLog2 - kSmiShift));
#else
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift > kPointerSizeLog2);
    ShiftRightArithImm(dst, src, kSmiShift - kPointerSizeLog2);
#endif
  }

  // Jump if either of the registers contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label) {
    TestIfSmi(value, r0);
    bne(not_smi_label, cr0);
  }

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);
  void AssertSmi(Register object);

#if !defined(V8_COMPRESS_POINTERS) && !defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  // Ensure it is permissible to read/write int value directly from
  // upper half of the smi.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 32);
#endif
#if V8_TARGET_ARCH_PPC64 && V8_TARGET_LITTLE_ENDIAN
#define SmiWordOffset(offset) (offset + kPointerSize / 2)
#else
#define SmiWordOffset(offset) offset
#endif

  // Abort execution if argument is not a Constructor, enabled via --debug-code.
  void AssertConstructor(Register object);

  // Abort execution if argument is not a JSFunction, enabled via --debug-code.
  void AssertFunction(Register object);

  // Abort execution if argument is not a JSBoundFunction,
  // enabled via --debug-code.
  void AssertBoundFunction(Register object);

  // Abort execution if argument is not a JSGeneratorObject (or subclass),
  // enabled via --debug-code.
  void AssertGeneratorObject(Register object);

  // Abort execution if argument is not undefined or an AllocationSite, enabled
  // via --debug-code.
  void AssertUndefinedOrAllocationSite(Register object, Register scratch);

  // ---------------------------------------------------------------------------
  // Patching helpers.

  template <typename Field>
  void DecodeField(Register dst, Register src, RCBit rc = LeaveRC) {
    ExtractBitRange(dst, src, Field::kShift + Field::kSize - 1, Field::kShift,
                    rc);
  }

  template <typename Field>
  void DecodeField(Register reg, RCBit rc = LeaveRC) {
    DecodeField<Field>(reg, reg, rc);
  }

 private:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  // Helper functions for generating invokes.
  void InvokePrologue(Register expected_parameter_count,
                      Register actual_parameter_count, Label* done,
                      InvokeFlag flag);

  DISALLOW_IMPLICIT_CONSTRUCTORS(MacroAssembler);
};

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_PPC_MACRO_ASSEMBLER_PPC_H_
