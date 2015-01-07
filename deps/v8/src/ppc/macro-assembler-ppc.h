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
                Register reg8 = no_reg);
#endif

// These exist to provide portability between 32 and 64bit
#if V8_TARGET_ARCH_PPC64
#define LoadPU ldu
#define LoadPX ldx
#define LoadPUX ldux
#define StorePU stdu
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
#define LoadPU lwzu
#define LoadPX lwzx
#define LoadPUX lwzux
#define StorePU stwu
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
  // The isolate parameter can be NULL if the macro assembler should
  // not use isolate-dependent functionality. In this case, it's the
  // responsibility of the caller to never invoke such function on the
  // macro assembler.
  MacroAssembler(Isolate* isolate, void* buffer, int size);


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
  void Ret(Condition cond = al);

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count, Condition cond = al);

  void Ret(int drop, Condition cond = al);

  void Call(Label* target);

  // Emit call to the code we are currently generating.
  void CallSelf() {
    Handle<Code> self(reinterpret_cast<Code**>(CodeObject().location()));
    Call(self, RelocInfo::CODE_TARGET);
  }

  // Register move. May do nothing if the registers are identical.
  void Move(Register dst, Handle<Object> value);
  void Move(Register dst, Register src, Condition cond = al);
  void Move(DoubleRegister dst, DoubleRegister src);

  void MultiPush(RegList regs);
  void MultiPop(RegList regs);

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

  void CheckMapDeprecated(Handle<Map> map, Register scratch,
                          Label* if_deprecated);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object, Register scratch, Label* branch) {
    InNewSpace(object, scratch, ne, branch);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object, Register scratch, Label* branch) {
    InNewSpace(object, scratch, eq, branch);
  }

  // Check if an object has a given incremental marking color.
  void HasColor(Register object, Register scratch0, Register scratch1,
                Label* has_color, int first_bit, int second_bit);

  void JumpIfBlack(Register object, Register scratch0, Register scratch1,
                   Label* on_black);

  // Checks the color of an object.  If the object is already grey or black
  // then we just fall through, since it is already live.  If it is white and
  // we can determine that it doesn't need to be scanned, then we just mark it
  // black and fall through.  For the rest we jump to the label so the
  // incremental marker can fix its assumptions.
  void EnsureNotWhite(Register object, Register scratch1, Register scratch2,
                      Register scratch3, Label* object_is_white_and_not_data);

  // Detects conservatively whether an object is data-only, i.e. it does need to
  // be scanned by the garbage collector.
  void JumpIfDataObject(Register value, Register scratch,
                        Label* not_data_object);

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldOperand(reg, off).
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

  // Push a fixed frame, consisting of lr, fp, context and
  // JS function / marker id if marker_reg is a valid register.
  void PushFixedFrame(Register marker_reg = no_reg);
  void PopFixedFrame(Register marker_reg = no_reg);

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
  // the result to |double_dst|
  void ConvertIntToDouble(Register src, DoubleRegister double_dst);

  // Converts the unsigned integer (untagged smi) in |src| to
  // a double, storing the result to |double_dst|
  void ConvertUnsignedIntToDouble(Register src, DoubleRegister double_dst);

  // Converts the integer (untagged smi) in |src| to
  // a float, storing the result in |dst|
  // Warning: The value in |int_scrach| will be changed in the process!
  void ConvertIntToFloat(const DoubleRegister dst, const Register src,
                         const Register int_scratch);

  // Converts the double_input to an integer.  Note that, upon return,
  // the contents of double_dst will also hold the fixed point representation.
  void ConvertDoubleToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_PPC64
                            const Register dst_hi,
#endif
                            const Register dst, const DoubleRegister double_dst,
                            FPRoundingMode rounding_mode = kRoundToZero);

  // Generates function and stub prologue code.
  void StubPrologue(int prologue_offset = 0);
  void Prologue(bool code_pre_aging, int prologue_offset = 0);

  // Enter exit frame.
  // stack_space - extra stack space, used for alignment before call to C.
  void EnterExitFrame(bool save_doubles, int stack_space = 0);

  // Leave the current exit frame. Expects the return value in r0.
  // Expect the number of values, pushed prior to the exit frame, to
  // remove in a register (or no_reg, if there is nothing to remove).
  void LeaveExitFrame(bool save_doubles, Register argument_count,
                      bool restore_context);

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();

  void LoadContext(Register dst, int context_chain_length);

  // Conditionally load the cached Array transitioned map of type
  // transitioned_kind from the native context if the map in register
  // map_in_out is the cached Array map in the native context of
  // expected_kind.
  void LoadTransitionedArrayMapConditional(ElementsKind expected_kind,
                                           ElementsKind transitioned_kind,
                                           Register map_in_out,
                                           Register scratch,
                                           Label* no_map_match);

  void LoadGlobalFunction(int index, Register function);

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

  void StoreHalfWord(Register src, const MemOperand& mem, Register scratch);

  void LoadByte(Register dst, const MemOperand& mem, Register scratch);

  void StoreByte(Register src, const MemOperand& mem, Register scratch);

  void LoadRepresentation(Register dst, const MemOperand& mem, Representation r,
                          Register scratch = no_reg);

  void StoreRepresentation(Register src, const MemOperand& mem,
                           Representation r, Register scratch = no_reg);

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
  void MovDoubleLowToInt(Register dst, DoubleRegister src);
  void MovDoubleHighToInt(Register dst, DoubleRegister src);
  void MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
      Register dst_hi,
#endif
      Register dst, DoubleRegister src);

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
  void StoreP(Register src, const MemOperand& mem, Register scratch = no_reg);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeCode(Register code, const ParameterCount& expected,
                  const ParameterCount& actual, InvokeFlag flag,
                  const CallWrapper& call_wrapper);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function, const ParameterCount& actual,
                      InvokeFlag flag, const CallWrapper& call_wrapper);

  void InvokeFunction(Register function, const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void IsObjectJSObjectType(Register heap_object, Register map,
                            Register scratch, Label* fail);

  void IsInstanceJSObjectType(Register map, Register scratch, Label* fail);

  void IsObjectJSStringType(Register object, Register scratch, Label* fail);

  void IsObjectNameType(Register object, Register scratch, Label* fail);

  // ---------------------------------------------------------------------------
  // Debugger Support

  void DebugBreak();

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new try handler and link into try handler chain.
  void PushTryHandler(StackHandler::Kind kind, int handler_index);

  // Unlink the stack handler on top of the stack from the try handler chain.
  // Must preserve the result register.
  void PopTryHandler();

  // Passes thrown value to the handler of top of the try handler chain.
  void Throw(Register value);

  // Propagates an uncatchable exception to the top of the current JS stack's
  // handler chain.
  void ThrowUncatchable(Register value);

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

  // Allocate an object in new space or old pointer space. The object_size is
  // specified either in bytes or in words if the allocation flag SIZE_IN_WORDS
  // is passed. If the space is exhausted control continues at the gc_required
  // label. The allocated object is returned in result. If the flag
  // tag_allocated_object is true the result is tagged as as a heap object.
  // All registers are clobbered also when control continues at the gc_required
  // label.
  void Allocate(int object_size, Register result, Register scratch1,
                Register scratch2, Label* gc_required, AllocationFlags flags);

  void Allocate(Register object_size, Register result, Register scratch1,
                Register scratch2, Label* gc_required, AllocationFlags flags);

  // Undo allocation in new space. The object passed and objects allocated after
  // it will no longer be allocated. The caller must make sure that no pointers
  // are left to the object(s) no longer allocated as they would be invalid when
  // allocation is undone.
  void UndoAllocationInNewSpace(Register object, Register scratch);


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
                          TaggingMode tagging_mode = TAG_RESULT,
                          MutableMode mode = IMMUTABLE);
  void AllocateHeapNumberWithValue(Register result, DoubleRegister value,
                                   Register scratch1, Register scratch2,
                                   Register heap_number_map,
                                   Label* gc_required);

  // Copies a fixed number of fields of heap objects from src to dst.
  void CopyFields(Register dst, Register src, RegList temps, int field_count);

  // Copies a number of bytes from src to dst. All registers are clobbered. On
  // exit src and dst will point to the place just after where the last byte was
  // read or written and length will be zero.
  void CopyBytes(Register src, Register dst, Register length, Register scratch);

  // Initialize fields with filler values.  |count| fields starting at
  // |start_offset| are overwritten with the value in |filler|.  At the end the
  // loop, |start_offset| points at the next uninitialized field.  |count| is
  // assumed to be non-zero.
  void InitializeNFieldsWithFiller(Register start_offset, Register count,
                                   Register filler);

  // Initialize fields with filler values.  Fields starting at |start_offset|
  // not including end_offset are overwritten with the value in |filler|.  At
  // the end the loop, |start_offset| takes the value of |end_offset|.
  void InitializeFieldsWithFiller(Register start_offset, Register end_offset,
                                  Register filler);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Try to get function prototype of a function and puts the value in
  // the result register. Checks that the function really is a
  // function and jumps to the miss label if the fast checks fail. The
  // function register will be untouched; the other registers may be
  // clobbered.
  void TryGetFunctionPrototype(Register function, Register result,
                               Register scratch, Label* miss,
                               bool miss_on_bound_function = false);

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

  // Compare object type for heap object. Branch to false_label if type
  // is lower than min_type or greater than max_type.
  // Load map into the register map.
  void CheckObjectTypeRange(Register heap_object, Register map,
                            InstanceType min_type, InstanceType max_type,
                            Label* false_label);

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


  // Check if the map of an object is equal to a specified map and branch to a
  // specified target if equal. Skip the smi check if not required (object is
  // known to be a heap object)
  void DispatchMap(Register obj, Register scratch, Handle<Map> map,
                   Handle<Code> success, SmiCheckType smi_check_type);


  // Compare the object in a register to a value from the root list.
  // Uses the ip register as scratch.
  void CompareRoot(Register obj, Heap::RootListIndex index);


  // Load and check the instance type of an object for being a string.
  // Loads the type into the second argument register.
  // Returns a condition that will be enabled if the object was a string.
  Condition IsObjectStringType(Register obj, Register type) {
    LoadP(type, FieldMemOperand(obj, HeapObject::kMapOffset));
    lbz(type, FieldMemOperand(type, Map::kInstanceTypeOffset));
    andi(r0, type, Operand(kIsNotStringMask));
    DCHECK_EQ(0, kStringTag);
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

  void RetOnOverflow(void) {
    Label label;

    blt(&label, cr0);
    Ret();
    bind(&label);
  }

  void RetOnNoOverflow(void) {
    Label label;

    bge(&label, cr0);
    Ret();
    bind(&label);
  }

  // Pushes <count> double values to <location>, starting from d<first>.
  void SaveFPRegs(Register location, int first, int count);

  // Pops <count> double values from <location>, starting from d<first>.
  void RestoreFPRegs(Register location, int first, int count);

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
  void CallRuntimeSaveDoubles(Runtime::FunctionId id) {
    const Runtime::Function* function = Runtime::FunctionForId(id);
    CallRuntime(function, function->nargs, kSaveFPRegs);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId id, int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    CallRuntime(Runtime::FunctionForId(id), num_arguments, save_doubles);
  }

  // Convenience function: call an external reference.
  void CallExternalReference(const ExternalReference& ext, int num_arguments);

  // Tail call of a runtime routine (jump).
  // Like JumpToExternalReference, but also takes care of passing the number
  // of parameters.
  void TailCallExternalReference(const ExternalReference& ext,
                                 int num_arguments, int result_size);

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid, int num_arguments,
                       int result_size);

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

  // Calls an API function.  Allocates HandleScope, extracts returned value
  // from handle and propagates exceptions.  Restores context.  stack_space
  // - space to be unwound on exit (includes the call JS arguments space and
  // the additional space allocated for the fast call).
  void CallApiFunctionAndReturn(Register function_address,
                                ExternalReference thunk_ref, int stack_space,
                                MemOperand return_value_operand,
                                MemOperand* context_restore_operand);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& builtin);

  // Invoke specified builtin JavaScript function. Adds an entry to
  // the unresolved list if the name does not resolve.
  void InvokeBuiltin(Builtins::JavaScript id, InvokeFlag flag,
                     const CallWrapper& call_wrapper = NullCallWrapper());

  // Store the code object for the given builtin in the target register and
  // setup the function in r1.
  void GetBuiltinEntry(Register target, Builtins::JavaScript id);

  // Store the function for the given builtin in the target register.
  void GetBuiltinFunction(Register target, Builtins::JavaScript id);

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
  // and place them into the least significant bits of dst.
  inline void ExtractBitRange(Register dst, Register src, int rangeStart,
                              int rangeEnd, RCBit rc = LeaveRC) {
    DCHECK(rangeStart >= rangeEnd && rangeStart < kBitsPerPointer);
    int rotate = (rangeEnd == 0) ? 0 : kBitsPerPointer - rangeEnd;
    int width = rangeStart - rangeEnd + 1;
#if V8_TARGET_ARCH_PPC64
    rldicl(dst, src, rotate, kBitsPerPointer - width, rc);
#else
    rlwinm(dst, src, rotate, kBitsPerPointer - width, kBitsPerPointer - 1, rc);
#endif
  }

  inline void ExtractBit(Register dst, Register src, uint32_t bitNumber,
                         RCBit rc = LeaveRC) {
    ExtractBitRange(dst, src, bitNumber, bitNumber, rc);
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

    ExtractBitRange(dst, src, start, end, rc);
  }

  // Test single bit in value.
  inline void TestBit(Register value, int bitNumber, Register scratch = r0) {
    ExtractBitRange(scratch, value, bitNumber, bitNumber, SetRC);
  }

  // Test consecutive bit range in value.  Range is defined by
  // rangeStart - rangeEnd.
  inline void TestBitRange(Register value, int rangeStart, int rangeEnd,
                           Register scratch = r0) {
    ExtractBitRange(scratch, value, rangeStart, rangeEnd, SetRC);
  }

  // Test consecutive bit range in value.  Range is defined by mask.
  inline void TestBitMask(Register value, uintptr_t mask,
                          Register scratch = r0) {
    ExtractBitMask(scratch, value, mask, SetRC);
  }


  // ---------------------------------------------------------------------------
  // Smi utilities

  // Shift left by 1
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
    TestBit(value, 0, scratch);  // tst(value, Operand(kSmiTagMask));
  }

  inline void TestIfPositiveSmi(Register value, Register scratch) {
    STATIC_ASSERT((kSmiTagMask | kSmiSignMask) ==
                  (intptr_t)(1UL << (kBitsPerPointer - 1) | 1));
#if V8_TARGET_ARCH_PPC64
    rldicl(scratch, value, 1, kBitsPerPointer - 2, SetRC);
#else
    rlwinm(scratch, value, 1, kBitsPerPointer - 2, kBitsPerPointer - 1, SetRC);
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

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);
  void AssertSmi(Register object);


#if V8_TARGET_ARCH_PPC64
  inline void TestIfInt32(Register value, Register scratch1, Register scratch2,
                          CRegister cr = cr7) {
    // High bits must be identical to fit into an 32-bit integer
    srawi(scratch1, value, 31);
    sradi(scratch2, value, 32);
    cmp(scratch1, scratch2, cr);
  }
#else
  inline void TestIfInt32(Register hi_word, Register lo_word, Register scratch,
                          CRegister cr = cr7) {
    // High bits must be identical to fit into an 32-bit integer
    srawi(scratch, lo_word, 31);
    cmp(scratch, hi_word, cr);
  }
#endif

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
  // HeapNumber utilities

  void JumpIfNotHeapNumber(Register object, Register heap_number_map,
                           Register scratch, Label* on_not_heap_number);

  // ---------------------------------------------------------------------------
  // String utilities

  // Generate code to do a lookup in the number string cache. If the number in
  // the register object is found in the cache the generated code falls through
  // with the result in the result register. The object and the result register
  // can be the same. If the number is not found in the cache the code jumps to
  // the label not_found with only the content of register object unchanged.
  void LookupNumberStringCache(Register object, Register result,
                               Register scratch1, Register scratch2,
                               Register scratch3, Label* not_found);

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

  // Retrieve/patch the relocated value (lis/ori pair or constant pool load).
  void GetRelocatedValue(Register location, Register result, Register scratch);
  void SetRelocatedValue(Register location, Register scratch,
                         Register new_value);

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

  template <typename Field>
  void DecodeField(Register dst, Register src) {
    ExtractBitRange(dst, src, Field::kShift + Field::kSize - 1, Field::kShift);
  }

  template <typename Field>
  void DecodeField(Register reg) {
    DecodeField<Field>(reg, reg);
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

  // Activation support.
  void EnterFrame(StackFrame::Type type,
                  bool load_constant_pool_pointer_reg = false);
  // Returns the pc offset at which the frame ends.
  int LeaveFrame(StackFrame::Type type, int stack_adjustment = 0);

  // Expects object in r0 and returns map with validated enum cache
  // in r0.  Assumes that any other register can be used as a scratch.
  void CheckEnumCache(Register null_value, Label* call_runtime);

  // AllocationMemento support. Arrays may have an associated
  // AllocationMemento object that can be checked for in order to pretransition
  // to another type.
  // On entry, receiver_reg should point to the array object.
  // scratch_reg gets clobbered.
  // If allocation info is present, condition flags are set to eq.
  void TestJSArrayForAllocationMemento(Register receiver_reg,
                                       Register scratch_reg,
                                       Label* no_memento_found);

  void JumpIfJSArrayHasAllocationMemento(Register receiver_reg,
                                         Register scratch_reg,
                                         Label* memento_found) {
    Label no_memento_found;
    TestJSArrayForAllocationMemento(receiver_reg, scratch_reg,
                                    &no_memento_found);
    beq(memento_found);
    bind(&no_memento_found);
  }

  // Jumps to found label if a prototype map has dictionary elements.
  void JumpIfDictionaryInPrototypeChain(Register object, Register scratch0,
                                        Register scratch1, Label* found);

 private:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  void CallCFunctionHelper(Register function, int num_reg_arguments,
                           int num_double_arguments);

  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al,
            CRegister cr = cr7);

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual, Handle<Code> code_constant,
                      Register code_reg, Label* done,
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

  // Helper for throwing exceptions.  Compute a handler address and jump to
  // it.  See the implementation for register usage.
  void JumpToHandlerEntry();

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);
  MemOperand SafepointRegisterSlot(Register reg);
  MemOperand SafepointRegistersAndDoublesSlot(Register reg);

#if V8_OOL_CONSTANT_POOL
  // Loads the constant pool pointer (kConstantPoolRegister).
  enum CodeObjectAccessMethod { CAN_USE_IP, CONSTRUCT_INTERNAL_REFERENCE };
  void LoadConstantPoolPointerRegister(CodeObjectAccessMethod access_method,
                                       int ip_code_entry_delta = 0);
#endif

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

  CodePatcher(byte* address, int instructions, FlushICache flush_cache = FLUSH);
  virtual ~CodePatcher();

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

inline MemOperand ContextOperand(Register context, int index) {
  return MemOperand(context, Context::SlotOffset(index));
}


inline MemOperand GlobalObjectOperand() {
  return ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX);
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
}
}  // namespace v8::internal

#endif  // V8_PPC_MACRO_ASSEMBLER_PPC_H_
