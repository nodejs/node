// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MIPS_MACRO_ASSEMBLER_MIPS_H_
#define V8_MIPS_MACRO_ASSEMBLER_MIPS_H_

#include "src/assembler.h"
#include "src/globals.h"
#include "src/mips64/assembler-mips64.h"

namespace v8 {
namespace internal {

// Forward declaration.
class JumpTarget;

// Reserved Register Usage Summary.
//
// Registers t8, t9, and at are reserved for use by the MacroAssembler.
//
// The programmer should know that the MacroAssembler may clobber these three,
// but won't touch other registers except in special cases.
//
// Per the MIPS ABI, register t9 must be used for indirect function call
// via 'jalr t9' or 'jr t9' instructions. This is relied upon by gcc when
// trying to update gp register for position-independent-code. Whenever
// MIPS generated code calls C code, it must be via t9 register.


// Flags used for LeaveExitFrame function.
enum LeaveExitFrameMode {
  EMIT_RETURN = true,
  NO_EMIT_RETURN = false
};

// Flags used for AllocateHeapNumber
enum TaggingMode {
  // Tag the result.
  TAG_RESULT,
  // Don't tag
  DONT_TAG_RESULT
};

// Flags used for the ObjectToDoubleFPURegister function.
enum ObjectToDoubleFlags {
  // No special flags.
  NO_OBJECT_TO_DOUBLE_FLAGS = 0,
  // Object is known to be a non smi.
  OBJECT_NOT_SMI = 1 << 0,
  // Don't load NaNs or infinities, branch to the non number case instead.
  AVOID_NANS_AND_INFINITIES = 1 << 1
};

// Allow programmer to use Branch Delay Slot of Branches, Jumps, Calls.
enum BranchDelaySlot {
  USE_DELAY_SLOT,
  PROTECT
};

// Flags used for the li macro-assembler function.
enum LiFlags {
  // If the constant value can be represented in just 16 bits, then
  // optimize the li to use a single instruction, rather than lui/ori/dsll
  // sequence.
  OPTIMIZE_SIZE = 0,
  // Always use 6 instructions (lui/ori/dsll sequence), even if the constant
  // could be loaded with just one, so that this value is patchable later.
  CONSTANT_SIZE = 1,
  // For address loads only 4 instruction are required. Used to mark
  // constant load that will be used as address without relocation
  // information. It ensures predictable code size, so specific sites
  // in code are patchable.
  ADDRESS_LOAD  = 2
};


enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum PointersToHereCheck {
  kPointersToHereMaybeInteresting,
  kPointersToHereAreAlwaysInteresting
};
enum RAStatus { kRAHasNotBeenSaved, kRAHasBeenSaved };

Register GetRegisterThatIsNotOneOf(Register reg1,
                                   Register reg2 = no_reg,
                                   Register reg3 = no_reg,
                                   Register reg4 = no_reg,
                                   Register reg5 = no_reg,
                                   Register reg6 = no_reg);

bool AreAliased(Register reg1,
                Register reg2,
                Register reg3 = no_reg,
                Register reg4 = no_reg,
                Register reg5 = no_reg,
                Register reg6 = no_reg,
                Register reg7 = no_reg,
                Register reg8 = no_reg);


// -----------------------------------------------------------------------------
// Static helper functions.

inline MemOperand ContextOperand(Register context, int index) {
  return MemOperand(context, Context::SlotOffset(index));
}


inline MemOperand GlobalObjectOperand()  {
  return ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX);
}


// Generate a MemOperand for loading a field from an object.
inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}


inline MemOperand UntagSmiMemOperand(Register rm, int offset) {
  // Assumes that Smis are shifted by 32 bits and little endianness.
  STATIC_ASSERT(kSmiShift == 32);
  return MemOperand(rm, offset + (kSmiShift / kBitsPerByte));
}


inline MemOperand UntagSmiFieldMemOperand(Register rm, int offset) {
  return UntagSmiMemOperand(rm, offset - kHeapObjectTag);
}


// Generate a MemOperand for storing arguments 5..N on the stack
// when calling CallCFunction().
// TODO(plind): Currently ONLY used for O32. Should be fixed for
//              n64, and used in RegExp code, and other places
//              with more than 8 arguments.
inline MemOperand CFunctionArgumentOperand(int index) {
  DCHECK(index > kCArgSlotCount);
  // Argument 5 takes the slot just past the four Arg-slots.
  int offset = (index - 5) * kPointerSize + kCArgsSlotsSize;
  return MemOperand(sp, offset);
}


// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  // The isolate parameter can be NULL if the macro assembler should
  // not use isolate-dependent functionality. In this case, it's the
  // responsibility of the caller to never invoke such function on the
  // macro assembler.
  MacroAssembler(Isolate* isolate, void* buffer, int size);

  // Arguments macros.
#define COND_TYPED_ARGS Condition cond, Register r1, const Operand& r2
#define COND_ARGS cond, r1, r2

  // Cases when relocation is not needed.
#define DECLARE_NORELOC_PROTOTYPE(Name, target_type) \
  void Name(target_type target, BranchDelaySlot bd = PROTECT); \
  inline void Name(BranchDelaySlot bd, target_type target) { \
    Name(target, bd); \
  } \
  void Name(target_type target, \
            COND_TYPED_ARGS, \
            BranchDelaySlot bd = PROTECT); \
  inline void Name(BranchDelaySlot bd, \
                   target_type target, \
                   COND_TYPED_ARGS) { \
    Name(target, COND_ARGS, bd); \
  }

#define DECLARE_BRANCH_PROTOTYPES(Name) \
  DECLARE_NORELOC_PROTOTYPE(Name, Label*) \
  DECLARE_NORELOC_PROTOTYPE(Name, int16_t)

  DECLARE_BRANCH_PROTOTYPES(Branch)
  DECLARE_BRANCH_PROTOTYPES(BranchAndLink)
  DECLARE_BRANCH_PROTOTYPES(BranchShort)

#undef DECLARE_BRANCH_PROTOTYPES
#undef COND_TYPED_ARGS
#undef COND_ARGS


  // Jump, Call, and Ret pseudo instructions implementing inter-working.
#define COND_ARGS Condition cond = al, Register rs = zero_reg, \
  const Operand& rt = Operand(zero_reg), BranchDelaySlot bd = PROTECT

  void Jump(Register target, COND_ARGS);
  void Jump(intptr_t target, RelocInfo::Mode rmode, COND_ARGS);
  void Jump(Address target, RelocInfo::Mode rmode, COND_ARGS);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, COND_ARGS);
  static int CallSize(Register target, COND_ARGS);
  void Call(Register target, COND_ARGS);
  static int CallSize(Address target, RelocInfo::Mode rmode, COND_ARGS);
  void Call(Address target, RelocInfo::Mode rmode, COND_ARGS);
  int CallSize(Handle<Code> code,
               RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
               TypeFeedbackId ast_id = TypeFeedbackId::None(),
               COND_ARGS);
  void Call(Handle<Code> code,
            RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            TypeFeedbackId ast_id = TypeFeedbackId::None(),
            COND_ARGS);
  void Ret(COND_ARGS);
  inline void Ret(BranchDelaySlot bd, Condition cond = al,
    Register rs = zero_reg, const Operand& rt = Operand(zero_reg)) {
    Ret(cond, rs, rt, bd);
  }

  void Branch(Label* L,
              Condition cond,
              Register rs,
              Heap::RootListIndex index,
              BranchDelaySlot bdslot = PROTECT);

#undef COND_ARGS

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count,
            Condition cond = cc_always,
            Register reg = no_reg,
            const Operand& op = Operand(no_reg));

  // Trivial case of DropAndRet that utilizes the delay slot and only emits
  // 2 instructions.
  void DropAndRet(int drop);

  void DropAndRet(int drop,
                  Condition cond,
                  Register reg,
                  const Operand& op);

  // Swap two registers.  If the scratch register is omitted then a slightly
  // less efficient form using xor instead of mov is emitted.
  void Swap(Register reg1, Register reg2, Register scratch = no_reg);

  void Call(Label* target);

  inline void Move(Register dst, Register src) {
    if (!dst.is(src)) {
      mov(dst, src);
    }
  }

  inline void Move(FPURegister dst, FPURegister src) {
    if (!dst.is(src)) {
      mov_d(dst, src);
    }
  }

  inline void Move(Register dst_low, Register dst_high, FPURegister src) {
    mfc1(dst_low, src);
    mfhc1(dst_high, src);
  }

  inline void FmoveHigh(Register dst_high, FPURegister src) {
    mfhc1(dst_high, src);
  }

  inline void FmoveHigh(FPURegister dst, Register src_high) {
    mthc1(src_high, dst);
  }

  inline void FmoveLow(Register dst_low, FPURegister src) {
    mfc1(dst_low, src);
  }

  void FmoveLow(FPURegister dst, Register src_low);

  inline void Move(FPURegister dst, Register src_low, Register src_high) {
    mtc1(src_low, dst);
    mthc1(src_high, dst);
  }

  void Move(FPURegister dst, float imm);
  void Move(FPURegister dst, double imm);

  // Conditional move.
  void Movz(Register rd, Register rs, Register rt);
  void Movn(Register rd, Register rs, Register rt);
  void Movt(Register rd, Register rs, uint16_t cc = 0);
  void Movf(Register rd, Register rs, uint16_t cc = 0);

  void Clz(Register rd, Register rs);

  // Jump unconditionally to given label.
  // We NEED a nop in the branch delay slot, as it used by v8, for example in
  // CodeGenerator::ProcessDeferred().
  // Currently the branch delay slot is filled by the MacroAssembler.
  // Use rather b(Label) for code generation.
  void jmp(Label* L) {
    Branch(L);
  }

  void Load(Register dst, const MemOperand& src, Representation r);
  void Store(Register src, const MemOperand& dst, Representation r);

  // Load an object from the root table.
  void LoadRoot(Register destination,
                Heap::RootListIndex index);
  void LoadRoot(Register destination,
                Heap::RootListIndex index,
                Condition cond, Register src1, const Operand& src2);

  // Store an object to the root table.
  void StoreRoot(Register source,
                 Heap::RootListIndex index);
  void StoreRoot(Register source,
                 Heap::RootListIndex index,
                 Condition cond, Register src1, const Operand& src2);

  // ---------------------------------------------------------------------------
  // GC Support

  void IncrementalMarkingRecordWriteHelper(Register object,
                                           Register value,
                                           Register address);

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
                     Label* condition_met);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfNotInNewSpace(Register object,
                           Register scratch,
                           Label* branch) {
    InNewSpace(object, scratch, ne, branch);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfInNewSpace(Register object,
                        Register scratch,
                        Label* branch) {
    InNewSpace(object, scratch, eq, branch);
  }

  // Check if an object has a given incremental marking color.
  void HasColor(Register object,
                Register scratch0,
                Register scratch1,
                Label* has_color,
                int first_bit,
                int second_bit);

  void JumpIfBlack(Register object,
                   Register scratch0,
                   Register scratch1,
                   Label* on_black);

  // Checks the color of an object.  If the object is already grey or black
  // then we just fall through, since it is already live.  If it is white and
  // we can determine that it doesn't need to be scanned, then we just mark it
  // black and fall through.  For the rest we jump to the label so the
  // incremental marker can fix its assumptions.
  void EnsureNotWhite(Register object,
                      Register scratch1,
                      Register scratch2,
                      Register scratch3,
                      Label* object_is_white_and_not_data);

  // Detects conservatively whether an object is data-only, i.e. it does need to
  // be scanned by the garbage collector.
  void JumpIfDataObject(Register value,
                        Register scratch,
                        Label* not_data_object);

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
      RAStatus ra_status,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // As above, but the offset has the tag presubtracted.  For use with
  // MemOperand(reg, off).
  inline void RecordWriteContextSlot(
      Register context,
      int offset,
      Register value,
      Register scratch,
      RAStatus ra_status,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting) {
    RecordWriteField(context,
                     offset + kHeapObjectTag,
                     value,
                     scratch,
                     ra_status,
                     save_fp,
                     remembered_set_action,
                     smi_check,
                     pointers_to_here_check_for_value);
  }

  void RecordWriteForMap(
      Register object,
      Register map,
      Register dst,
      RAStatus ra_status,
      SaveFPRegsMode save_fp);

  // For a given |object| notify the garbage collector that the slot |address|
  // has been written.  |value| is the object being stored. The value and
  // address registers are clobbered by the operation.
  void RecordWrite(
      Register object,
      Register address,
      Register value,
      RAStatus ra_status,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);


  // ---------------------------------------------------------------------------
  // Inline caching support.

  // Generate code for checking access rights - used for security checks
  // on access to global objects across environments. The holder register
  // is left untouched, whereas both scratch registers are clobbered.
  void CheckAccessGlobalProxy(Register holder_reg,
                              Register scratch,
                              Label* miss);

  void GetNumberHash(Register reg0, Register scratch);

  void LoadFromNumberDictionary(Label* miss,
                                Register elements,
                                Register key,
                                Register result,
                                Register reg0,
                                Register reg1,
                                Register reg2);


  inline void MarkCode(NopMarkerTypes type) {
    nop(type);
  }

  // Check if the given instruction is a 'type' marker.
  // i.e. check if it is a sll zero_reg, zero_reg, <type> (referenced as
  // nop(type)). These instructions are generated to mark special location in
  // the code, like some special IC code.
  static inline bool IsMarkedCode(Instr instr, int type) {
    DCHECK((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER));
    return IsNop(instr, type);
  }


  static inline int GetCodeMarker(Instr instr) {
    uint32_t opcode = ((instr & kOpcodeMask));
    uint32_t rt = ((instr & kRtFieldMask) >> kRtShift);
    uint32_t rs = ((instr & kRsFieldMask) >> kRsShift);
    uint32_t sa = ((instr & kSaFieldMask) >> kSaShift);

    // Return <n> if we have a sll zero_reg, zero_reg, n
    // else return -1.
    bool sllzz = (opcode == SLL &&
                  rt == static_cast<uint32_t>(ToNumber(zero_reg)) &&
                  rs == static_cast<uint32_t>(ToNumber(zero_reg)));
    int type =
        (sllzz && FIRST_IC_MARKER <= sa && sa < LAST_CODE_MARKER) ? sa : -1;
    DCHECK((type == -1) ||
           ((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER)));
    return type;
  }



  // ---------------------------------------------------------------------------
  // Allocation support.

  // Allocate an object in new space or old space. The object_size is
  // specified either in bytes or in words if the allocation flag SIZE_IN_WORDS
  // is passed. If the space is exhausted control continues at the gc_required
  // label. The allocated object is returned in result. If the flag
  // tag_allocated_object is true the result is tagged as as a heap object.
  // All registers are clobbered also when control continues at the gc_required
  // label.
  void Allocate(int object_size,
                Register result,
                Register scratch1,
                Register scratch2,
                Label* gc_required,
                AllocationFlags flags);

  void Allocate(Register object_size,
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
  void AllocateOneByteString(Register result, Register length,
                             Register scratch1, Register scratch2,
                             Register scratch3, Label* gc_required);
  void AllocateTwoByteConsString(Register result,
                                 Register length,
                                 Register scratch1,
                                 Register scratch2,
                                 Label* gc_required);
  void AllocateOneByteConsString(Register result, Register length,
                                 Register scratch1, Register scratch2,
                                 Label* gc_required);
  void AllocateTwoByteSlicedString(Register result,
                                   Register length,
                                   Register scratch1,
                                   Register scratch2,
                                   Label* gc_required);
  void AllocateOneByteSlicedString(Register result, Register length,
                                   Register scratch1, Register scratch2,
                                   Label* gc_required);

  // Allocates a heap number or jumps to the gc_required label if the young
  // space is full and a scavenge is needed. All registers are clobbered also
  // when control continues at the gc_required label.
  void AllocateHeapNumber(Register result,
                          Register scratch1,
                          Register scratch2,
                          Register heap_number_map,
                          Label* gc_required,
                          TaggingMode tagging_mode = TAG_RESULT,
                          MutableMode mode = IMMUTABLE);

  void AllocateHeapNumberWithValue(Register result,
                                   FPURegister value,
                                   Register scratch1,
                                   Register scratch2,
                                   Label* gc_required);

  // ---------------------------------------------------------------------------
  // Instruction macros.

#define DEFINE_INSTRUCTION(instr)                                              \
  void instr(Register rd, Register rs, const Operand& rt);                     \
  void instr(Register rd, Register rs, Register rt) {                          \
    instr(rd, rs, Operand(rt));                                                \
  }                                                                            \
  void instr(Register rs, Register rt, int32_t j) {                            \
    instr(rs, rt, Operand(j));                                                 \
  }

#define DEFINE_INSTRUCTION2(instr)                                             \
  void instr(Register rs, const Operand& rt);                                  \
  void instr(Register rs, Register rt) {                                       \
    instr(rs, Operand(rt));                                                    \
  }                                                                            \
  void instr(Register rs, int32_t j) {                                         \
    instr(rs, Operand(j));                                                     \
  }

  DEFINE_INSTRUCTION(Addu);
  DEFINE_INSTRUCTION(Daddu);
  DEFINE_INSTRUCTION(Div);
  DEFINE_INSTRUCTION(Divu);
  DEFINE_INSTRUCTION(Ddivu);
  DEFINE_INSTRUCTION(Mod);
  DEFINE_INSTRUCTION(Modu);
  DEFINE_INSTRUCTION(Ddiv);
  DEFINE_INSTRUCTION(Subu);
  DEFINE_INSTRUCTION(Dsubu);
  DEFINE_INSTRUCTION(Dmod);
  DEFINE_INSTRUCTION(Dmodu);
  DEFINE_INSTRUCTION(Mul);
  DEFINE_INSTRUCTION(Mulh);
  DEFINE_INSTRUCTION(Mulhu);
  DEFINE_INSTRUCTION(Dmul);
  DEFINE_INSTRUCTION(Dmulh);
  DEFINE_INSTRUCTION2(Mult);
  DEFINE_INSTRUCTION2(Dmult);
  DEFINE_INSTRUCTION2(Multu);
  DEFINE_INSTRUCTION2(Dmultu);
  DEFINE_INSTRUCTION2(Div);
  DEFINE_INSTRUCTION2(Ddiv);
  DEFINE_INSTRUCTION2(Divu);
  DEFINE_INSTRUCTION2(Ddivu);

  DEFINE_INSTRUCTION(And);
  DEFINE_INSTRUCTION(Or);
  DEFINE_INSTRUCTION(Xor);
  DEFINE_INSTRUCTION(Nor);
  DEFINE_INSTRUCTION2(Neg);

  DEFINE_INSTRUCTION(Slt);
  DEFINE_INSTRUCTION(Sltu);

  // MIPS32 R2 instruction macro.
  DEFINE_INSTRUCTION(Ror);
  DEFINE_INSTRUCTION(Dror);

#undef DEFINE_INSTRUCTION
#undef DEFINE_INSTRUCTION2

  void Pref(int32_t hint, const MemOperand& rs);


  // ---------------------------------------------------------------------------
  // Pseudo-instructions.

  void mov(Register rd, Register rt) { or_(rd, rt, zero_reg); }

  void Ulw(Register rd, const MemOperand& rs);
  void Usw(Register rd, const MemOperand& rs);
  void Uld(Register rd, const MemOperand& rs, Register scratch = at);
  void Usd(Register rd, const MemOperand& rs, Register scratch = at);

  // Load int32 in the rd register.
  void li(Register rd, Operand j, LiFlags mode = OPTIMIZE_SIZE);
  inline void li(Register rd, int64_t j, LiFlags mode = OPTIMIZE_SIZE) {
    li(rd, Operand(j), mode);
  }
  void li(Register dst, Handle<Object> value, LiFlags mode = OPTIMIZE_SIZE);

  // Push multiple registers on the stack.
  // Registers are saved in numerical order, with higher numbered registers
  // saved in higher memory addresses.
  void MultiPush(RegList regs);
  void MultiPushReversed(RegList regs);

  void MultiPushFPU(RegList regs);
  void MultiPushReversedFPU(RegList regs);

  void push(Register src) {
    Daddu(sp, sp, Operand(-kPointerSize));
    sd(src, MemOperand(sp, 0));
  }
  void Push(Register src) { push(src); }

  // Push a handle.
  void Push(Handle<Object> handle);
  void Push(Smi* smi) { Push(Handle<Smi>(smi, isolate())); }

  // Push two registers. Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2) {
    Dsubu(sp, sp, Operand(2 * kPointerSize));
    sd(src1, MemOperand(sp, 1 * kPointerSize));
    sd(src2, MemOperand(sp, 0 * kPointerSize));
  }

  // Push three registers. Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3) {
    Dsubu(sp, sp, Operand(3 * kPointerSize));
    sd(src1, MemOperand(sp, 2 * kPointerSize));
    sd(src2, MemOperand(sp, 1 * kPointerSize));
    sd(src3, MemOperand(sp, 0 * kPointerSize));
  }

  // Push four registers. Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Register src4) {
    Dsubu(sp, sp, Operand(4 * kPointerSize));
    sd(src1, MemOperand(sp, 3 * kPointerSize));
    sd(src2, MemOperand(sp, 2 * kPointerSize));
    sd(src3, MemOperand(sp, 1 * kPointerSize));
    sd(src4, MemOperand(sp, 0 * kPointerSize));
  }

  void Push(Register src, Condition cond, Register tst1, Register tst2) {
    // Since we don't have conditional execution we use a Branch.
    Branch(3, cond, tst1, Operand(tst2));
    Dsubu(sp, sp, Operand(kPointerSize));
    sd(src, MemOperand(sp, 0));
  }

  void PushRegisterAsTwoSmis(Register src, Register scratch = at);
  void PopRegisterAsTwoSmis(Register dst, Register scratch = at);

  // Pops multiple values from the stack and load them in the
  // registers specified in regs. Pop order is the opposite as in MultiPush.
  void MultiPop(RegList regs);
  void MultiPopReversed(RegList regs);

  void MultiPopFPU(RegList regs);
  void MultiPopReversedFPU(RegList regs);

  void pop(Register dst) {
    ld(dst, MemOperand(sp, 0));
    Daddu(sp, sp, Operand(kPointerSize));
  }
  void Pop(Register dst) { pop(dst); }

  // Pop two registers. Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2) {
    DCHECK(!src1.is(src2));
    ld(src2, MemOperand(sp, 0 * kPointerSize));
    ld(src1, MemOperand(sp, 1 * kPointerSize));
    Daddu(sp, sp, 2 * kPointerSize);
  }

  // Pop three registers. Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3) {
    ld(src3, MemOperand(sp, 0 * kPointerSize));
    ld(src2, MemOperand(sp, 1 * kPointerSize));
    ld(src1, MemOperand(sp, 2 * kPointerSize));
    Daddu(sp, sp, 3 * kPointerSize);
  }

  void Pop(uint32_t count = 1) {
    Daddu(sp, sp, Operand(count * kPointerSize));
  }

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
  void FlushICache(Register address, unsigned instructions);

  // MIPS64 R2 instruction macro.
  void Ins(Register rt, Register rs, uint16_t pos, uint16_t size);
  void Ext(Register rt, Register rs, uint16_t pos, uint16_t size);
  void Dext(Register rt, Register rs, uint16_t pos, uint16_t size);

  // ---------------------------------------------------------------------------
  // FPU macros. These do not handle special cases like NaN or +- inf.

  // Convert unsigned word to double.
  void Cvt_d_uw(FPURegister fd, FPURegister fs, FPURegister scratch);
  void Cvt_d_uw(FPURegister fd, Register rs, FPURegister scratch);

  // Convert double to unsigned long.
  void Trunc_l_ud(FPURegister fd, FPURegister fs, FPURegister scratch);

  void Trunc_l_d(FPURegister fd, FPURegister fs);
  void Round_l_d(FPURegister fd, FPURegister fs);
  void Floor_l_d(FPURegister fd, FPURegister fs);
  void Ceil_l_d(FPURegister fd, FPURegister fs);

  // Convert double to unsigned word.
  void Trunc_uw_d(FPURegister fd, FPURegister fs, FPURegister scratch);
  void Trunc_uw_d(FPURegister fd, Register rs, FPURegister scratch);

  void Trunc_w_d(FPURegister fd, FPURegister fs);
  void Round_w_d(FPURegister fd, FPURegister fs);
  void Floor_w_d(FPURegister fd, FPURegister fs);
  void Ceil_w_d(FPURegister fd, FPURegister fs);

  void Madd_d(FPURegister fd,
              FPURegister fr,
              FPURegister fs,
              FPURegister ft,
              FPURegister scratch);

  // Wrapper functions for the different cmp/branch types.
  inline void BranchF32(Label* target, Label* nan, Condition cc,
                        FPURegister cmp1, FPURegister cmp2,
                        BranchDelaySlot bd = PROTECT) {
    BranchFCommon(S, target, nan, cc, cmp1, cmp2, bd);
  }

  inline void BranchF64(Label* target, Label* nan, Condition cc,
                        FPURegister cmp1, FPURegister cmp2,
                        BranchDelaySlot bd = PROTECT) {
    BranchFCommon(D, target, nan, cc, cmp1, cmp2, bd);
  }

  // Alternate (inline) version for better readability with USE_DELAY_SLOT.
  inline void BranchF64(BranchDelaySlot bd, Label* target, Label* nan,
                        Condition cc, FPURegister cmp1, FPURegister cmp2) {
    BranchF64(target, nan, cc, cmp1, cmp2, bd);
  }

  inline void BranchF32(BranchDelaySlot bd, Label* target, Label* nan,
                        Condition cc, FPURegister cmp1, FPURegister cmp2) {
    BranchF32(target, nan, cc, cmp1, cmp2, bd);
  }

  // Alias functions for backward compatibility.
  inline void BranchF(Label* target, Label* nan, Condition cc, FPURegister cmp1,
                      FPURegister cmp2, BranchDelaySlot bd = PROTECT) {
    BranchF64(target, nan, cc, cmp1, cmp2, bd);
  }

  inline void BranchF(BranchDelaySlot bd, Label* target, Label* nan,
                      Condition cc, FPURegister cmp1, FPURegister cmp2) {
    BranchF64(bd, target, nan, cc, cmp1, cmp2);
  }

  // Truncates a double using a specific rounding mode, and writes the value
  // to the result register.
  // The except_flag will contain any exceptions caused by the instruction.
  // If check_inexact is kDontCheckForInexactConversion, then the inexact
  // exception is masked.
  void EmitFPUTruncate(FPURoundingMode rounding_mode,
                       Register result,
                       DoubleRegister double_input,
                       Register scratch,
                       DoubleRegister double_scratch,
                       Register except_flag,
                       CheckForInexactConversion check_inexact
                           = kDontCheckForInexactConversion);

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. Goes to 'done' if it
  // succeeds, otherwise falls through if result is saturated. On return
  // 'result' either holds answer, or is clobbered on fall through.
  //
  // Only public for the test code in test-code-stubs-arm.cc.
  void TryInlineTruncateDoubleToI(Register result,
                                  DoubleRegister input,
                                  Label* done);

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32.
  // Exits with 'result' holding the answer.
  void TruncateDoubleToI(Register result, DoubleRegister double_input);

  // Performs a truncating conversion of a heap number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. 'result' and 'input'
  // must be different registers. Exits with 'result' holding the answer.
  void TruncateHeapNumberToI(Register result, Register object);

  // Converts the smi or heap number in object to an int32 using the rules
  // for ToInt32 as described in ECMAScript 9.5.: the value is truncated
  // and brought into the range -2^31 .. +2^31 - 1. 'result' and 'input' must be
  // different registers.
  void TruncateNumberToI(Register object,
                         Register result,
                         Register heap_number_map,
                         Register scratch,
                         Label* not_int32);

  // Loads the number from object into dst register.
  // If |object| is neither smi nor heap number, |not_number| is jumped to
  // with |object| still intact.
  void LoadNumber(Register object,
                  FPURegister dst,
                  Register heap_number_map,
                  Register scratch,
                  Label* not_number);

  // Loads the number from object into double_dst in the double format.
  // Control will jump to not_int32 if the value cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be loaded.
  void LoadNumberAsInt32Double(Register object,
                               DoubleRegister double_dst,
                               Register heap_number_map,
                               Register scratch1,
                               Register scratch2,
                               FPURegister double_scratch,
                               Label* not_int32);

  // Loads the number from object into dst as a 32-bit integer.
  // Control will jump to not_int32 if the object cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be converted.
  void LoadNumberAsInt32(Register object,
                         Register dst,
                         Register heap_number_map,
                         Register scratch1,
                         Register scratch2,
                         FPURegister double_scratch0,
                         FPURegister double_scratch1,
                         Label* not_int32);

  // Enter exit frame.
  // argc - argument count to be dropped by LeaveExitFrame.
  // save_doubles - saves FPU registers on stack, currently disabled.
  // stack_space - extra stack space.
  void EnterExitFrame(bool save_doubles,
                      int stack_space = 0);

  // Leave the current exit frame.
  void LeaveExitFrame(bool save_doubles, Register arg_count,
                      bool restore_context, bool do_return = NO_EMIT_RETURN,
                      bool argument_count_is_length = false);

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();

  // Make sure the stack is aligned. Only emits code in debug mode.
  void AssertStackIsAligned();

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

  void LoadGlobalFunction(int index, Register function);

  // Load the initial map from the global function. The registers
  // function and map can be the same, function is then overwritten.
  void LoadGlobalFunctionInitialMap(Register function,
                                    Register map,
                                    Register scratch);

  void InitializeRootRegister() {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    li(kRootRegister, Operand(roots_array_start));
  }

  // -------------------------------------------------------------------------
  // JavaScript invokes.

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeCode(Register code,
                  const ParameterCount& expected,
                  const ParameterCount& actual,
                  InvokeFlag flag,
                  const CallWrapper& call_wrapper);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Register function,
                      const ParameterCount& expected,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);


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

  void IsObjectNameType(Register object,
                        Register scratch,
                        Label* fail);

  // -------------------------------------------------------------------------
  // Debugger Support.

  void DebugBreak();

  // -------------------------------------------------------------------------
  // Exception handling.

  // Push a new stack handler and link into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  // Must preserve the result register.
  void PopStackHandler();

  // Copies a fixed number of fields of heap objects from src to dst.
  void CopyFields(Register dst, Register src, RegList temps, int field_count);

  // Copies a number of bytes from src to dst. All registers are clobbered. On
  // exit src and dst will point to the place just after where the last byte was
  // read or written and length will be zero.
  void CopyBytes(Register src,
                 Register dst,
                 Register length,
                 Register scratch);

  // Initialize fields with filler values.  Fields starting at |start_offset|
  // not including end_offset are overwritten with the value in |filler|.  At
  // the end the loop, |start_offset| takes the value of |end_offset|.
  void InitializeFieldsWithFiller(Register start_offset,
                                  Register end_offset,
                                  Register filler);

  // -------------------------------------------------------------------------
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
  void TryGetFunctionPrototype(Register function,
                               Register result,
                               Register scratch,
                               Label* miss,
                               bool miss_on_bound_function = false);

  void GetObjectType(Register function,
                     Register map,
                     Register type_reg);

  // Check if a map for a JSObject indicates that the object has fast elements.
  // Jump to the specified label if it does not.
  void CheckFastElements(Register map,
                         Register scratch,
                         Label* fail);

  // Check if a map for a JSObject indicates that the object can have both smi
  // and HeapObject elements.  Jump to the specified label if it does not.
  void CheckFastObjectElements(Register map,
                               Register scratch,
                               Label* fail);

  // Check if a map for a JSObject indicates that the object has fast smi only
  // elements.  Jump to the specified label if it does not.
  void CheckFastSmiElements(Register map,
                            Register scratch,
                            Label* fail);

  // Check to see if maybe_number can be stored as a double in
  // FastDoubleElements. If it can, store it at the index specified by key in
  // the FastDoubleElements array elements. Otherwise jump to fail.
  void StoreNumberToDoubleElements(Register value_reg,
                                   Register key_reg,
                                   Register elements_reg,
                                   Register scratch1,
                                   Register scratch2,
                                   Label* fail,
                                   int elements_offset = 0);

  // Compare an object's map with the specified map and its transitioned
  // elements maps if mode is ALLOW_ELEMENT_TRANSITION_MAPS. Jumps to
  // "branch_to" if the result of the comparison is "cond". If multiple map
  // compares are required, the compare sequences branches to early_success.
  void CompareMapAndBranch(Register obj,
                           Register scratch,
                           Handle<Map> map,
                           Label* early_success,
                           Condition cond,
                           Label* branch_to);

  // As above, but the map of the object is already loaded into the register
  // which is preserved by the code generated.
  void CompareMapAndBranch(Register obj_map,
                           Handle<Map> map,
                           Label* early_success,
                           Condition cond,
                           Label* branch_to);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specificed map.
  void CheckMap(Register obj,
                Register scratch,
                Handle<Map> map,
                Label* fail,
                SmiCheckType smi_check_type);


  void CheckMap(Register obj,
                Register scratch,
                Heap::RootListIndex index,
                Label* fail,
                SmiCheckType smi_check_type);

  // Check if the map of an object is equal to a specified weak map and branch
  // to a specified target if equal. Skip the smi check if not required
  // (object is known to be a heap object)
  void DispatchWeakMap(Register obj, Register scratch1, Register scratch2,
                       Handle<WeakCell> cell, Handle<Code> success,
                       SmiCheckType smi_check_type);

  // If the value is a NaN, canonicalize the value else, do nothing.
  void FPUCanonicalizeNaN(const DoubleRegister dst, const DoubleRegister src);


  // Get value of the weak cell.
  void GetWeakValue(Register value, Handle<WeakCell> cell);

  // Load the value of the weak cell in the value register. Branch to the
  // given miss label is the weak cell was cleared.
  void LoadWeakValue(Register value, Handle<WeakCell> cell, Label* miss);

  // Load and check the instance type of an object for being a string.
  // Loads the type into the second argument register.
  // Returns a condition that will be enabled if the object was a string.
  Condition IsObjectStringType(Register obj,
                               Register type,
                               Register result) {
    ld(type, FieldMemOperand(obj, HeapObject::kMapOffset));
    lbu(type, FieldMemOperand(type, Map::kInstanceTypeOffset));
    And(type, type, Operand(kIsNotStringMask));
    DCHECK_EQ(0u, kStringTag);
    return eq;
  }


  // Picks out an array index from the hash field.
  // Register use:
  //   hash - holds the index's hash. Clobbered.
  //   index - holds the overwritten index on exit.
  void IndexFromHash(Register hash, Register index);

  // Get the number of least significant bits from a register.
  void GetLeastBitsFromSmi(Register dst, Register src, int num_least_bits);
  void GetLeastBitsFromInt32(Register dst, Register src, int mun_least_bits);

  // Load the value of a number object into a FPU double register. If the
  // object is not a number a jump to the label not_number is performed
  // and the FPU double register is unchanged.
  void ObjectToDoubleFPURegister(
      Register object,
      FPURegister value,
      Register scratch1,
      Register scratch2,
      Register heap_number_map,
      Label* not_number,
      ObjectToDoubleFlags flags = NO_OBJECT_TO_DOUBLE_FLAGS);

  // Load the value of a smi object into a FPU double register. The register
  // scratch1 can be the same register as smi in which case smi will hold the
  // untagged value afterwards.
  void SmiToDoubleFPURegister(Register smi,
                              FPURegister value,
                              Register scratch1);

  // -------------------------------------------------------------------------
  // Overflow handling functions.
  // Usage: first call the appropriate arithmetic function, then call one of the
  // jump functions with the overflow_dst register as the second parameter.

  void AdduAndCheckForOverflow(Register dst,
                               Register left,
                               Register right,
                               Register overflow_dst,
                               Register scratch = at);

  void AdduAndCheckForOverflow(Register dst, Register left,
                               const Operand& right, Register overflow_dst,
                               Register scratch = at);

  void SubuAndCheckForOverflow(Register dst,
                               Register left,
                               Register right,
                               Register overflow_dst,
                               Register scratch = at);

  void SubuAndCheckForOverflow(Register dst, Register left,
                               const Operand& right, Register overflow_dst,
                               Register scratch = at);

  void BranchOnOverflow(Label* label,
                        Register overflow_check,
                        BranchDelaySlot bd = PROTECT) {
    Branch(label, lt, overflow_check, Operand(zero_reg), bd);
  }

  void BranchOnNoOverflow(Label* label,
                          Register overflow_check,
                          BranchDelaySlot bd = PROTECT) {
    Branch(label, ge, overflow_check, Operand(zero_reg), bd);
  }

  void RetOnOverflow(Register overflow_check, BranchDelaySlot bd = PROTECT) {
    Ret(lt, overflow_check, Operand(zero_reg), bd);
  }

  void RetOnNoOverflow(Register overflow_check, BranchDelaySlot bd = PROTECT) {
    Ret(ge, overflow_check, Operand(zero_reg), bd);
  }

  // -------------------------------------------------------------------------
  // Runtime calls.

  // See comments at the beginning of CEntryStub::Generate.
  inline void PrepareCEntryArgs(int num_args) { li(a0, num_args); }

  inline void PrepareCEntryFunction(const ExternalReference& ref) {
    li(a1, Operand(ref));
  }

#define COND_ARGS Condition cond = al, Register rs = zero_reg, \
const Operand& rt = Operand(zero_reg), BranchDelaySlot bd = PROTECT

  // Call a code stub.
  void CallStub(CodeStub* stub,
                TypeFeedbackId ast_id = TypeFeedbackId::None(),
                COND_ARGS);

  // Tail call a code stub (jump).
  void TailCallStub(CodeStub* stub, COND_ARGS);

#undef COND_ARGS

  void CallJSExitStub(CodeStub* stub);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f,
                   int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);
  void CallRuntimeSaveDoubles(Runtime::FunctionId id) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, function->nargs, kSaveFPRegs);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId id,
                   int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    CallRuntime(Runtime::FunctionForId(id), num_arguments, save_doubles);
  }

  // Convenience function: call an external reference.
  void CallExternalReference(const ExternalReference& ext,
                             int num_arguments,
                             BranchDelaySlot bd = PROTECT);

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

  int CalculateStackPassedWords(int num_reg_arguments,
                                int num_double_arguments);

  // Before calling a C-function from generated code, align arguments on stack
  // and add space for the four mips argument slots.
  // After aligning the frame, non-register arguments must be stored on the
  // stack, after the argument-slots using helper: CFunctionArgumentOperand().
  // The argument count assumes all arguments are word sized.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  void PrepareCallCFunction(int num_reg_arguments,
                            int num_double_registers,
                            Register scratch);
  void PrepareCallCFunction(int num_reg_arguments,
                            Register scratch);

  // Arguments 1-4 are placed in registers a0 thru a3 respectively.
  // Arguments 5..n are stored to stack using following:
  //  sw(a4, CFunctionArgumentOperand(5));

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);
  void CallCFunction(ExternalReference function,
                     int num_reg_arguments,
                     int num_double_arguments);
  void CallCFunction(Register function,
                     int num_reg_arguments,
                     int num_double_arguments);
  void MovFromFloatResult(DoubleRegister dst);
  void MovFromFloatParameter(DoubleRegister dst);

  // There are two ways of passing double arguments on MIPS, depending on
  // whether soft or hard floating point ABI is used. These functions
  // abstract parameter passing for the three different ways we call
  // C functions from generated code.
  void MovToFloatParameter(DoubleRegister src);
  void MovToFloatParameters(DoubleRegister src1, DoubleRegister src2);
  void MovToFloatResult(DoubleRegister src);

  // Jump to the builtin routine.
  void JumpToExternalReference(const ExternalReference& builtin,
                               BranchDelaySlot bd = PROTECT);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id,
                     InvokeFlag flag,
                     const CallWrapper& call_wrapper = NullCallWrapper());

  // Store the code object for the given builtin in the target register and
  // setup the function in a1.
  void GetBuiltinEntry(Register target, Builtins::JavaScript id);

  // Store the function for the given builtin in the target register.
  void GetBuiltinFunction(Register target, Builtins::JavaScript id);

  struct Unresolved {
    int pc;
    uint32_t flags;  // See Bootstrapper::FixupFlags decoders/encoders.
    const char* name;
  };

  Handle<Object> CodeObject() {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }

  // Emit code for a truncating division by a constant. The dividend register is
  // unchanged and at gets clobbered. Dividend and result must be different.
  void TruncatingDiv(Register result, Register dividend, int32_t divisor);

  // -------------------------------------------------------------------------
  // StatsCounter support.

  void SetCounter(StatsCounter* counter, int value,
                  Register scratch1, Register scratch2);
  void IncrementCounter(StatsCounter* counter, int value,
                        Register scratch1, Register scratch2);
  void DecrementCounter(StatsCounter* counter, int value,
                        Register scratch1, Register scratch2);


  // -------------------------------------------------------------------------
  // Debugging.

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, BailoutReason reason, Register rs, Operand rt);
  void AssertFastElements(Register elements);

  // Like Assert(), but always enabled.
  void Check(Condition cc, BailoutReason reason, Register rs, Operand rt);

  // Print a message to stdout and abort execution.
  void Abort(BailoutReason msg);

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() { return has_frame_; }
  inline bool AllowThisStubCall(CodeStub* stub);

  // ---------------------------------------------------------------------------
  // Number utilities.

  // Check whether the value of reg is a power of two and not zero. If not
  // control continues at the label not_power_of_two. If reg is a power of two
  // the register scratch contains the value of (reg - 1) when control falls
  // through.
  void JumpIfNotPowerOfTwoOrZero(Register reg,
                                 Register scratch,
                                 Label* not_power_of_two_or_zero);

  // -------------------------------------------------------------------------
  // Smi utilities.

  // Test for overflow < 0: use BranchOnOverflow() or BranchOnNoOverflow().
  void SmiTagCheckOverflow(Register reg, Register overflow);
  void SmiTagCheckOverflow(Register dst, Register src, Register overflow);

  void SmiTag(Register dst, Register src) {
    STATIC_ASSERT(kSmiTag == 0);
    if (SmiValuesAre32Bits()) {
      STATIC_ASSERT(kSmiShift == 32);
      dsll32(dst, src, 0);
    } else {
      Addu(dst, src, src);
    }
  }

  void SmiTag(Register reg) {
    SmiTag(reg, reg);
  }

  // Try to convert int32 to smi. If the value is to large, preserve
  // the original value and jump to not_a_smi. Destroys scratch and
  // sets flags.
  void TrySmiTag(Register reg, Register scratch, Label* not_a_smi) {
    TrySmiTag(reg, reg, scratch, not_a_smi);
  }

  void TrySmiTag(Register dst,
                 Register src,
                 Register scratch,
                 Label* not_a_smi) {
    if (SmiValuesAre32Bits()) {
      SmiTag(dst, src);
    } else {
      SmiTagCheckOverflow(at, src, scratch);
      BranchOnOverflow(not_a_smi, scratch);
      mov(dst, at);
    }
  }

  void SmiUntag(Register dst, Register src) {
    if (SmiValuesAre32Bits()) {
      STATIC_ASSERT(kSmiShift == 32);
      dsra32(dst, src, 0);
    } else {
      sra(dst, src, kSmiTagSize);
    }
  }

  void SmiUntag(Register reg) {
    SmiUntag(reg, reg);
  }

  // Left-shifted from int32 equivalent of Smi.
  void SmiScale(Register dst, Register src, int scale) {
    if (SmiValuesAre32Bits()) {
      // The int portion is upper 32-bits of 64-bit word.
      dsra(dst, src, kSmiShift - scale);
    } else {
      DCHECK(scale >= kSmiTagSize);
      sll(dst, src, scale - kSmiTagSize);
    }
  }

  // Combine load with untagging or scaling.
  void SmiLoadUntag(Register dst, MemOperand src);

  void SmiLoadScale(Register dst, MemOperand src, int scale);

  // Returns 2 values: the Smi and a scaled version of the int within the Smi.
  void SmiLoadWithScale(Register d_smi,
                        Register d_scaled,
                        MemOperand src,
                        int scale);

  // Returns 2 values: the untagged Smi (int32) and scaled version of that int.
  void SmiLoadUntagWithScale(Register d_int,
                             Register d_scaled,
                             MemOperand src,
                             int scale);


  // Test if the register contains a smi.
  inline void SmiTst(Register value, Register scratch) {
    And(scratch, value, Operand(kSmiTagMask));
  }
  inline void NonNegativeSmiTst(Register value, Register scratch) {
    And(scratch, value, Operand(kSmiTagMask | kSmiSignMask));
  }

  // Untag the source value into destination and jump if source is a smi.
  // Source and destination can be the same register.
  void UntagAndJumpIfSmi(Register dst, Register src, Label* smi_case);

  // Untag the source value into destination and jump if source is not a smi.
  // Source and destination can be the same register.
  void UntagAndJumpIfNotSmi(Register dst, Register src, Label* non_smi_case);

  // Jump the register contains a smi.
  void JumpIfSmi(Register value,
                 Label* smi_label,
                 Register scratch = at,
                 BranchDelaySlot bd = PROTECT);

  // Jump if the register contains a non-smi.
  void JumpIfNotSmi(Register value,
                    Label* not_smi_label,
                    Register scratch = at,
                    BranchDelaySlot bd = PROTECT);

  // Jump if either of the registers contain a non-smi.
  void JumpIfNotBothSmi(Register reg1, Register reg2, Label* on_not_both_smi);
  // Jump if either of the registers contain a smi.
  void JumpIfEitherSmi(Register reg1, Register reg2, Label* on_either_smi);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);
  void AssertSmi(Register object);

  // Abort execution if argument is not a string, enabled via --debug-code.
  void AssertString(Register object);

  // Abort execution if argument is not a name, enabled via --debug-code.
  void AssertName(Register object);

  // Abort execution if argument is not undefined or an AllocationSite, enabled
  // via --debug-code.
  void AssertUndefinedOrAllocationSite(Register object, Register scratch);

  // Abort execution if reg is not the root value with the given index,
  // enabled via --debug-code.
  void AssertIsRoot(Register reg, Heap::RootListIndex index);

  // ---------------------------------------------------------------------------
  // HeapNumber utilities.

  void JumpIfNotHeapNumber(Register object,
                           Register heap_number_map,
                           Register scratch,
                           Label* on_not_heap_number);

  // -------------------------------------------------------------------------
  // String utilities.

  // Generate code to do a lookup in the number string cache. If the number in
  // the register object is found in the cache the generated code falls through
  // with the result in the result register. The object and the result register
  // can be the same. If the number is not found in the cache the code jumps to
  // the label not_found with only the content of register object unchanged.
  void LookupNumberStringCache(Register object,
                               Register result,
                               Register scratch1,
                               Register scratch2,
                               Register scratch3,
                               Label* not_found);

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

  void EmitSeqStringSetCharCheck(Register string,
                                 Register index,
                                 Register value,
                                 Register scratch,
                                 uint32_t encoding_mask);

  // Checks if both objects are sequential one-byte strings and jumps to label
  // if either is not. Assumes that neither object is a smi.
  void JumpIfNonSmisNotBothSequentialOneByteStrings(Register first,
                                                    Register second,
                                                    Register scratch1,
                                                    Register scratch2,
                                                    Label* failure);

  // Checks if both objects are sequential one-byte strings and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialOneByteStrings(Register first, Register second,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* not_flat_one_byte_strings);

  void ClampUint8(Register output_reg, Register input_reg);

  void ClampDoubleToUint8(Register result_reg,
                          DoubleRegister input_reg,
                          DoubleRegister temp_double_reg);


  void LoadInstanceDescriptors(Register map, Register descriptors);
  void EnumLength(Register dst, Register map);
  void NumberOfOwnDescriptors(Register dst, Register map);
  void LoadAccessor(Register dst, Register holder, int accessor_index,
                    AccessorComponent accessor);

  template<typename Field>
  void DecodeField(Register dst, Register src) {
    Ext(dst, src, Field::kShift, Field::kSize);
  }

  template<typename Field>
  void DecodeField(Register reg) {
    DecodeField<Field>(reg, reg);
  }

  template<typename Field>
  void DecodeFieldToSmi(Register dst, Register src) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> shift;
    dsrl(dst, src, shift);
    And(dst, dst, Operand(mask));
    dsll32(dst, dst, 0);
  }

  template<typename Field>
  void DecodeFieldToSmi(Register reg) {
    DecodeField<Field>(reg, reg);
  }
  // Generates function and stub prologue code.
  void StubPrologue();
  void Prologue(bool code_pre_aging);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void EnterFrame(StackFrame::Type type, bool load_constant_pool_pointer_reg);
  void LeaveFrame(StackFrame::Type type);

  // Patch the relocated value (lui/ori pair).
  void PatchRelocatedValue(Register li_location,
                           Register scratch,
                           Register new_value);
  // Get the relocatad value (loaded data) from the lui/ori pair.
  void GetRelocatedValue(Register li_location,
                         Register value,
                         Register scratch);

  // Expects object in a0 and returns map with validated enum cache
  // in a0.  Assumes that any other register can be used as a scratch.
  void CheckEnumCache(Register null_value, Label* call_runtime);

  // AllocationMemento support. Arrays may have an associated
  // AllocationMemento object that can be checked for in order to pretransition
  // to another type.
  // On entry, receiver_reg should point to the array object.
  // scratch_reg gets clobbered.
  // If allocation info is present, jump to allocation_memento_present.
  void TestJSArrayForAllocationMemento(
      Register receiver_reg,
      Register scratch_reg,
      Label* no_memento_found,
      Condition cond = al,
      Label* allocation_memento_present = NULL);

  void JumpIfJSArrayHasAllocationMemento(Register receiver_reg,
                                         Register scratch_reg,
                                         Label* memento_found) {
    Label no_memento_found;
    TestJSArrayForAllocationMemento(receiver_reg, scratch_reg,
                                    &no_memento_found, eq, memento_found);
    bind(&no_memento_found);
  }

  // Jumps to found label if a prototype map has dictionary elements.
  void JumpIfDictionaryInPrototypeChain(Register object, Register scratch0,
                                        Register scratch1, Label* found);

 private:
  void CallCFunctionHelper(Register function,
                           int num_reg_arguments,
                           int num_double_arguments);

  void BranchAndLinkShort(int16_t offset, BranchDelaySlot bdslot = PROTECT);
  void BranchAndLinkShort(int16_t offset, Condition cond, Register rs,
                          const Operand& rt,
                          BranchDelaySlot bdslot = PROTECT);
  void BranchAndLinkShort(Label* L, BranchDelaySlot bdslot = PROTECT);
  void BranchAndLinkShort(Label* L, Condition cond, Register rs,
                          const Operand& rt,
                          BranchDelaySlot bdslot = PROTECT);
  void Jr(Label* L, BranchDelaySlot bdslot);
  void Jalr(Label* L, BranchDelaySlot bdslot);

  // Common implementation of BranchF functions for the different formats.
  void BranchFCommon(SecondaryField sizeField, Label* target, Label* nan,
                     Condition cc, FPURegister cmp1, FPURegister cmp2,
                     BranchDelaySlot bd = PROTECT);

  void BranchShortF(SecondaryField sizeField, Label* target, Condition cc,
                    FPURegister cmp1, FPURegister cmp2,
                    BranchDelaySlot bd = PROTECT);


  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Handle<Code> code_constant,
                      Register code_reg,
                      Label* done,
                      bool* definitely_mismatches,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  // Get the code for the given builtin. Returns if able to resolve
  // the function in the 'resolved' flag.
  Handle<Code> ResolveBuiltin(Builtins::JavaScript id, bool* resolved);

  void InitializeNewString(Register string,
                           Register length,
                           Heap::RootListIndex map_index,
                           Register scratch1,
                           Register scratch2);

  // Helper for implementing JumpIfNotInNewSpace and JumpIfInNewSpace.
  void InNewSpace(Register object,
                  Register scratch,
                  Condition cond,  // eq for new space, ne otherwise.
                  Label* branch);

  // Helper for finding the mark bits for an address.  Afterwards, the
  // bitmap register points at the word with the mark bits and the mask
  // the position of the first bit.  Leaves addr_reg unchanged.
  inline void GetMarkBits(Register addr_reg,
                          Register bitmap_reg,
                          Register mask_reg);

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);
  MemOperand SafepointRegisterSlot(Register reg);
  MemOperand SafepointRegistersAndDoublesSlot(Register reg);

  bool generating_stub_;
  bool has_frame_;
  bool has_double_zero_reg_set_;
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
  enum FlushICache {
    FLUSH,
    DONT_FLUSH
  };

  CodePatcher(byte* address,
              int instructions,
              FlushICache flush_cache = FLUSH);
  virtual ~CodePatcher();

  // Macro assembler to emit code.
  MacroAssembler* masm() { return &masm_; }

  // Emit an instruction directly.
  void Emit(Instr instr);

  // Emit an address directly.
  void Emit(Address addr);

  // Change the condition part of an instruction leaving the rest of the current
  // instruction unchanged.
  void ChangeBranchCondition(Condition cond);

 private:
  byte* address_;  // The address of the code being patched.
  int size_;  // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
  FlushICache flush_cache_;  // Whether to flush the I cache after patching.
};



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
