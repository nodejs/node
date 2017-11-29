// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_MACRO_ASSEMBLER_IA32_H_
#define V8_IA32_MACRO_ASSEMBLER_IA32_H_

#include "src/assembler.h"
#include "src/bailout-reason.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Give alias names to registers for calling conventions.
const Register kReturnRegister0 = {Register::kCode_eax};
const Register kReturnRegister1 = {Register::kCode_edx};
const Register kReturnRegister2 = {Register::kCode_edi};
const Register kJSFunctionRegister = {Register::kCode_edi};
const Register kContextRegister = {Register::kCode_esi};
const Register kAllocateSizeRegister = {Register::kCode_edx};
const Register kInterpreterAccumulatorRegister = {Register::kCode_eax};
const Register kInterpreterBytecodeOffsetRegister = {Register::kCode_ecx};
const Register kInterpreterBytecodeArrayRegister = {Register::kCode_edi};
const Register kInterpreterDispatchTableRegister = {Register::kCode_esi};
const Register kJavaScriptCallArgCountRegister = {Register::kCode_eax};
const Register kJavaScriptCallNewTargetRegister = {Register::kCode_edx};
const Register kRuntimeCallFunctionRegister = {Register::kCode_ebx};
const Register kRuntimeCallArgCountRegister = {Register::kCode_eax};

// Convenience for platform-independent signatures.  We do not normally
// distinguish memory operands from other operands on ia32.
typedef Operand MemOperand;

enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum PointersToHereCheck {
  kPointersToHereMaybeInteresting,
  kPointersToHereAreAlwaysInteresting
};

enum RegisterValueType { REGISTER_VALUE_IS_SMI, REGISTER_VALUE_IS_INT32 };

enum class ReturnAddressState { kOnStack, kNotOnStack };

#ifdef DEBUG
bool AreAliased(Register reg1, Register reg2, Register reg3 = no_reg,
                Register reg4 = no_reg, Register reg5 = no_reg,
                Register reg6 = no_reg, Register reg7 = no_reg,
                Register reg8 = no_reg);
#endif

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

  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met,
                     Label::Distance condition_met_distance = Label::kFar);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void EnterFrame(StackFrame::Type type, bool load_constant_pool_pointer_reg) {
    // Out-of-line constant pool not implemented on ia32.
    UNREACHABLE();
  }
  void LeaveFrame(StackFrame::Type type);

  // Print a message to stdout and abort execution.
  void Abort(BailoutReason reason);

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, BailoutReason reason);

  // Like Assert(), but without condition.
  // Use --debug_code to enable.
  void AssertUnreachable(BailoutReason reason);

  // Like Assert(), but always enabled.
  void Check(Condition cc, BailoutReason reason);

  // Check that the stack is aligned.
  void CheckStackAlignment();

  // Nop, because ia32 does not have a root register.
  void InitializeRootRegister() {}

  // Move a constant into a destination using the most efficient encoding.
  void Move(Register dst, const Immediate& x);

  void Move(Register dst, Smi* source) { Move(dst, Immediate(source)); }

  // Move if the registers are not identical.
  void Move(Register target, Register source);

  void Move(const Operand& dst, const Immediate& x);

  // Move an immediate into an XMM register.
  void Move(XMMRegister dst, uint32_t src);
  void Move(XMMRegister dst, uint64_t src);
  void Move(XMMRegister dst, float src) { Move(dst, bit_cast<uint32_t>(src)); }
  void Move(XMMRegister dst, double src) { Move(dst, bit_cast<uint64_t>(src)); }

  void Move(Register dst, Handle<HeapObject> handle);

  void Call(Handle<Code> target, RelocInfo::Mode rmode) { call(target, rmode); }
  void Call(Label* target) { call(target); }

  void CallForDeoptimization(Address target, RelocInfo::Mode rmode) {
    call(target, rmode);
  }

  inline bool AllowThisStubCall(CodeStub* stub);
  void CallStubDelayed(CodeStub* stub);

  void CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                          SaveFPRegsMode save_doubles = kDontSaveFPRegs);

  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label,
                        Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(zero, smi_label, distance);
  }
  // Jump if the operand is a smi.
  inline void JumpIfSmi(Operand value, Label* smi_label,
                        Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(zero, smi_label, distance);
  }

  void SmiUntag(Register reg) { sar(reg, kSmiTagSize); }

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // |ra_state| defines whether return address is already pushed to stack or
  // not. Both |callee_args_count| and |caller_args_count_reg| do not include
  // receiver. |callee_args_count| is not modified, |caller_args_count_reg|
  // is trashed. |number_of_temp_values_after_return_address| specifies
  // the number of words pushed to the stack after the return address. This is
  // to allow "allocation" of scratch registers that this function requires
  // by saving their values on the stack.
  void PrepareForTailCall(const ParameterCount& callee_args_count,
                          Register caller_args_count_reg, Register scratch0,
                          Register scratch1, ReturnAddressState ra_state,
                          int number_of_temp_values_after_return_address);

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

  void ShlPair(Register high, Register low, uint8_t imm8);
  void ShlPair_cl(Register high, Register low);
  void ShrPair(Register high, Register low, uint8_t imm8);
  void ShrPair_cl(Register high, Register src);
  void SarPair(Register high, Register low, uint8_t imm8);
  void SarPair_cl(Register high, Register low);

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue();

  void Lzcnt(Register dst, Register src) { Lzcnt(dst, Operand(src)); }
  void Lzcnt(Register dst, const Operand& src);

  void Tzcnt(Register dst, Register src) { Tzcnt(dst, Operand(src)); }
  void Tzcnt(Register dst, const Operand& src);

  void Popcnt(Register dst, Register src) { Popcnt(dst, Operand(src)); }
  void Popcnt(Register dst, const Operand& src);

  void Ret();

  // Return and drop arguments from stack, where the number of arguments
  // may be bigger than 2^16 - 1.  Requires a scratch register.
  void Ret(int bytes_dropped, Register scratch);

  void Pshuflw(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
    Pshuflw(dst, Operand(src), shuffle);
  }
  void Pshuflw(XMMRegister dst, const Operand& src, uint8_t shuffle);
  void Pshufd(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
    Pshufd(dst, Operand(src), shuffle);
  }
  void Pshufd(XMMRegister dst, const Operand& src, uint8_t shuffle);

// SSE/SSE2 instructions with AVX version.
#define AVX_OP2_WITH_TYPE(macro_name, name, dst_type, src_type) \
  void macro_name(dst_type dst, src_type src) {                 \
    if (CpuFeatures::IsSupported(AVX)) {                        \
      CpuFeatureScope scope(this, AVX);                         \
      v##name(dst, src);                                        \
    } else {                                                    \
      name(dst, src);                                           \
    }                                                           \
  }

  AVX_OP2_WITH_TYPE(Movd, movd, XMMRegister, Register)
  AVX_OP2_WITH_TYPE(Movd, movd, XMMRegister, const Operand&)
  AVX_OP2_WITH_TYPE(Movd, movd, Register, XMMRegister)
  AVX_OP2_WITH_TYPE(Movd, movd, const Operand&, XMMRegister)

#undef AVX_OP2_WITH_TYPE

// Only use these macros when non-destructive source of AVX version is not
// needed.
#define AVX_OP3_WITH_TYPE(macro_name, name, dst_type, src_type) \
  void macro_name(dst_type dst, src_type src) {                 \
    if (CpuFeatures::IsSupported(AVX)) {                        \
      CpuFeatureScope scope(this, AVX);                         \
      v##name(dst, dst, src);                                   \
    } else {                                                    \
      name(dst, src);                                           \
    }                                                           \
  }
#define AVX_OP3_XO(macro_name, name)                            \
  AVX_OP3_WITH_TYPE(macro_name, name, XMMRegister, XMMRegister) \
  AVX_OP3_WITH_TYPE(macro_name, name, XMMRegister, const Operand&)

  AVX_OP3_XO(Pcmpeqd, pcmpeqd)
  AVX_OP3_XO(Psubd, psubd)
  AVX_OP3_XO(Pxor, pxor)

#undef AVX_OP3_XO
#undef AVX_OP3_WITH_TYPE

  // Non-SSE2 instructions.
  void Pshufb(XMMRegister dst, XMMRegister src) { Pshufb(dst, Operand(src)); }
  void Pshufb(XMMRegister dst, const Operand& src);
  void Psignd(XMMRegister dst, XMMRegister src) { Psignd(dst, Operand(src)); }
  void Psignd(XMMRegister dst, const Operand& src);

  void Pextrb(Register dst, XMMRegister src, int8_t imm8);
  void Pextrw(Register dst, XMMRegister src, int8_t imm8);
  void Pextrd(Register dst, XMMRegister src, int8_t imm8);
  void Pinsrd(XMMRegister dst, Register src, int8_t imm8,
              bool is_64_bits = false) {
    Pinsrd(dst, Operand(src), imm8, is_64_bits);
  }
  void Pinsrd(XMMRegister dst, const Operand& src, int8_t imm8,
              bool is_64_bits = false);

  void LoadUint32(XMMRegister dst, Register src) {
    LoadUint32(dst, Operand(src));
  }
  void LoadUint32(XMMRegister dst, const Operand& src);

  // Expression support
  // cvtsi2sd instruction only writes to the low 64-bit of dst register, which
  // hinders register renaming and makes dependence chains longer. So we use
  // xorps to clear the dst register before cvtsi2sd to solve this issue.
  void Cvtsi2sd(XMMRegister dst, Register src) { Cvtsi2sd(dst, Operand(src)); }
  void Cvtsi2sd(XMMRegister dst, const Operand& src);

  void Cvtui2ss(XMMRegister dst, Register src, Register tmp);

  void SlowTruncateToIDelayed(Zone* zone, Register result_reg,
                              Register input_reg,
                              int offset = HeapNumber::kValueOffset -
                                           kHeapObjectTag);

  void Push(Register src) { push(src); }
  void Push(const Operand& src) { push(src); }
  void Push(Immediate value) { push(value); }
  void Push(Handle<HeapObject> handle) { push(Immediate(handle)); }
  void Push(Smi* smi) { Push(Immediate(smi)); }

  // These functions do not arrange the registers in any particular order so
  // they are not useful for calls that can cause a GC.  The caller can
  // exclude up to 3 registers that do not need to be saved and restored.
  void PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                       Register exclusion2 = no_reg,
                       Register exclusion3 = no_reg);
  void PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                      Register exclusion2 = no_reg,
                      Register exclusion3 = no_reg);

 private:
  bool has_frame_ = false;
  Isolate* const isolate_;
  // This handle will be patched with the code object on installation.
  Handle<HeapObject> code_object_;
};

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler : public TurboAssembler {
 public:
  MacroAssembler(Isolate* isolate, void* buffer, int size,
                 CodeObjectRequired create_code_object);

  // Load a register with a long value as efficiently as possible.
  void Set(Register dst, int32_t x) {
    if (x == 0) {
      xor_(dst, dst);
    } else {
      mov(dst, Immediate(x));
    }
  }
  void Set(const Operand& dst, int32_t x) { mov(dst, Immediate(x)); }

  // Operations on roots in the root-array.
  void LoadRoot(Register destination, Heap::RootListIndex index);
  void CompareRoot(Register with, Register scratch, Heap::RootListIndex index);
  // These methods can only be used with constant roots (i.e. non-writable
  // and not in new space).
  void CompareRoot(Register with, Heap::RootListIndex index);
  void CompareRoot(const Operand& with, Heap::RootListIndex index);
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
  enum RememberedSetFinalAction { kReturnAtEnd, kFallThroughAtEnd };

  // Record in the remembered set the fact that we have a pointer to new space
  // at the address pointed to by the addr register.  Only works if addr is not
  // in new space.
  void RememberedSetHelper(Register object,  // Used for debug code.
                           Register addr, Register scratch,
                           SaveFPRegsMode save_fp,
                           RememberedSetFinalAction and_then);

  void CheckPageFlagForMap(
      Handle<Map> map, int mask, Condition cc, Label* condition_met,
      Label::Distance condition_met_distance = Label::kFar);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object, Register scratch, Label* branch,
                           Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, zero, branch, distance);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object, Register scratch, Label* branch,
                        Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, not_zero, branch, distance);
  }

  // Check if an object has a given incremental marking color.  Also uses ecx!
  void HasColor(Register object, Register scratch0, Register scratch1,
                Label* has_color, Label::Distance has_color_distance,
                int first_bit, int second_bit);

  void JumpIfBlack(Register object, Register scratch0, Register scratch1,
                   Label* on_black,
                   Label::Distance on_black_distance = Label::kFar);

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
      Register object, int offset, Register value, Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // As above, but the offset has the tag presubtracted.  For use with
  // Operand(reg, off).
  void RecordWriteContextSlot(
      Register context, int offset, Register value, Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting) {
    RecordWriteField(context, offset + kHeapObjectTag, value, scratch, save_fp,
                     remembered_set_action, smi_check,
                     pointers_to_here_check_for_value);
  }

  // Notify the garbage collector that we wrote a pointer into a fixed array.
  // |array| is the array being stored into, |value| is the
  // object being stored.  |index| is the array index represented as a
  // Smi. All registers are clobbered by the operation RecordWriteArray
  // filters out smis so it does not update the write barrier if the
  // value is a smi.
  void RecordWriteArray(
      Register array, Register value, Register index, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // For page containing |object| mark region covering |address|
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. The address and value registers are clobbered by the
  // operation. RecordWrite filters out smis so it does not update the
  // write barrier if the value is a smi.
  void RecordWrite(
      Register object, Register address, Register value, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // For page containing |object| mark the region covering the object's map
  // dirty. |object| is the object being stored into, |map| is the Map object
  // that was stored.
  void RecordWriteForMap(Register object, Handle<Map> map, Register scratch1,
                         Register scratch2, SaveFPRegsMode save_fp);

  // Frame restart support
  void MaybeDropFrames();

  // Enter specific kind of exit frame. Expects the number of
  // arguments in register eax and sets up the number of arguments in
  // register edi and the pointer to the first argument in register
  // esi.
  void EnterExitFrame(int argc, bool save_doubles, StackFrame::Type frame_type);

  void EnterApiExitFrame(int argc);

  // Leave the current exit frame. Expects the return value in
  // register eax:edx (untouched) and the pointer to the first
  // argument in register esi (if pop_arguments == true).
  void LeaveExitFrame(bool save_doubles, bool pop_arguments = true);

  // Leave the current exit frame. Expects the return value in
  // register eax (untouched).
  void LeaveApiExitFrame(bool restore_context);

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst);

  // Load the global function with the given index.
  void LoadGlobalFunction(int index, Register function);

  // Load the initial map from the global function. The registers
  // function and map can be the same.
  void LoadGlobalFunctionInitialMap(Register function, Register map);

  // Push and pop the registers that can hold pointers.
  void PushSafepointRegisters() { pushad(); }
  void PopSafepointRegisters() { popad(); }

  void CmpObject(Register reg, Handle<Object> object) {
    AllowDeferredHandleDereference heap_object_check;
    if (object->IsHeapObject()) {
      cmp(reg, Handle<HeapObject>::cast(object));
    } else {
      cmp(reg, Immediate(Smi::cast(*object)));
    }
  }

  void GetWeakValue(Register value, Handle<WeakCell> cell);

  // Load the value of the weak cell in the value register. Branch to the given
  // miss label if the weak cell was cleared.
  void LoadWeakValue(Register value, Handle<WeakCell> cell, Label* miss);

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

  void InvokeFunction(Register function, const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag);

  // Compare object type for heap object.
  // Incoming register is heap_object and outgoing register is map.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  void CmpInstanceType(Register map, InstanceType type);

  // Compare an object's map with the specified map.
  void CompareMap(Register obj, Handle<Map> map);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specified map.
  void CheckMap(Register obj, Handle<Map> map, Label* fail,
                SmiCheckType smi_check_type);

  void DoubleToI(Register result_reg, XMMRegister input_reg,
                 XMMRegister scratch, MinusZeroMode minus_zero_mode,
                 Label* lost_precision, Label* is_nan, Label* minus_zero,
                 Label::Distance dst = Label::kFar);

  // Smi tagging support.
  void SmiTag(Register reg) {
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    add(reg, reg);
  }

  // Modifies the register even if it does not contain a Smi!
  void UntagSmi(Register reg, Label* is_smi) {
    STATIC_ASSERT(kSmiTagSize == 1);
    sar(reg, kSmiTagSize);
    STATIC_ASSERT(kSmiTag == 0);
    j(not_carry, is_smi);
  }

  // Jump if register contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label,
                           Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(not_zero, not_smi_label, distance);
  }
  // Jump if the operand is not a smi.
  inline void JumpIfNotSmi(Operand value, Label* smi_label,
                           Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(not_zero, smi_label, distance);
  }

  void LoadInstanceDescriptors(Register map, Register descriptors);
  void LoadAccessor(Register dst, Register holder, int accessor_index,
                    AccessorComponent accessor);

  template<typename Field>
  void DecodeField(Register reg) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> Field::kShift;
    if (shift != 0) {
      sar(reg, shift);
    }
    and_(reg, Immediate(mask));
  }

  // Abort execution if argument is not a smi, enabled via --debug-code.
  void AssertSmi(Register object);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);

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
  void Allocate(int object_size, Register result, Register result_end,
                Register scratch, Label* gc_required, AllocationFlags flags);

  // Allocate and initialize a JSValue wrapper with the specified {constructor}
  // and {value}.
  void AllocateJSValue(Register result, Register constructor, Register value,
                       Register scratch, Label* gc_required);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Machine code version of Map::GetConstructor().
  // |temp| holds |result|'s map when done.
  void GetMapConstructor(Register result, Register map, Register temp);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.  Generate the code if necessary.
  void CallStub(CodeStub* stub);

  // Tail call a code stub (jump).  Generate the code if necessary.
  void TailCallStub(CodeStub* stub);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f, int num_arguments,
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

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& ext,
                               bool builtin_exit_frame = false);

  // ---------------------------------------------------------------------------
  // Utilities

  // Emit code that loads |parameter_index|'th parameter from the stack to
  // the register according to the CallInterfaceDescriptor definition.
  // |sp_to_caller_sp_offset_in_words| specifies the number of words pushed
  // below the caller's sp (on ia32 it's at least return address).
  template <class Descriptor>
  void LoadParameterFromStack(
      Register reg, typename Descriptor::ParameterIndices parameter_index,
      int sp_to_ra_offset_in_words = 1) {
    DCHECK(Descriptor::kPassLastArgsOnStack);
    DCHECK_LT(parameter_index, Descriptor::kParameterCount);
    DCHECK_LE(Descriptor::kParameterCount - Descriptor::kStackArgumentsCount,
              parameter_index);
    int offset = (Descriptor::kParameterCount - parameter_index - 1 +
                  sp_to_ra_offset_in_words) *
                 kPointerSize;
    mov(reg, Operand(esp, offset));
  }

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the esp register.
  void Drop(int element_count);

  void Jump(Handle<Code> target, RelocInfo::Mode rmode) { jmp(target, rmode); }
  void Pop(Register dst) { pop(dst); }
  void Pop(const Operand& dst) { pop(dst); }
  void PushReturnAddressFrom(Register src) { push(src); }
  void PopReturnAddressTo(Register dst) { pop(dst); }

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int value);
  void DecrementCounter(StatsCounter* counter, int value);
  void IncrementCounter(Condition cc, StatsCounter* counter, int value);
  void DecrementCounter(Condition cc, StatsCounter* counter, int value);

  // ---------------------------------------------------------------------------
  // String utilities.

  // Checks if both objects are sequential one-byte strings, and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialOneByteStrings(
      Register object1, Register object2, Register scratch1, Register scratch2,
      Label* on_not_flat_one_byte_strings);

  // Checks if the given register or operand is a unique name
  void JumpIfNotUniqueNameInstanceType(Register reg, Label* not_unique_name,
                                       Label::Distance distance = Label::kFar) {
    JumpIfNotUniqueNameInstanceType(Operand(reg), not_unique_name, distance);
  }

  void JumpIfNotUniqueNameInstanceType(Operand operand, Label* not_unique_name,
                                       Label::Distance distance = Label::kFar);

  static int SafepointRegisterStackIndex(Register reg) {
    return SafepointRegisterStackIndex(reg.code());
  }

  void EnterBuiltinFrame(Register context, Register target, Register argc);
  void LeaveBuiltinFrame(Register context, Register target, Register argc);

 private:
  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual, Label* done,
                      bool* definitely_mismatches, InvokeFlag flag,
                      Label::Distance done_distance);

  void EnterExitFramePrologue(StackFrame::Type frame_type);
  void EnterExitFrameEpilogue(int argc, bool save_doubles);

  void LeaveExitFrameEpilogue(bool restore_context);

  // Allocation support helpers.
  void LoadAllocationTopHelper(Register result, Register scratch,
                               AllocationFlags flags);

  void UpdateAllocationTopHelper(Register result_end, Register scratch,
                                 AllocationFlags flags);

  // Helper for implementing JumpIfNotInNewSpace and JumpIfInNewSpace.
  void InNewSpace(Register object, Register scratch, Condition cc,
                  Label* condition_met,
                  Label::Distance condition_met_distance = Label::kFar);

  // Helper for finding the mark bits for an address.  Afterwards, the
  // bitmap register points at the word with the mark bits and the mask
  // the position of the first bit.  Uses ecx as scratch and leaves addr_reg
  // unchanged.
  inline void GetMarkBits(Register addr_reg, Register bitmap_reg,
                          Register mask_reg);

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);

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
  byte* address_;        // The address of the code being patched.
  int size_;             // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
};

// -----------------------------------------------------------------------------
// Static helper functions.

// Generate an Operand for loading a field from an object.
inline Operand FieldOperand(Register object, int offset) {
  return Operand(object, offset - kHeapObjectTag);
}

// Generate an Operand for loading an indexed field from an object.
inline Operand FieldOperand(Register object, Register index, ScaleFactor scale,
                            int offset) {
  return Operand(object, index, scale, offset - kHeapObjectTag);
}

inline Operand FixedArrayElementOperand(Register array, Register index_as_smi,
                                        int additional_offset = 0) {
  int offset = FixedArray::kHeaderSize + additional_offset * kPointerSize;
  return FieldOperand(array, index_as_smi, times_half_pointer_size, offset);
}

inline Operand ContextOperand(Register context, int index) {
  return Operand(context, Context::SlotOffset(index));
}

inline Operand ContextOperand(Register context, Register index) {
  return Operand(context, index, times_pointer_size, Context::SlotOffset(0));
}

inline Operand NativeContextOperand() {
  return ContextOperand(esi, Context::NATIVE_CONTEXT_INDEX);
}

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_MACRO_ASSEMBLER_IA32_H_
