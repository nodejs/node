// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_MACRO_ASSEMBLER_X64_H_
#define V8_X64_MACRO_ASSEMBLER_X64_H_

#include "src/bailout-reason.h"
#include "src/base/flags.h"
#include "src/globals.h"
#include "src/x64/assembler-x64.h"

namespace v8 {
namespace internal {

// Give alias names to registers for calling conventions.
const Register kReturnRegister0 = {Register::kCode_rax};
const Register kReturnRegister1 = {Register::kCode_rdx};
const Register kReturnRegister2 = {Register::kCode_r8};
const Register kJSFunctionRegister = {Register::kCode_rdi};
const Register kContextRegister = {Register::kCode_rsi};
const Register kAllocateSizeRegister = {Register::kCode_rdx};
const Register kInterpreterAccumulatorRegister = {Register::kCode_rax};
const Register kInterpreterBytecodeOffsetRegister = {Register::kCode_r12};
const Register kInterpreterBytecodeArrayRegister = {Register::kCode_r14};
const Register kInterpreterDispatchTableRegister = {Register::kCode_r15};
const Register kJavaScriptCallArgCountRegister = {Register::kCode_rax};
const Register kJavaScriptCallNewTargetRegister = {Register::kCode_rdx};
const Register kRuntimeCallFunctionRegister = {Register::kCode_rbx};
const Register kRuntimeCallArgCountRegister = {Register::kCode_rax};

// Default scratch register used by MacroAssembler (and other code that needs
// a spare register). The register isn't callee save, and not used by the
// function calling convention.
const Register kScratchRegister = {10};      // r10.
const XMMRegister kScratchDoubleReg = {15};  // xmm15.
const Register kRootRegister = {13};         // r13 (callee save).
// Actual value of root register is offset from the root array's start
// to take advantage of negitive 8-bit displacement values.
const int kRootRegisterBias = 128;

// Convenience for platform-independent signatures.
typedef Operand MemOperand;

enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum PointersToHereCheck {
  kPointersToHereMaybeInteresting,
  kPointersToHereAreAlwaysInteresting
};

enum class SmiOperationConstraint {
  kPreserveSourceRegister = 1 << 0,
  kBailoutOnNoOverflow = 1 << 1,
  kBailoutOnOverflow = 1 << 2
};

enum class ReturnAddressState { kOnStack, kNotOnStack };

typedef base::Flags<SmiOperationConstraint> SmiOperationConstraints;

DEFINE_OPERATORS_FOR_FLAGS(SmiOperationConstraints)

#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3 = no_reg,
                Register reg4 = no_reg,
                Register reg5 = no_reg,
                Register reg6 = no_reg,
                Register reg7 = no_reg,
                Register reg8 = no_reg);
#endif

// Forward declaration.
class JumpTarget;

struct SmiIndex {
  SmiIndex(Register index_register, ScaleFactor scale)
      : reg(index_register),
        scale(scale) {}
  Register reg;
  ScaleFactor scale;
};

class TurboAssembler : public Assembler {
 public:
  TurboAssembler(Isolate* isolate, void* buffer, int buffer_size,
                 CodeObjectRequired create_code_object)
      : Assembler(isolate, buffer, buffer_size), isolate_(isolate) {
    if (create_code_object == CodeObjectRequired::kYes) {
      code_object_ =
          Handle<HeapObject>::New(isolate->heap()->undefined_value(), isolate);
    }
  }

  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() const { return has_frame_; }

  Isolate* isolate() const { return isolate_; }

  Handle<HeapObject> CodeObject() {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }

#define AVX_OP2_WITH_TYPE(macro_name, name, src_type) \
  void macro_name(XMMRegister dst, src_type src) {    \
    if (CpuFeatures::IsSupported(AVX)) {              \
      CpuFeatureScope scope(this, AVX);               \
      v##name(dst, dst, src);                         \
    } else {                                          \
      name(dst, src);                                 \
    }                                                 \
  }
#define AVX_OP2_X(macro_name, name) \
  AVX_OP2_WITH_TYPE(macro_name, name, XMMRegister)
#define AVX_OP2_O(macro_name, name) \
  AVX_OP2_WITH_TYPE(macro_name, name, const Operand&)
#define AVX_OP2_XO(macro_name, name) \
  AVX_OP2_X(macro_name, name)        \
  AVX_OP2_O(macro_name, name)

  AVX_OP2_XO(Subsd, subsd)
  AVX_OP2_XO(Divss, divss)
  AVX_OP2_XO(Divsd, divsd)
  AVX_OP2_XO(Xorpd, xorpd)
  AVX_OP2_X(Pcmpeqd, pcmpeqd)
  AVX_OP2_WITH_TYPE(Psllq, psllq, byte)
  AVX_OP2_WITH_TYPE(Psrlq, psrlq, byte)

#undef AVX_OP2_O
#undef AVX_OP2_X
#undef AVX_OP2_XO
#undef AVX_OP2_WITH_TYPE

  void Xorps(XMMRegister dst, XMMRegister src);
  void Xorps(XMMRegister dst, const Operand& src);

  void Movd(XMMRegister dst, Register src);
  void Movd(XMMRegister dst, const Operand& src);
  void Movd(Register dst, XMMRegister src);
  void Movq(XMMRegister dst, Register src);
  void Movq(Register dst, XMMRegister src);

  void Movsd(XMMRegister dst, XMMRegister src);
  void Movsd(XMMRegister dst, const Operand& src);
  void Movsd(const Operand& dst, XMMRegister src);
  void Movss(XMMRegister dst, XMMRegister src);
  void Movss(XMMRegister dst, const Operand& src);
  void Movss(const Operand& dst, XMMRegister src);

  void PushReturnAddressFrom(Register src) { pushq(src); }
  void PopReturnAddressTo(Register dst) { popq(dst); }

  void Ret();

  // Return and drop arguments from stack, where the number of arguments
  // may be bigger than 2^16 - 1.  Requires a scratch register.
  void Ret(int bytes_dropped, Register scratch);

  // Load a register with a long value as efficiently as possible.
  void Set(Register dst, int64_t x);
  void Set(const Operand& dst, intptr_t x);

  // Operations on roots in the root-array.
  void LoadRoot(Register destination, Heap::RootListIndex index);
  void LoadRoot(const Operand& destination, Heap::RootListIndex index) {
    LoadRoot(kScratchRegister, index);
    movp(destination, kScratchRegister);
  }

  void Movups(XMMRegister dst, XMMRegister src);
  void Movups(XMMRegister dst, const Operand& src);
  void Movups(const Operand& dst, XMMRegister src);
  void Movapd(XMMRegister dst, XMMRegister src);
  void Movaps(XMMRegister dst, XMMRegister src);
  void Movmskpd(Register dst, XMMRegister src);
  void Movmskps(Register dst, XMMRegister src);

  void Push(Register src);
  void Push(const Operand& src);
  void Push(Immediate value);
  void Push(Smi* smi);
  void Push(Handle<HeapObject> source);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in rsp[0], rsp[8],
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

  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met,
                     Label::Distance condition_met_distance = Label::kFar);

  void Cvtss2sd(XMMRegister dst, XMMRegister src);
  void Cvtss2sd(XMMRegister dst, const Operand& src);
  void Cvtsd2ss(XMMRegister dst, XMMRegister src);
  void Cvtsd2ss(XMMRegister dst, const Operand& src);
  void Cvttsd2si(Register dst, XMMRegister src);
  void Cvttsd2si(Register dst, const Operand& src);
  void Cvttsd2siq(Register dst, XMMRegister src);
  void Cvttsd2siq(Register dst, const Operand& src);
  void Cvttss2si(Register dst, XMMRegister src);
  void Cvttss2si(Register dst, const Operand& src);
  void Cvttss2siq(Register dst, XMMRegister src);
  void Cvttss2siq(Register dst, const Operand& src);
  void Cvtqsi2ss(XMMRegister dst, Register src);
  void Cvtqsi2ss(XMMRegister dst, const Operand& src);
  void Cvtqsi2sd(XMMRegister dst, Register src);
  void Cvtqsi2sd(XMMRegister dst, const Operand& src);
  void Cvtlsi2ss(XMMRegister dst, Register src);
  void Cvtlsi2ss(XMMRegister dst, const Operand& src);
  void Cvtqui2ss(XMMRegister dst, Register src, Register tmp);
  void Cvtqui2sd(XMMRegister dst, Register src, Register tmp);

  // cvtsi2sd instruction only writes to the low 64-bit of dst register, which
  // hinders register renaming and makes dependence chains longer. So we use
  // xorpd to clear the dst register before cvtsi2sd to solve this issue.
  void Cvtlsi2sd(XMMRegister dst, Register src);
  void Cvtlsi2sd(XMMRegister dst, const Operand& src);

  void Roundss(XMMRegister dst, XMMRegister src, RoundingMode mode);
  void Roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode);

  void Sqrtsd(XMMRegister dst, XMMRegister src);
  void Sqrtsd(XMMRegister dst, const Operand& src);

  void Ucomiss(XMMRegister src1, XMMRegister src2);
  void Ucomiss(XMMRegister src1, const Operand& src2);
  void Ucomisd(XMMRegister src1, XMMRegister src2);
  void Ucomisd(XMMRegister src1, const Operand& src2);

  void Lzcntq(Register dst, Register src);
  void Lzcntq(Register dst, const Operand& src);
  void Lzcntl(Register dst, Register src);
  void Lzcntl(Register dst, const Operand& src);
  void Tzcntq(Register dst, Register src);
  void Tzcntq(Register dst, const Operand& src);
  void Tzcntl(Register dst, Register src);
  void Tzcntl(Register dst, const Operand& src);
  void Popcntl(Register dst, Register src);
  void Popcntl(Register dst, const Operand& src);
  void Popcntq(Register dst, Register src);
  void Popcntq(Register dst, const Operand& src);

  // Is the value a tagged smi.
  Condition CheckSmi(Register src);
  Condition CheckSmi(const Operand& src);

  // Jump to label if the value is a tagged smi.
  void JumpIfSmi(Register src, Label* on_smi,
                 Label::Distance near_jump = Label::kFar);

  void Move(Register dst, Smi* source);

  void Move(const Operand& dst, Smi* source) {
    Register constant = GetSmiConstant(source);
    movp(dst, constant);
  }

  void Move(Register dst, ExternalReference ext) {
    movp(dst, reinterpret_cast<void*>(ext.address()),
         RelocInfo::EXTERNAL_REFERENCE);
  }

  void Move(XMMRegister dst, uint32_t src);
  void Move(XMMRegister dst, uint64_t src);
  void Move(XMMRegister dst, float src) { Move(dst, bit_cast<uint32_t>(src)); }
  void Move(XMMRegister dst, double src) { Move(dst, bit_cast<uint64_t>(src)); }

  // Move if the registers are not identical.
  void Move(Register target, Register source);

  void Move(Register dst, Handle<HeapObject> source,
            RelocInfo::Mode rmode = RelocInfo::EMBEDDED_OBJECT);
  void Move(const Operand& dst, Handle<HeapObject> source,
            RelocInfo::Mode rmode = RelocInfo::EMBEDDED_OBJECT);

  // Loads a pointer into a register with a relocation mode.
  void Move(Register dst, void* ptr, RelocInfo::Mode rmode) {
    // This method must not be used with heap object references. The stored
    // address is not GC safe. Use the handle version instead.
    DCHECK(rmode > RelocInfo::LAST_GCED_ENUM);
    movp(dst, ptr, rmode);
  }

  // Convert smi to 32-bit integer. I.e., not sign extended into
  // high 32 bits of destination.
  void SmiToInteger32(Register dst, Register src);
  void SmiToInteger32(Register dst, const Operand& src);

  // Loads the address of the external reference into the destination
  // register.
  void LoadAddress(Register destination, ExternalReference source);

  void Call(const Operand& op);
  void Call(Handle<Code> code_object, RelocInfo::Mode rmode);
  void Call(Address destination, RelocInfo::Mode rmode);
  void Call(ExternalReference ext);
  void Call(Label* target) { call(target); }

  void CallForDeoptimization(Address target, RelocInfo::Mode rmode) {
    call(target, rmode);
  }

  // The size of the code generated for different call instructions.
  int CallSize(ExternalReference ext);
  int CallSize(Address destination) { return kCallSequenceLength; }
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

  // Returns the size of the code generated by LoadAddress.
  // Used by CallSize(ExternalReference) to find the size of a call.
  int LoadAddressSize(ExternalReference source);

  // Non-SSE2 instructions.
  void Pextrd(Register dst, XMMRegister src, int8_t imm8);
  void Pinsrd(XMMRegister dst, Register src, int8_t imm8);
  void Pinsrd(XMMRegister dst, const Operand& src, int8_t imm8);

  void CompareRoot(Register with, Heap::RootListIndex index);
  void CompareRoot(const Operand& with, Heap::RootListIndex index);

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue();

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, BailoutReason reason);

  // Like Assert(), but without condition.
  // Use --debug_code to enable.
  void AssertUnreachable(BailoutReason reason);

  // Abort execution if a 64 bit register containing a 32 bit payload does not
  // have zeros in the top 32 bits, enabled via --debug-code.
  void AssertZeroExtended(Register reg);

  // Like Assert(), but always enabled.
  void Check(Condition cc, BailoutReason reason);

  // Print a message to stdout and abort execution.
  void Abort(BailoutReason msg);

  // Check that the stack is aligned.
  void CheckStackAlignment();

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void EnterFrame(StackFrame::Type type, bool load_constant_pool_pointer_reg) {
    // Out-of-line constant pool not implemented on x64.
    UNREACHABLE();
  }
  void LeaveFrame(StackFrame::Type type);

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // |ra_state| defines whether return address is already pushed to stack or
  // not. Both |callee_args_count| and |caller_args_count_reg| do not include
  // receiver. |callee_args_count| is not modified, |caller_args_count_reg|
  // is trashed.
  void PrepareForTailCall(const ParameterCount& callee_args_count,
                          Register caller_args_count_reg, Register scratch0,
                          Register scratch1, ReturnAddressState ra_state);

  inline bool AllowThisStubCall(CodeStub* stub);

  // Call a code stub. This expects {stub} to be zone-allocated, as it does not
  // trigger generation of the stub's code object but instead files a
  // HeapObjectRequest that will be fulfilled after code assembly.
  void CallStubDelayed(CodeStub* stub);

  void SlowTruncateToIDelayed(Zone* zone, Register result_reg,
                              Register input_reg,
                              int offset = HeapNumber::kValueOffset -
                                           kHeapObjectTag);

  // Call a runtime routine.
  void CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                          SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  void InitializeRootRegister() {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    Move(kRootRegister, roots_array_start);
    addp(kRootRegister, Immediate(kRootRegisterBias));
  }

  void MoveNumber(Register dst, double value);
  void MoveNonSmi(Register dst, double value);

  // These functions do not arrange the registers in any particular order so
  // they are not useful for calls that can cause a GC.  The caller can
  // exclude up to 3 registers that do not need to be saved and restored.
  void PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                       Register exclusion2 = no_reg,
                       Register exclusion3 = no_reg);
  void PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                      Register exclusion2 = no_reg,
                      Register exclusion3 = no_reg);

 protected:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;
  int smi_count = 0;
  int heap_object_count = 0;

  bool root_array_available_ = true;

  int64_t RootRegisterDelta(ExternalReference other);

  // Returns a register holding the smi value. The register MUST NOT be
  // modified. It may be the "smi 1 constant" register.
  Register GetSmiConstant(Smi* value);

 private:
  bool has_frame_ = false;
  // This handle will be patched with the code object on installation.
  Handle<HeapObject> code_object_;
  Isolate* const isolate_;
};

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler : public TurboAssembler {
 public:
  MacroAssembler(Isolate* isolate, void* buffer, int size,
                 CodeObjectRequired create_code_object);

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

  // Pushes the address of the external reference onto the stack.
  void PushAddress(ExternalReference source);

  // Operations on roots in the root-array.
  // Load a root value where the index (or part of it) is variable.
  // The variable_offset register is added to the fixed_offset value
  // to get the index into the root-array.
  void LoadRootIndexed(Register destination,
                       Register variable_offset,
                       int fixed_offset);
  void PushRoot(Heap::RootListIndex index);

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(Register with, Heap::RootListIndex index, Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }
  void JumpIfRoot(const Operand& with, Heap::RootListIndex index,
                  Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(Register with, Heap::RootListIndex index,
                     Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }
  void JumpIfNotRoot(const Operand& with, Heap::RootListIndex index,
                     Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }


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

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object,
                           Register scratch,
                           Label* branch,
                           Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, zero, branch, distance);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object,
                        Register scratch,
                        Label* branch,
                        Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, not_zero, branch, distance);
  }

  // Check if an object has the black incremental marking color.  Also uses rcx!
  void JumpIfBlack(Register object, Register bitmap_scratch,
                   Register mask_scratch, Label* on_black,
                   Label::Distance on_black_distance);

  // Checks the color of an object.  If the object is white we jump to the
  // incremental marker.
  void JumpIfWhite(Register value, Register scratch1, Register scratch2,
                   Label* value_is_white, Label::Distance distance);

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
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // As above, but the offset has the tag presubtracted.  For use with
  // Operand(reg, off).
  void RecordWriteContextSlot(
      Register context,
      int offset,
      Register value,
      Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting) {
    RecordWriteField(context,
                     offset + kHeapObjectTag,
                     value,
                     scratch,
                     save_fp,
                     remembered_set_action,
                     smi_check,
                     pointers_to_here_check_for_value);
  }

  void RecordWriteForMap(
      Register object,
      Register map,
      Register dst,
      SaveFPRegsMode save_fp);

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
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // Frame restart support.
  void MaybeDropFrames();

  // Enter specific kind of exit frame; either in normal or
  // debug mode. Expects the number of arguments in register rax and
  // sets up the number of arguments in register rdi and the pointer
  // to the first argument in register rsi.
  //
  // Allocates arg_stack_space * kPointerSize memory (not GCed) on the stack
  // accessible via StackSpaceOperand.
  void EnterExitFrame(int arg_stack_space = 0, bool save_doubles = false,
                      StackFrame::Type frame_type = StackFrame::EXIT);

  // Enter specific kind of exit frame. Allocates arg_stack_space * kPointerSize
  // memory (not GCed) on the stack accessible via StackSpaceOperand.
  void EnterApiExitFrame(int arg_stack_space);

  // Leave the current exit frame. Expects/provides the return value in
  // register rax:rdx (untouched) and the pointer to the first
  // argument in register rsi (if pop_arguments == true).
  void LeaveExitFrame(bool save_doubles = false, bool pop_arguments = true);

  // Leave the current exit frame. Expects/provides the return value in
  // register rax (untouched).
  void LeaveApiExitFrame(bool restore_context);

  // Push and pop the registers that can hold pointers.
  void PushSafepointRegisters() { Pushad(); }
  void PopSafepointRegisters() { Popad(); }

  // ---------------------------------------------------------------------------
  // JavaScript invokes

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

  void InvokeFunction(Register function, Register new_target,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag);

  // ---------------------------------------------------------------------------
  // Conversions between tagged smi values and non-tagged integer values.

  // Tag an integer value. The result must be known to be a valid smi value.
  // Only uses the low 32 bits of the src register. Sets the N and Z flags
  // based on the value of the resulting smi.
  void Integer32ToSmi(Register dst, Register src);

  // Convert smi to 64-bit integer (sign extended if necessary).
  void SmiToInteger64(Register dst, Register src);
  void SmiToInteger64(Register dst, const Operand& src);

  // Convert smi to double.
  void SmiToDouble(XMMRegister dst, Register src) {
    SmiToInteger32(kScratchRegister, src);
    Cvtlsi2sd(dst, kScratchRegister);
  }

  // Multiply a positive smi's integer value by a power of two.
  // Provides result as 64-bit integer value.
  void PositiveSmiTimesPowerOfTwoToInteger64(Register dst,
                                             Register src,
                                             int power);

  // Simple comparison of smis.  Both sides must be known smis to use these,
  // otherwise use Cmp.
  void SmiCompare(Register smi1, Register smi2);
  void SmiCompare(Register dst, Smi* src);
  void SmiCompare(Register dst, const Operand& src);
  void SmiCompare(const Operand& dst, Register src);
  void SmiCompare(const Operand& dst, Smi* src);
  // Compare the int32 in src register to the value of the smi stored at dst.
  void SmiTest(Register src);

  // Functions performing a check on a known or potential smi. Returns
  // a condition that is satisfied if the check is successful.

  // Are both values tagged smis.
  Condition CheckBothSmi(Register first, Register second);

  // Are either value a tagged smi.
  Condition CheckEitherSmi(Register first,
                           Register second,
                           Register scratch = kScratchRegister);
  // Test-and-jump functions. Typically combines a check function
  // above with a conditional jump.

  // Jump to label if the value is not a tagged smi.
  void JumpIfNotSmi(Register src,
                    Label* on_not_smi,
                    Label::Distance near_jump = Label::kFar);

  // Jump to label if the value is not a tagged smi.
  void JumpIfNotSmi(Operand src, Label* on_not_smi,
                    Label::Distance near_jump = Label::kFar);

  // Jump if either or both register are not smi values.
  void JumpIfNotBothSmi(Register src1,
                        Register src2,
                        Label* on_not_both_smi,
                        Label::Distance near_jump = Label::kFar);

  // Operations on tagged smi values.

  // Smis represent a subset of integers. The subset is always equivalent to
  // a two's complement interpretation of a fixed number of bits.

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  void SmiAddConstant(Register dst, Register src, Smi* constant);

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  void SmiAddConstant(const Operand& dst, Smi* constant);

  // Add an integer constant to a tagged smi, giving a tagged smi as result,
  // or jumping to a label if the result cannot be represented by a smi.
  void SmiAddConstant(Register dst, Register src, Smi* constant,
                      SmiOperationConstraints constraints, Label* bailout_label,
                      Label::Distance near_jump = Label::kFar);

  // Subtract an integer constant from a tagged smi, giving a tagged smi as
  // result. No testing on the result is done. Sets the N and Z flags
  // based on the value of the resulting integer.
  void SmiSubConstant(Register dst, Register src, Smi* constant);

  // Subtract an integer constant from a tagged smi, giving a tagged smi as
  // result, or jumping to a label if the result cannot be represented by a smi.
  void SmiSubConstant(Register dst, Register src, Smi* constant,
                      SmiOperationConstraints constraints, Label* bailout_label,
                      Label::Distance near_jump = Label::kFar);

  // Adds smi values and return the result as a smi.
  // If dst is src1, then src1 will be destroyed if the operation is
  // successful, otherwise kept intact.
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
  // If dst is src1, then src1 will be destroyed if the operation is
  // successful, otherwise kept intact.
  void SmiSub(Register dst,
              Register src1,
              Register src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);
  void SmiSub(Register dst,
              Register src1,
              const Operand& src2,
              Label* on_not_smi_result,
              Label::Distance near_jump = Label::kFar);

  void SmiSub(Register dst,
              Register src1,
              Register src2);

  void SmiSub(Register dst,
              Register src1,
              const Operand& src2);

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

  // ---------------------------------------------------------------------------
  // String macros.

  void JumpIfNotBothSequentialOneByteStrings(
      Register first_object, Register second_object, Register scratch1,
      Register scratch2, Label* on_not_both_flat_one_byte,
      Label::Distance near_jump = Label::kFar);

  void JumpIfBothInstanceTypesAreNotSequentialOneByte(
      Register first_object_instance_type, Register second_object_instance_type,
      Register scratch1, Register scratch2, Label* on_fail,
      Label::Distance near_jump = Label::kFar);

  // Checks if the given register or operand is a unique name
  void JumpIfNotUniqueNameInstanceType(Register reg, Label* not_unique_name,
                                       Label::Distance distance = Label::kFar);
  void JumpIfNotUniqueNameInstanceType(Operand operand, Label* not_unique_name,
                                       Label::Distance distance = Label::kFar);

  // ---------------------------------------------------------------------------
  // Macro instructions.

  // Load/store with specific representation.
  void Load(Register dst, const Operand& src, Representation r);
  void Store(const Operand& dst, Register src, Representation r);

  void Cmp(Register dst, Handle<Object> source);
  void Cmp(const Operand& dst, Handle<Object> source);
  void Cmp(Register dst, Smi* src);
  void Cmp(const Operand& dst, Smi* src);

  void GetWeakValue(Register value, Handle<WeakCell> cell);

  // Load the value of the weak cell in the value register. Branch to the given
  // miss label if the weak cell was cleared.
  void LoadWeakValue(Register value, Handle<WeakCell> cell, Label* miss);

  // Emit code that loads |parameter_index|'th parameter from the stack to
  // the register according to the CallInterfaceDescriptor definition.
  // |sp_to_caller_sp_offset_in_words| specifies the number of words pushed
  // below the caller's sp (on x64 it's at least return address).
  template <class Descriptor>
  void LoadParameterFromStack(
      Register reg, typename Descriptor::ParameterIndices parameter_index,
      int sp_to_ra_offset_in_words = 1) {
    DCHECK(Descriptor::kPassLastArgsOnStack);
    UNIMPLEMENTED();
  }

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the rsp register.
  void Drop(int stack_elements);
  // Emit code to discard a positive number of pointer-sized elements
  // from the stack under the return address which remains on the top,
  // clobbering the rsp register.
  void DropUnderReturnAddress(int stack_elements,
                              Register scratch = kScratchRegister);

  void PushQuad(const Operand& src);
  void PushImm32(int32_t imm32);
  void Pop(Register dst);
  void Pop(const Operand& dst);
  void PopQuad(const Operand& dst);

#define AVX_OP2_WITH_TYPE(macro_name, name, src_type) \
  void macro_name(XMMRegister dst, src_type src) {    \
    if (CpuFeatures::IsSupported(AVX)) {              \
      CpuFeatureScope scope(this, AVX);               \
      v##name(dst, dst, src);                         \
    } else {                                          \
      name(dst, src);                                 \
    }                                                 \
  }
#define AVX_OP2_X(macro_name, name) \
  AVX_OP2_WITH_TYPE(macro_name, name, XMMRegister)
#define AVX_OP2_O(macro_name, name) \
  AVX_OP2_WITH_TYPE(macro_name, name, const Operand&)
#define AVX_OP2_XO(macro_name, name) \
  AVX_OP2_X(macro_name, name)        \
  AVX_OP2_O(macro_name, name)

  AVX_OP2_XO(Addsd, addsd)
  AVX_OP2_XO(Mulsd, mulsd)
  AVX_OP2_XO(Andps, andps)
  AVX_OP2_XO(Andpd, andpd)
  AVX_OP2_XO(Orpd, orpd)
  AVX_OP2_XO(Cmpeqps, cmpeqps)
  AVX_OP2_XO(Cmpltps, cmpltps)
  AVX_OP2_XO(Cmpleps, cmpleps)
  AVX_OP2_XO(Cmpneqps, cmpneqps)
  AVX_OP2_XO(Cmpnltps, cmpnltps)
  AVX_OP2_XO(Cmpnleps, cmpnleps)
  AVX_OP2_XO(Cmpeqpd, cmpeqpd)
  AVX_OP2_XO(Cmpltpd, cmpltpd)
  AVX_OP2_XO(Cmplepd, cmplepd)
  AVX_OP2_XO(Cmpneqpd, cmpneqpd)
  AVX_OP2_XO(Cmpnltpd, cmpnltpd)
  AVX_OP2_XO(Cmpnlepd, cmpnlepd)

#undef AVX_OP2_O
#undef AVX_OP2_X
#undef AVX_OP2_XO
#undef AVX_OP2_WITH_TYPE

  // ---------------------------------------------------------------------------
  // SIMD macros.
  void Absps(XMMRegister dst);
  void Negps(XMMRegister dst);
  void Abspd(XMMRegister dst);
  void Negpd(XMMRegister dst);

  // Control Flow
  void Jump(Address destination, RelocInfo::Mode rmode);
  void Jump(ExternalReference ext);
  void Jump(const Operand& op);
  void Jump(Handle<Code> code_object, RelocInfo::Mode rmode);

  // Non-x64 instructions.
  // Push/pop all general purpose registers.
  // Does not push rsp/rbp nor any of the assembler's special purpose registers
  // (kScratchRegister, kRootRegister).
  void Pushad();
  void Popad();

  // Compare object type for heap object.
  // Always use unsigned comparisons: above and below, not less and greater.
  // Incoming register is heap_object and outgoing register is map.
  // They may be the same register, and may be kScratchRegister.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  // Always use unsigned comparisons: above and below, not less and greater.
  void CmpInstanceType(Register map, InstanceType type);

  // Compare an object's map with the specified map.
  void CompareMap(Register obj, Handle<Map> map);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specified map.
  void CheckMap(Register obj,
                Handle<Map> map,
                Label* fail,
                SmiCheckType smi_check_type);

  void DoubleToI(Register result_reg, XMMRegister input_reg,
                 XMMRegister scratch, MinusZeroMode minus_zero_mode,
                 Label* lost_precision, Label* is_nan, Label* minus_zero,
                 Label::Distance dst = Label::kFar);

  void LoadInstanceDescriptors(Register map, Register descriptors);
  void LoadAccessor(Register dst, Register holder, int accessor_index,
                    AccessorComponent accessor);

  template<typename Field>
  void DecodeField(Register reg) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> Field::kShift;
    if (shift != 0) {
      shrp(reg, Immediate(shift));
    }
    andp(reg, Immediate(mask));
  }

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);

  // Abort execution if argument is not a smi, enabled via --debug-code.
  void AssertSmi(Register object);
  void AssertSmi(const Operand& object);

  // Abort execution if argument is not a FixedArray, enabled via --debug-code.
  void AssertFixedArray(Register object);

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
  void AssertUndefinedOrAllocationSite(Register object);

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new stack handler and link it into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  void PopStackHandler();

  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space or old space. If the given space
  // is exhausted control continues at the gc_required label. The allocated
  // object is returned in result and end of the new object is returned in
  // result_end. The register scratch can be passed as no_reg in which case
  // an additional object reference will be added to the reloc info. The
  // returned pointers in result and result_end have not yet been tagged as
  // heap objects. If result_contains_top_on_entry is true the content of
  // result is known to be the allocation top on entry (could be result_end
  // from a previous call). If result_contains_top_on_entry is true scratch
  // should be no_reg as it is never used.
  void Allocate(int object_size,
                Register result,
                Register result_end,
                Register scratch,
                Label* gc_required,
                AllocationFlags flags);

  // Allocate and initialize a JSValue wrapper with the specified {constructor}
  // and {value}.
  void AllocateJSValue(Register result, Register constructor, Register value,
                       Register scratch, Label* gc_required);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Machine code version of Map::GetConstructor().
  // |temp| holds |result|'s map when done.
  void GetMapConstructor(Register result, Register map, Register temp);

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst) {
    LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
  }

  // Load the native context slot with the current index.
  void LoadNativeContextSlot(int index, Register dst);

  // Load the initial map from the global function. The registers
  // function and map can be the same.
  void LoadGlobalFunctionInitialMap(Register function, Register map);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  // The code object is generated immediately, in contrast to
  // TurboAssembler::CallStubDelayed.
  void CallStub(CodeStub* stub);

  // Tail call a code stub (jump).
  void TailCallStub(CodeStub* stub);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f,
                   int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);

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

  // Convenience function: tail call a runtime routine (jump)
  void TailCallRuntime(Runtime::FunctionId fid);

  // Jump to a runtime routines
  void JumpToExternalReference(const ExternalReference& ext,
                               bool builtin_exit_frame = false);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int value);
  void DecrementCounter(StatsCounter* counter, int value);


  // ---------------------------------------------------------------------------
  // Debugging

  static int SafepointRegisterStackIndex(Register reg) {
    return SafepointRegisterStackIndex(reg.code());
  }

  void EnterBuiltinFrame(Register context, Register target, Register argc);
  void LeaveBuiltinFrame(Register context, Register target, Register argc);

 private:
  // Order general registers are pushed by Pushad.
  // rax, rcx, rdx, rbx, rsi, rdi, r8, r9, r11, r12, r14, r15.
  static const int kSafepointPushRegisterIndices[Register::kNumRegisters];
  static const int kNumSafepointSavedRegisters = 12;

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual, Label* done,
                      bool* definitely_mismatches, InvokeFlag flag,
                      Label::Distance near_jump);

  void EnterExitFramePrologue(bool save_rax, StackFrame::Type frame_type);

  // Allocates arg_stack_space * kPointerSize memory (not GCed) on the stack
  // accessible via StackSpaceOperand.
  void EnterExitFrameEpilogue(int arg_stack_space, bool save_doubles);

  void LeaveExitFrameEpilogue(bool restore_context);

  // Allocation support helpers.
  // Loads the top of new-space into the result register.
  // Otherwise the address of the new-space top is loaded into scratch (if
  // scratch is valid), and the new-space top is loaded into result.
  void LoadAllocationTopHelper(Register result,
                               Register scratch,
                               AllocationFlags flags);

  void MakeSureDoubleAlignedHelper(Register result,
                                   Register scratch,
                                   Label* gc_required,
                                   AllocationFlags flags);

  // Update allocation top with value in result_end register.
  // If scratch is valid, it contains the address of the allocation top.
  void UpdateAllocationTopHelper(Register result_end,
                                 Register scratch,
                                 AllocationFlags flags);

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

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code) {
    return kNumSafepointRegisters - kSafepointPushRegisterIndices[reg_code] - 1;
  }

  // Needs access to SafepointRegisterStackIndex for compiled frame
  // traversal.
  friend class StandardFrame;
};


// The code patcher is used to patch (typically) small parts of code e.g. for
// debugging and other types of instrumentation. When using the code patcher
// the exact number of bytes specified must be emitted. Is not legal to emit
// relocation information. If any of these constraints are violated it causes
// an assertion.
class CodePatcher {
 public:
  CodePatcher(Isolate* isolate, byte* address, int size);
  ~CodePatcher();

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


inline Operand ContextOperand(Register context, Register index) {
  return Operand(context, index, times_pointer_size, Context::SlotOffset(0));
}


inline Operand NativeContextOperand() {
  return ContextOperand(rsi, Context::NATIVE_CONTEXT_INDEX);
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


inline Operand StackOperandForReturnAddress(int32_t disp) {
  return Operand(rsp, disp);
}

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_X64_MACRO_ASSEMBLER_X64_H_
