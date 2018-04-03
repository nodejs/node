// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_S390_MACRO_ASSEMBLER_S390_H_
#define V8_S390_MACRO_ASSEMBLER_S390_H_

#include "src/assembler.h"
#include "src/bailout-reason.h"
#include "src/globals.h"
#include "src/s390/assembler-s390.h"

namespace v8 {
namespace internal {

// Give alias names to registers for calling conventions.
const Register kReturnRegister0 = r2;
const Register kReturnRegister1 = r3;
const Register kReturnRegister2 = r4;
const Register kJSFunctionRegister = r3;
const Register kContextRegister = r13;
const Register kAllocateSizeRegister = r3;
const Register kInterpreterAccumulatorRegister = r2;
const Register kInterpreterBytecodeOffsetRegister = r6;
const Register kInterpreterBytecodeArrayRegister = r7;
const Register kInterpreterDispatchTableRegister = r8;
const Register kJavaScriptCallArgCountRegister = r2;
const Register kJavaScriptCallNewTargetRegister = r5;
const Register kRuntimeCallFunctionRegister = r3;
const Register kRuntimeCallArgCountRegister = r2;

// ----------------------------------------------------------------------------
// Static helper functions

// Generate a MemOperand for loading a field from an object.
inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}

// Generate a MemOperand for loading a field from an object.
inline MemOperand FieldMemOperand(Register object, Register index, int offset) {
  return MemOperand(object, index, offset - kHeapObjectTag);
}

// Generate a MemOperand for loading a field from Root register
inline MemOperand RootMemOperand(Heap::RootListIndex index) {
  return MemOperand(kRootRegister, index << kPointerSizeLog2);
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
bool AreAliased(DoubleRegister reg1, DoubleRegister reg2,
                DoubleRegister reg3 = no_dreg, DoubleRegister reg4 = no_dreg,
                DoubleRegister reg5 = no_dreg, DoubleRegister reg6 = no_dreg,
                DoubleRegister reg7 = no_dreg, DoubleRegister reg8 = no_dreg,
                DoubleRegister reg9 = no_dreg, DoubleRegister reg10 = no_dreg);
#endif

// These exist to provide portability between 32 and 64bit
#if V8_TARGET_ARCH_S390X
#define Div divd

// The length of the arithmetic operation is the length
// of the register.

// Length:
// H = halfword
// W = word

// arithmetics and bitwise
#define AddMI agsi
#define AddRR agr
#define SubRR sgr
#define AndRR ngr
#define OrRR ogr
#define XorRR xgr
#define LoadComplementRR lcgr
#define LoadNegativeRR lngr

// Distinct Operands
#define AddP_RRR agrk
#define AddPImm_RRI aghik
#define AddLogicalP_RRR algrk
#define SubP_RRR sgrk
#define SubLogicalP_RRR slgrk
#define AndP_RRR ngrk
#define OrP_RRR ogrk
#define XorP_RRR xgrk

// Load / Store
#define LoadRR lgr
#define LoadAndTestRR ltgr
#define LoadImmP lghi

// Compare
#define CmpPH cghi
#define CmpLogicalPW clgfi

// Shifts
#define ShiftLeftP sllg
#define ShiftRightP srlg
#define ShiftLeftArithP slag
#define ShiftRightArithP srag
#else

// arithmetics and bitwise
// Reg2Reg
#define AddMI asi
#define AddRR ar
#define SubRR sr
#define AndRR nr
#define OrRR or_z
#define XorRR xr
#define LoadComplementRR lcr
#define LoadNegativeRR lnr

// Distinct Operands
#define AddP_RRR ark
#define AddPImm_RRI ahik
#define AddLogicalP_RRR alrk
#define SubP_RRR srk
#define SubLogicalP_RRR slrk
#define AndP_RRR nrk
#define OrP_RRR ork
#define XorP_RRR xrk

// Load / Store
#define LoadRR lr
#define LoadAndTestRR ltr
#define LoadImmP lhi

// Compare
#define CmpPH chi
#define CmpLogicalPW clfi

// Shifts
#define ShiftLeftP ShiftLeft
#define ShiftRightP ShiftRight
#define ShiftLeftArithP ShiftLeftArith
#define ShiftRightArithP ShiftRightArith

#endif

class TurboAssembler : public Assembler {
 public:
  TurboAssembler(Isolate* isolate, void* buffer, int buffer_size,
                 CodeObjectRequired create_code_object);

  Isolate* isolate() const { return isolate_; }

  Handle<HeapObject> CodeObject() {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }

  // Returns the size of a call in instructions.
  static int CallSize(Register target);
  int CallSize(Address target, RelocInfo::Mode rmode, Condition cond = al);

  // Jump, Call, and Ret pseudo instructions implementing inter-working.
  void Jump(Register target);
  void Jump(Address target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label) {
    TestIfSmi(value);
    beq(smi_label /*, cr0*/);  // branch if SMI
  }
  void Call(Register target);
  void Call(Address target, RelocInfo::Mode rmode, Condition cond = al);
  int CallSize(Handle<Code> code,
               RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
               Condition cond = al);
  void Call(Handle<Code> code, RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            Condition cond = al);
  void Ret() { b(r14); }
  void Ret(Condition cond) { b(cond, r14); }

  void CallForDeoptimization(Address target, RelocInfo::Mode rmode) {
    Call(target, rmode);
  }

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count);
  void Drop(Register count, Register scratch = r0);

  void Ret(int drop) {
    Drop(drop);
    Ret();
  }

  void Call(Label* target);

  // Register move. May do nothing if the registers are identical.
  void Move(Register dst, Smi* smi) { LoadSmiLiteral(dst, smi); }
  void Move(Register dst, Handle<HeapObject> value);
  void Move(Register dst, Register src, Condition cond = al);
  void Move(DoubleRegister dst, DoubleRegister src);

  void SaveRegisters(RegList registers);
  void RestoreRegisters(RegList registers);

  void CallRecordWriteStub(Register object, Register address,
                           RememberedSetAction remembered_set_action,
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
  void LoadRoot(Register destination, Heap::RootListIndex index,
                Condition cond = al);
  //--------------------------------------------------------------------------
  // S390 Macro Assemblers for Instructions
  //--------------------------------------------------------------------------

  // Arithmetic Operations

  // Add (Register - Immediate)
  void Add32(Register dst, const Operand& imm);
  void Add32_RI(Register dst, const Operand& imm);
  void AddP(Register dst, const Operand& imm);
  void Add32(Register dst, Register src, const Operand& imm);
  void Add32_RRI(Register dst, Register src, const Operand& imm);
  void AddP(Register dst, Register src, const Operand& imm);

  // Add (Register - Register)
  void Add32(Register dst, Register src);
  void AddP(Register dst, Register src);
  void AddP_ExtendSrc(Register dst, Register src);
  void Add32(Register dst, Register src1, Register src2);
  void AddP(Register dst, Register src1, Register src2);
  void AddP_ExtendSrc(Register dst, Register src1, Register src2);

  // Add (Register - Mem)
  void Add32(Register dst, const MemOperand& opnd);
  void AddP(Register dst, const MemOperand& opnd);
  void AddP_ExtendSrc(Register dst, const MemOperand& opnd);

  // Add (Mem - Immediate)
  void Add32(const MemOperand& opnd, const Operand& imm);
  void AddP(const MemOperand& opnd, const Operand& imm);

  // Add Logical (Register - Register)
  void AddLogical32(Register dst, Register src1, Register src2);

  // Add Logical With Carry (Register - Register)
  void AddLogicalWithCarry32(Register dst, Register src1, Register src2);

  // Add Logical (Register - Immediate)
  void AddLogical(Register dst, const Operand& imm);
  void AddLogicalP(Register dst, const Operand& imm);

  // Add Logical (Register - Mem)
  void AddLogical(Register dst, const MemOperand& opnd);
  void AddLogicalP(Register dst, const MemOperand& opnd);

  // Subtract (Register - Immediate)
  void Sub32(Register dst, const Operand& imm);
  void Sub32_RI(Register dst, const Operand& imm) { Sub32(dst, imm); }
  void SubP(Register dst, const Operand& imm);
  void Sub32(Register dst, Register src, const Operand& imm);
  void Sub32_RRI(Register dst, Register src, const Operand& imm) {
    Sub32(dst, src, imm);
  }
  void SubP(Register dst, Register src, const Operand& imm);

  // Subtract (Register - Register)
  void Sub32(Register dst, Register src);
  void SubP(Register dst, Register src);
  void SubP_ExtendSrc(Register dst, Register src);
  void Sub32(Register dst, Register src1, Register src2);
  void SubP(Register dst, Register src1, Register src2);
  void SubP_ExtendSrc(Register dst, Register src1, Register src2);

  // Subtract (Register - Mem)
  void Sub32(Register dst, const MemOperand& opnd);
  void SubP(Register dst, const MemOperand& opnd);
  void SubP_ExtendSrc(Register dst, const MemOperand& opnd);

  // Subtract Logical (Register - Mem)
  void SubLogical(Register dst, const MemOperand& opnd);
  void SubLogicalP(Register dst, const MemOperand& opnd);
  void SubLogicalP_ExtendSrc(Register dst, const MemOperand& opnd);
  // Subtract Logical 32-bit
  void SubLogical32(Register dst, Register src1, Register src2);
  // Subtract Logical With Borrow 32-bit
  void SubLogicalWithBorrow32(Register dst, Register src1, Register src2);

  // Multiply
  void MulP(Register dst, const Operand& opnd);
  void MulP(Register dst, Register src);
  void MulP(Register dst, const MemOperand& opnd);
  void Mul(Register dst, Register src1, Register src2);
  void Mul32(Register dst, const MemOperand& src1);
  void Mul32(Register dst, Register src1);
  void Mul32(Register dst, const Operand& src1);
  void MulHigh32(Register dst, Register src1, const MemOperand& src2);
  void MulHigh32(Register dst, Register src1, Register src2);
  void MulHigh32(Register dst, Register src1, const Operand& src2);
  void MulHighU32(Register dst, Register src1, const MemOperand& src2);
  void MulHighU32(Register dst, Register src1, Register src2);
  void MulHighU32(Register dst, Register src1, const Operand& src2);
  void Mul32WithOverflowIfCCUnequal(Register dst, Register src1,
                                    const MemOperand& src2);
  void Mul32WithOverflowIfCCUnequal(Register dst, Register src1, Register src2);
  void Mul32WithOverflowIfCCUnequal(Register dst, Register src1,
                                    const Operand& src2);
  void Mul64(Register dst, const MemOperand& src1);
  void Mul64(Register dst, Register src1);
  void Mul64(Register dst, const Operand& src1);
  void MulPWithCondition(Register dst, Register src1, Register src2);

  // Divide
  void DivP(Register dividend, Register divider);
  void Div32(Register dst, Register src1, const MemOperand& src2);
  void Div32(Register dst, Register src1, Register src2);
  void DivU32(Register dst, Register src1, const MemOperand& src2);
  void DivU32(Register dst, Register src1, Register src2);
  void Div64(Register dst, Register src1, const MemOperand& src2);
  void Div64(Register dst, Register src1, Register src2);
  void DivU64(Register dst, Register src1, const MemOperand& src2);
  void DivU64(Register dst, Register src1, Register src2);

  // Mod
  void Mod32(Register dst, Register src1, const MemOperand& src2);
  void Mod32(Register dst, Register src1, Register src2);
  void ModU32(Register dst, Register src1, const MemOperand& src2);
  void ModU32(Register dst, Register src1, Register src2);
  void Mod64(Register dst, Register src1, const MemOperand& src2);
  void Mod64(Register dst, Register src1, Register src2);
  void ModU64(Register dst, Register src1, const MemOperand& src2);
  void ModU64(Register dst, Register src1, Register src2);

  // Square root
  void Sqrt(DoubleRegister result, DoubleRegister input);
  void Sqrt(DoubleRegister result, const MemOperand& input);

  // Compare
  void Cmp32(Register src1, Register src2);
  void CmpP(Register src1, Register src2);
  void Cmp32(Register dst, const Operand& opnd);
  void CmpP(Register dst, const Operand& opnd);
  void Cmp32(Register dst, const MemOperand& opnd);
  void CmpP(Register dst, const MemOperand& opnd);

  // Compare Logical
  void CmpLogical32(Register src1, Register src2);
  void CmpLogicalP(Register src1, Register src2);
  void CmpLogical32(Register src1, const Operand& opnd);
  void CmpLogicalP(Register src1, const Operand& opnd);
  void CmpLogical32(Register dst, const MemOperand& opnd);
  void CmpLogicalP(Register dst, const MemOperand& opnd);

  // Compare Logical Byte (CLI/CLIY)
  void CmpLogicalByte(const MemOperand& mem, const Operand& imm);

  // Load 32bit
  void Load(Register dst, const MemOperand& opnd);
  void Load(Register dst, const Operand& opnd);
  void LoadW(Register dst, const MemOperand& opnd, Register scratch = no_reg);
  void LoadW(Register dst, Register src);
  void LoadlW(Register dst, const MemOperand& opnd, Register scratch = no_reg);
  void LoadlW(Register dst, Register src);
  void LoadLogicalHalfWordP(Register dst, const MemOperand& opnd);
  void LoadLogicalHalfWordP(Register dst, Register src);
  void LoadB(Register dst, const MemOperand& opnd);
  void LoadB(Register dst, Register src);
  void LoadlB(Register dst, const MemOperand& opnd);
  void LoadlB(Register dst, Register src);

  void LoadLogicalReversedWordP(Register dst, const MemOperand& opnd);
  void LoadLogicalReversedHalfWordP(Register dst, const MemOperand& opnd);

  // Load And Test
  void LoadAndTest32(Register dst, Register src);
  void LoadAndTestP_ExtendSrc(Register dst, Register src);
  void LoadAndTestP(Register dst, Register src);

  void LoadAndTest32(Register dst, const MemOperand& opnd);
  void LoadAndTestP(Register dst, const MemOperand& opnd);

  // Load Floating Point
  void LoadDouble(DoubleRegister dst, const MemOperand& opnd);
  void LoadFloat32(DoubleRegister dst, const MemOperand& opnd);
  void LoadFloat32ConvertToDouble(DoubleRegister dst, const MemOperand& mem);

  void AddFloat32(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void AddFloat64(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void SubFloat32(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void SubFloat64(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void MulFloat32(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void MulFloat64(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void DivFloat32(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void DivFloat64(DoubleRegister dst, const MemOperand& opnd,
                  DoubleRegister scratch);
  void LoadFloat32ToDouble(DoubleRegister dst, const MemOperand& opnd,
                           DoubleRegister scratch);

  // Load On Condition
  void LoadOnConditionP(Condition cond, Register dst, Register src);

  void LoadPositiveP(Register result, Register input);
  void LoadPositive32(Register result, Register input);

  // Store Floating Point
  void StoreDouble(DoubleRegister dst, const MemOperand& opnd);
  void StoreFloat32(DoubleRegister dst, const MemOperand& opnd);
  void StoreDoubleAsFloat32(DoubleRegister src, const MemOperand& mem,
                            DoubleRegister scratch);

  void Branch(Condition c, const Operand& opnd);
  void BranchOnCount(Register r1, Label* l);

  // Shifts
  void ShiftLeft(Register dst, Register src, Register val);
  void ShiftLeft(Register dst, Register src, const Operand& val);
  void ShiftRight(Register dst, Register src, Register val);
  void ShiftRight(Register dst, Register src, const Operand& val);
  void ShiftLeftArith(Register dst, Register src, Register shift);
  void ShiftLeftArith(Register dst, Register src, const Operand& val);
  void ShiftRightArith(Register dst, Register src, Register shift);
  void ShiftRightArith(Register dst, Register src, const Operand& val);

  void ClearRightImm(Register dst, Register src, const Operand& val);

  // Bitwise operations
  void And(Register dst, Register src);
  void AndP(Register dst, Register src);
  void And(Register dst, Register src1, Register src2);
  void AndP(Register dst, Register src1, Register src2);
  void And(Register dst, const MemOperand& opnd);
  void AndP(Register dst, const MemOperand& opnd);
  void And(Register dst, const Operand& opnd);
  void AndP(Register dst, const Operand& opnd);
  void And(Register dst, Register src, const Operand& opnd);
  void AndP(Register dst, Register src, const Operand& opnd);
  void Or(Register dst, Register src);
  void OrP(Register dst, Register src);
  void Or(Register dst, Register src1, Register src2);
  void OrP(Register dst, Register src1, Register src2);
  void Or(Register dst, const MemOperand& opnd);
  void OrP(Register dst, const MemOperand& opnd);
  void Or(Register dst, const Operand& opnd);
  void OrP(Register dst, const Operand& opnd);
  void Or(Register dst, Register src, const Operand& opnd);
  void OrP(Register dst, Register src, const Operand& opnd);
  void Xor(Register dst, Register src);
  void XorP(Register dst, Register src);
  void Xor(Register dst, Register src1, Register src2);
  void XorP(Register dst, Register src1, Register src2);
  void Xor(Register dst, const MemOperand& opnd);
  void XorP(Register dst, const MemOperand& opnd);
  void Xor(Register dst, const Operand& opnd);
  void XorP(Register dst, const Operand& opnd);
  void Xor(Register dst, Register src, const Operand& opnd);
  void XorP(Register dst, Register src, const Operand& opnd);
  void Popcnt32(Register dst, Register src);
  void Not32(Register dst, Register src = no_reg);
  void Not64(Register dst, Register src = no_reg);
  void NotP(Register dst, Register src = no_reg);

#ifdef V8_TARGET_ARCH_S390X
  void Popcnt64(Register dst, Register src);
#endif

  void mov(Register dst, const Operand& src);

  void CleanUInt32(Register x) {
#ifdef V8_TARGET_ARCH_S390X
    llgfr(x, x);
#endif
  }


  void push(Register src) {
    lay(sp, MemOperand(sp, -kPointerSize));
    StoreP(src, MemOperand(sp));
  }

  void pop(Register dst) {
    LoadP(dst, MemOperand(sp));
    la(sp, MemOperand(sp, kPointerSize));
  }

  void pop() { la(sp, MemOperand(sp, kPointerSize)); }

  void Push(Register src) { push(src); }

  // Push a handle.
  void Push(Handle<HeapObject> handle);
  void Push(Smi* smi);

  // Push two registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2) {
    lay(sp, MemOperand(sp, -kPointerSize * 2));
    StoreP(src1, MemOperand(sp, kPointerSize));
    StoreP(src2, MemOperand(sp, 0));
  }

  // Push three registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3) {
    lay(sp, MemOperand(sp, -kPointerSize * 3));
    StoreP(src1, MemOperand(sp, kPointerSize * 2));
    StoreP(src2, MemOperand(sp, kPointerSize));
    StoreP(src3, MemOperand(sp, 0));
  }

  // Push four registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Register src4) {
    lay(sp, MemOperand(sp, -kPointerSize * 4));
    StoreP(src1, MemOperand(sp, kPointerSize * 3));
    StoreP(src2, MemOperand(sp, kPointerSize * 2));
    StoreP(src3, MemOperand(sp, kPointerSize));
    StoreP(src4, MemOperand(sp, 0));
  }

  // Push five registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Register src4,
            Register src5) {
    DCHECK(src1 != src2);
    DCHECK(src1 != src3);
    DCHECK(src2 != src3);
    DCHECK(src1 != src4);
    DCHECK(src2 != src4);
    DCHECK(src3 != src4);
    DCHECK(src1 != src5);
    DCHECK(src2 != src5);
    DCHECK(src3 != src5);
    DCHECK(src4 != src5);

    lay(sp, MemOperand(sp, -kPointerSize * 5));
    StoreP(src1, MemOperand(sp, kPointerSize * 4));
    StoreP(src2, MemOperand(sp, kPointerSize * 3));
    StoreP(src3, MemOperand(sp, kPointerSize * 2));
    StoreP(src4, MemOperand(sp, kPointerSize));
    StoreP(src5, MemOperand(sp, 0));
  }

  void Pop(Register dst) { pop(dst); }

  // Pop two registers. Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2) {
    LoadP(src2, MemOperand(sp, 0));
    LoadP(src1, MemOperand(sp, kPointerSize));
    la(sp, MemOperand(sp, 2 * kPointerSize));
  }

  // Pop three registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3) {
    LoadP(src3, MemOperand(sp, 0));
    LoadP(src2, MemOperand(sp, kPointerSize));
    LoadP(src1, MemOperand(sp, 2 * kPointerSize));
    la(sp, MemOperand(sp, 3 * kPointerSize));
  }

  // Pop four registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3, Register src4) {
    LoadP(src4, MemOperand(sp, 0));
    LoadP(src3, MemOperand(sp, kPointerSize));
    LoadP(src2, MemOperand(sp, 2 * kPointerSize));
    LoadP(src1, MemOperand(sp, 3 * kPointerSize));
    la(sp, MemOperand(sp, 4 * kPointerSize));
  }

  // Pop five registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3, Register src4,
           Register src5) {
    LoadP(src5, MemOperand(sp, 0));
    LoadP(src4, MemOperand(sp, kPointerSize));
    LoadP(src3, MemOperand(sp, 2 * kPointerSize));
    LoadP(src2, MemOperand(sp, 3 * kPointerSize));
    LoadP(src1, MemOperand(sp, 4 * kPointerSize));
    la(sp, MemOperand(sp, 5 * kPointerSize));
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

  void InitializeRootRegister() {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    mov(kRootRegister, Operand(roots_array_start));
  }

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
  void ConvertIntToDouble(DoubleRegister dst, Register src);

  // Converts the unsigned integer (untagged smi) in |src| to
  // a double, storing the result to |dst|
  void ConvertUnsignedIntToDouble(DoubleRegister dst, Register src);

  // Converts the integer (untagged smi) in |src| to
  // a float, storing the result in |dst|
  void ConvertIntToFloat(DoubleRegister dst, Register src);

  // Converts the unsigned integer (untagged smi) in |src| to
  // a float, storing the result in |dst|
  void ConvertUnsignedIntToFloat(DoubleRegister dst, Register src);

  void ConvertInt64ToFloat(DoubleRegister double_dst, Register src);
  void ConvertInt64ToDouble(DoubleRegister double_dst, Register src);
  void ConvertUnsignedInt64ToFloat(DoubleRegister double_dst, Register src);
  void ConvertUnsignedInt64ToDouble(DoubleRegister double_dst, Register src);

  void MovIntToFloat(DoubleRegister dst, Register src);
  void MovFloatToInt(Register dst, DoubleRegister src);
  void MovDoubleToInt64(Register dst, DoubleRegister src);
  void MovInt64ToDouble(DoubleRegister dst, Register src);
  // Converts the double_input to an integer.  Note that, upon return,
  // the contents of double_dst will also hold the fixed point representation.
  void ConvertFloat32ToInt64(const Register dst,
                             const DoubleRegister double_input,
                             FPRoundingMode rounding_mode = kRoundToZero);

  // Converts the double_input to an integer.  Note that, upon return,
  // the contents of double_dst will also hold the fixed point representation.
  void ConvertDoubleToInt64(const Register dst,
                            const DoubleRegister double_input,
                            FPRoundingMode rounding_mode = kRoundToZero);
  void ConvertDoubleToInt32(const Register dst,
                            const DoubleRegister double_input,
                            FPRoundingMode rounding_mode = kRoundToZero);

  void ConvertFloat32ToInt32(const Register result,
                             const DoubleRegister double_input,
                             FPRoundingMode rounding_mode);
  void ConvertFloat32ToUnsignedInt32(
      const Register result, const DoubleRegister double_input,
      FPRoundingMode rounding_mode = kRoundToZero);
  // Converts the double_input to an unsigned integer.  Note that, upon return,
  // the contents of double_dst will also hold the fixed point representation.
  void ConvertDoubleToUnsignedInt64(
      const Register dst, const DoubleRegister double_input,
      FPRoundingMode rounding_mode = kRoundToZero);
  void ConvertDoubleToUnsignedInt32(
      const Register dst, const DoubleRegister double_input,
      FPRoundingMode rounding_mode = kRoundToZero);
  void ConvertFloat32ToUnsignedInt64(
      const Register result, const DoubleRegister double_input,
      FPRoundingMode rounding_mode = kRoundToZero);

#if !V8_TARGET_ARCH_S390X
  void ShiftLeftPair(Register dst_low, Register dst_high, Register src_low,
                     Register src_high, Register scratch, Register shift);
  void ShiftLeftPair(Register dst_low, Register dst_high, Register src_low,
                     Register src_high, uint32_t shift);
  void ShiftRightPair(Register dst_low, Register dst_high, Register src_low,
                      Register src_high, Register scratch, Register shift);
  void ShiftRightPair(Register dst_low, Register dst_high, Register src_low,
                      Register src_high, uint32_t shift);
  void ShiftRightArithPair(Register dst_low, Register dst_high,
                           Register src_low, Register src_high,
                           Register scratch, Register shift);
  void ShiftRightArithPair(Register dst_low, Register dst_high,
                           Register src_low, Register src_high, uint32_t shift);
#endif

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type, Register base = no_reg,
                    int prologue_offset = 0);
  void Prologue(Register base, int prologue_offset = 0);

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();
  // ----------------------------------------------------------------
  // new S390 macro-assembler interfaces that are slightly higher level
  // than assembler-s390 and may generate variable length sequences

  // load a literal signed int value <value> to GPR <dst>
  void LoadIntLiteral(Register dst, int value);

  // load an SMI value <value> to GPR <dst>
  void LoadSmiLiteral(Register dst, Smi* smi);

  // load a literal double value <value> to FPR <result>
  void LoadDoubleLiteral(DoubleRegister result, double value, Register scratch);
  void LoadDoubleLiteral(DoubleRegister result, uint64_t value,
                         Register scratch);

  void LoadFloat32Literal(DoubleRegister result, float value, Register scratch);

  void StoreW(Register src, const MemOperand& mem, Register scratch = no_reg);

  void LoadHalfWordP(Register dst, const MemOperand& mem,
                     Register scratch = no_reg);

  void StoreHalfWord(Register src, const MemOperand& mem,
                     Register scratch = r0);
  void StoreByte(Register src, const MemOperand& mem, Register scratch = r0);

  void AddSmiLiteral(Register dst, Register src, Smi* smi,
                     Register scratch = r0);
  void SubSmiLiteral(Register dst, Register src, Smi* smi,
                     Register scratch = r0);
  void CmpSmiLiteral(Register src1, Smi* smi, Register scratch);
  void CmpLogicalSmiLiteral(Register src1, Smi* smi, Register scratch);
  void AndSmiLiteral(Register dst, Register src, Smi* smi);

  // Set new rounding mode RN to FPSCR
  void SetRoundingMode(FPRoundingMode RN);

  // reset rounding mode to default (kRoundToNearest)
  void ResetRoundingMode();

  // These exist to provide portability between 32 and 64bit
  void LoadP(Register dst, const MemOperand& mem, Register scratch = no_reg);
  void StoreP(Register src, const MemOperand& mem, Register scratch = no_reg);
  void StoreP(const MemOperand& mem, const Operand& opnd,
              Register scratch = no_reg);
  void LoadMultipleP(Register dst1, Register dst2, const MemOperand& mem);
  void StoreMultipleP(Register dst1, Register dst2, const MemOperand& mem);
  void LoadMultipleW(Register dst1, Register dst2, const MemOperand& mem);
  void StoreMultipleW(Register dst1, Register dst2, const MemOperand& mem);

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

  // Cleanse pointer address on 31bit by zero out top  bit.
  // This is a NOP on 64-bit.
  void CleanseP(Register src) {
#if (V8_HOST_ARCH_S390 && !(V8_TARGET_ARCH_S390X))
    nilh(src, Operand(0x7FFF));
#endif
  }

  void PrepareForTailCall(const ParameterCount& callee_args_count,
                          Register caller_args_count_reg, Register scratch0,
                          Register scratch1);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStubDelayed(CodeStub* stub);

  // Call a runtime routine.
  void CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                          SaveFPRegsMode save_doubles = kDontSaveFPRegs);

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

  // Emit code for a truncating division by a constant. The dividend register is
  // unchanged and ip gets clobbered. Dividend and result must be different.
  void TruncateDoubleToIDelayed(Zone* zone, Register result,
                                DoubleRegister double_input);
  void TryInlineTruncateDoubleToI(Register result, DoubleRegister double_input,
                                  Label* done);

  // ---------------------------------------------------------------------------
  // Debugging

  // Calls Abort(msg) if the condition cond is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cond, AbortReason reason, CRegister cr = cr7);

  // Like Assert(), but always enabled.
  void Check(Condition cond, AbortReason reason, CRegister cr = cr7);

  // Print a message to stdout and abort execution.
  void Abort(AbortReason reason);

  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() { return has_frame_; }
  inline bool AllowThisStubCall(CodeStub* stub);

  // ---------------------------------------------------------------------------
  // Bit testing/extraction
  //
  // Bit numbering is such that the least significant bit is bit 0
  // (for consistency between 32/64-bit).

  // Extract consecutive bits (defined by rangeStart - rangeEnd) from src
  // and place them into the least significant bits of dst.
  inline void ExtractBitRange(Register dst, Register src, int rangeStart,
                              int rangeEnd) {
    DCHECK(rangeStart >= rangeEnd && rangeStart < kBitsPerPointer);

    // Try to use RISBG if possible.
    if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
      int shiftAmount = (64 - rangeEnd) % 64;  // Convert to shift left.
      int endBit = 63;  // End is always LSB after shifting.
      int startBit = 63 - rangeStart + rangeEnd;
      risbg(dst, src, Operand(startBit), Operand(endBit), Operand(shiftAmount),
            true);
    } else {
      if (rangeEnd > 0)  // Don't need to shift if rangeEnd is zero.
        ShiftRightP(dst, src, Operand(rangeEnd));
      else if (dst != src)  // If we didn't shift, we might need to copy
        LoadRR(dst, src);
      int width = rangeStart - rangeEnd + 1;
#if V8_TARGET_ARCH_S390X
      uint64_t mask = (static_cast<uint64_t>(1) << width) - 1;
      nihf(dst, Operand(mask >> 32));
      nilf(dst, Operand(mask & 0xFFFFFFFF));
      ltgr(dst, dst);
#else
      uint32_t mask = (1 << width) - 1;
      AndP(dst, Operand(mask));
#endif
    }
  }

  inline void ExtractBit(Register dst, Register src, uint32_t bitNumber) {
    ExtractBitRange(dst, src, bitNumber, bitNumber);
  }

  // Extract consecutive bits (defined by mask) from src and place them
  // into the least significant bits of dst.
  inline void ExtractBitMask(Register dst, Register src, uintptr_t mask,
                             RCBit rc = LeaveRC) {
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

    ExtractBitRange(dst, src, start, end);
  }

  // Test single bit in value.
  inline void TestBit(Register value, int bitNumber, Register scratch = r0) {
    ExtractBitRange(scratch, value, bitNumber, bitNumber);
  }

  // Test consecutive bit range in value.  Range is defined by
  // rangeStart - rangeEnd.
  inline void TestBitRange(Register value, int rangeStart, int rangeEnd,
                           Register scratch = r0) {
    ExtractBitRange(scratch, value, rangeStart, rangeEnd);
  }

  // Test consecutive bit range in value.  Range is defined by mask.
  inline void TestBitMask(Register value, uintptr_t mask,
                          Register scratch = r0) {
    ExtractBitMask(scratch, value, mask, SetRC);
  }
  inline void TestIfSmi(Register value) { tmll(value, Operand(1)); }

  inline void TestIfSmi(MemOperand value) {
    if (is_uint12(value.offset())) {
      tm(value, Operand(1));
    } else if (is_int20(value.offset())) {
      tmy(value, Operand(1));
    } else {
      LoadB(r0, value);
      tmll(r0, Operand(1));
    }
  }

  inline void TestIfInt32(Register value) {
    // High bits must be identical to fit into an 32-bit integer
    cgfr(value, value);
  }
  void SmiUntag(Register reg) { SmiUntag(reg, reg); }

  void SmiUntag(Register dst, Register src) {
    ShiftRightArithP(dst, src, Operand(kSmiShift));
  }

  // Activation support.
  void EnterFrame(StackFrame::Type type,
                  bool load_constant_pool_pointer_reg = false);
  // Returns the pc offset at which the frame ends.
  int LeaveFrame(StackFrame::Type type, int stack_adjustment = 0);

  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met);

 private:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  void CallCFunctionHelper(Register function, int num_reg_arguments,
                           int num_double_arguments);

  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);
  int CalculateStackPassedWords(int num_reg_arguments,
                                int num_double_arguments);

  bool has_frame_ = false;
  Isolate* isolate_;
  // This handle will be patched with the code object on installation.
  Handle<HeapObject> code_object_;
};

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler : public TurboAssembler {
 public:
  MacroAssembler(Isolate* isolate, void* buffer, int size,
                 CodeObjectRequired create_code_object);

  // Call a code stub.
  void TailCallStub(CodeStub* stub, Condition cond = al);

  void CallStub(CodeStub* stub, Condition cond = al);
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
  void CompareRoot(Register obj, Heap::RootListIndex index);
  void PushRoot(Heap::RootListIndex index) {
    LoadRoot(r0, index);
    Push(r0);
  }

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& builtin,
                               bool builtin_exit_frame = false);

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

  // Try to convert a double to a signed 32-bit integer.
  // CR_EQ in cr7 is set and result assigned if the conversion is exact.
  void TryDoubleToInt32Exact(Register result, DoubleRegister double_input,
                             Register scratch, DoubleRegister double_scratch);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void IncrementCounter(StatsCounter* counter, int value, Register scratch1,
                        Register scratch2);
  void DecrementCounter(StatsCounter* counter, int value, Register scratch1,
                        Register scratch2);
  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Set up call kind marking in ecx. The method takes ecx as an
  // explicit first parameter to make the code more readable at the
  // call sites.
  // void SetCallKind(Register dst, CallKind kind);

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // Both |callee_args_count| and |caller_args_count_reg| do not include
  // receiver. |callee_args_count| is not modified, |caller_args_count_reg|
  // is trashed.

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeFunctionCode(Register function, Register new_target,
                          const ParameterCount& expected,
                          const ParameterCount& actual, InvokeFlag flag);

  // On function call, call into the debugger if necessary.
  void CheckDebugHook(Register fun, Register new_target,
                      const ParameterCount& expected,
                      const ParameterCount& actual);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function, Register new_target,
                      const ParameterCount& actual, InvokeFlag flag);

  void InvokeFunction(Register function, const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag);

  // Frame restart support
  void MaybeDropFrames();

  // Exception handling

  // Push a new stack handler and link into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  // Must preserve the result register.
  void PopStackHandler();

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

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst) {
    LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
  }

  void LoadNativeContextSlot(int index, Register dst);

  // ---------------------------------------------------------------------------
  // Smi utilities

  // Shift left by kSmiShift
  void SmiTag(Register reg) { SmiTag(reg, reg); }
  void SmiTag(Register dst, Register src) {
    ShiftLeftP(dst, src, Operand(kSmiShift));
  }

  void SmiToPtrArrayOffset(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift > kPointerSizeLog2);
    ShiftRightArithP(dst, src, Operand(kSmiShift - kPointerSizeLog2));
#else
    STATIC_ASSERT(kSmiTag == 0 && kSmiShift < kPointerSizeLog2);
    ShiftLeftP(dst, src, Operand(kPointerSizeLog2 - kSmiShift));
#endif
  }

  // Untag the source value into destination and jump if source is a smi.
  // Souce and destination can be the same register.
  void UntagAndJumpIfSmi(Register dst, Register src, Label* smi_case);

  // Jump if either of the registers contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label) {
    TestIfSmi(value);
    bne(not_smi_label /*, cr0*/);
  }
  // Jump if either of the registers contain a smi.
  void JumpIfEitherSmi(Register reg1, Register reg2, Label* on_either_smi);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);
  void AssertSmi(Register object);

#if V8_TARGET_ARCH_S390X
  // Ensure it is permissible to read/write int value directly from
  // upper half of the smi.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 32);
#endif
#if V8_TARGET_LITTLE_ENDIAN
#define SmiWordOffset(offset) (offset + kPointerSize / 2)
#else
#define SmiWordOffset(offset) offset
#endif

  // Abort execution if argument is not a FixedArray, enabled via --debug-code.
  void AssertFixedArray(Register object);

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

  template <typename Field>
  void DecodeField(Register dst, Register src) {
    ExtractBitRange(dst, src, Field::kShift + Field::kSize - 1, Field::kShift);
  }

  template <typename Field>
  void DecodeField(Register reg) {
    DecodeField<Field>(reg, reg);
  }

  // ---------------------------------------------------------------------------
  // GC Support

  void IncrementalMarkingRecordWriteHelper(Register object, Register value,
                                           Register address);

  // Record in the remembered set the fact that we have a pointer to new space
  // at the address pointed to by the addr register.  Only works if addr is not
  // in new space.
  void RememberedSetHelper(Register object,  // Used for debug code.
                           Register addr, Register scratch,
                           SaveFPRegsMode save_fp);

  void CallJSEntry(Register target);
  static int CallSizeNotPredictableCodeSize(Address target,
                                            RelocInfo::Mode rmode,
                                            Condition cond = al);
  void JumpToJSEntry(Register target);

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

  // Push and pop the registers that can hold pointers, as defined by the
  // RegList constant kSafepointSavedRegisters.
  void PushSafepointRegisters();
  void PopSafepointRegisters();

  void LoadRepresentation(Register dst, const MemOperand& mem, Representation r,
                          Register scratch = no_reg);
  void StoreRepresentation(Register src, const MemOperand& mem,
                           Representation r, Register scratch = no_reg);

 private:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;
  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual, Label* done,
                      bool* definitely_mismatches, InvokeFlag flag);

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);

  // Needs access to SafepointRegisterStackIndex for compiled frame
  // traversal.
  friend class StandardFrame;
};

// -----------------------------------------------------------------------------
// Static helper functions.

inline MemOperand ContextMemOperand(Register context, int index = 0) {
  return MemOperand(context, Context::SlotOffset(index));
}

inline MemOperand NativeContextMemOperand() {
  return ContextMemOperand(cp, Context::NATIVE_CONTEXT_INDEX);
}

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_MACRO_ASSEMBLER_S390_H_
