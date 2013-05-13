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

#include "v8.h"

#if defined(V8_TARGET_ARCH_IA32)

#include "bootstrapper.h"
#include "codegen.h"
#include "debug.h"
#include "runtime.h"
#include "serialize.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MacroAssembler implementation.

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true),
      has_frame_(false) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


void MacroAssembler::InNewSpace(
    Register object,
    Register scratch,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  ASSERT(cc == equal || cc == not_equal);
  if (scratch.is(object)) {
    and_(scratch, Immediate(~Page::kPageAlignmentMask));
  } else {
    mov(scratch, Immediate(~Page::kPageAlignmentMask));
    and_(scratch, object);
  }
  // Check that we can use a test_b.
  ASSERT(MemoryChunk::IN_FROM_SPACE < 8);
  ASSERT(MemoryChunk::IN_TO_SPACE < 8);
  int mask = (1 << MemoryChunk::IN_FROM_SPACE)
           | (1 << MemoryChunk::IN_TO_SPACE);
  // If non-zero, the page belongs to new-space.
  test_b(Operand(scratch, MemoryChunk::kFlagsOffset),
         static_cast<uint8_t>(mask));
  j(cc, condition_met, condition_met_distance);
}


void MacroAssembler::RememberedSetHelper(
    Register object,  // Only used for debug checks.
    Register addr,
    Register scratch,
    SaveFPRegsMode save_fp,
    MacroAssembler::RememberedSetFinalAction and_then) {
  Label done;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok, Label::kNear);
    int3();
    bind(&ok);
  }
  // Load store buffer top.
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  mov(scratch, Operand::StaticVariable(store_buffer));
  // Store pointer to buffer.
  mov(Operand(scratch, 0), addr);
  // Increment buffer top.
  add(scratch, Immediate(kPointerSize));
  // Write back new top of buffer.
  mov(Operand::StaticVariable(store_buffer), scratch);
  // Call stub on end of buffer.
  // Check for end of buffer.
  test(scratch, Immediate(StoreBuffer::kStoreBufferOverflowBit));
  if (and_then == kReturnAtEnd) {
    Label buffer_overflowed;
    j(not_equal, &buffer_overflowed, Label::kNear);
    ret(0);
    bind(&buffer_overflowed);
  } else {
    ASSERT(and_then == kFallThroughAtEnd);
    j(equal, &done, Label::kNear);
  }
  StoreBufferOverflowStub store_buffer_overflow =
      StoreBufferOverflowStub(save_fp);
  CallStub(&store_buffer_overflow);
  if (and_then == kReturnAtEnd) {
    ret(0);
  } else {
    ASSERT(and_then == kFallThroughAtEnd);
    bind(&done);
  }
}


void MacroAssembler::ClampDoubleToUint8(XMMRegister input_reg,
                                        XMMRegister scratch_reg,
                                        Register result_reg) {
  Label done;
  Label conv_failure;
  pxor(scratch_reg, scratch_reg);
  cvtsd2si(result_reg, input_reg);
  test(result_reg, Immediate(0xFFFFFF00));
  j(zero, &done, Label::kNear);
  cmp(result_reg, Immediate(0x80000000));
  j(equal, &conv_failure, Label::kNear);
  mov(result_reg, Immediate(0));
  setcc(above, result_reg);
  sub(result_reg, Immediate(1));
  and_(result_reg, Immediate(255));
  jmp(&done, Label::kNear);
  bind(&conv_failure);
  Set(result_reg, Immediate(0));
  ucomisd(input_reg, scratch_reg);
  j(below, &done, Label::kNear);
  Set(result_reg, Immediate(255));
  bind(&done);
}


void MacroAssembler::ClampUint8(Register reg) {
  Label done;
  test(reg, Immediate(0xFFFFFF00));
  j(zero, &done, Label::kNear);
  setcc(negative, reg);  // 1 if negative, 0 if positive.
  dec_b(reg);  // 0 if negative, 255 if positive.
  bind(&done);
}


static double kUint32Bias =
    static_cast<double>(static_cast<uint32_t>(0xFFFFFFFF)) + 1;


void MacroAssembler::LoadUint32(XMMRegister dst,
                                Register src,
                                XMMRegister scratch) {
  Label done;
  cmp(src, Immediate(0));
  movdbl(scratch,
         Operand(reinterpret_cast<int32_t>(&kUint32Bias), RelocInfo::NONE32));
  cvtsi2sd(dst, src);
  j(not_sign, &done, Label::kNear);
  addsd(dst, scratch);
  bind(&done);
}


void MacroAssembler::RecordWriteArray(Register object,
                                      Register value,
                                      Register index,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    ASSERT_EQ(0, kSmiTag);
    test(value, Immediate(kSmiTagMask));
    j(zero, &done);
  }

  // Array access: calculate the destination address in the same manner as
  // KeyedStoreIC::GenerateGeneric.  Multiply a smi by 2 to get an offset
  // into an array of words.
  Register dst = index;
  lea(dst, Operand(object, index, times_half_pointer_size,
                   FixedArray::kHeaderSize - kHeapObjectTag));

  RecordWrite(
      object, dst, value, save_fp, remembered_set_action, OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(BitCast<int32_t>(kZapValue)));
    mov(index, Immediate(BitCast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWriteField(
    Register object,
    int offset,
    Register value,
    Register dst,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done, Label::kNear);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  ASSERT(IsAligned(offset, kPointerSize));

  lea(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    test_b(dst, (1 << kPointerSizeLog2) - 1);
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(
      object, dst, value, save_fp, remembered_set_action, OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(BitCast<int32_t>(kZapValue)));
    mov(dst, Immediate(BitCast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWriteForMap(
    Register object,
    Handle<Map> map,
    Register scratch1,
    Register scratch2,
    SaveFPRegsMode save_fp) {
  Label done;

  Register address = scratch1;
  Register value = scratch2;
  if (emit_debug_code()) {
    Label ok;
    lea(address, FieldOperand(object, HeapObject::kMapOffset));
    test_b(address, (1 << kPointerSizeLog2) - 1);
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  ASSERT(!object.is(value));
  ASSERT(!object.is(address));
  ASSERT(!value.is(address));
  AssertNotSmi(object);

  if (!FLAG_incremental_marking) {
    return;
  }

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  ASSERT(!isolate()->heap()->InNewSpace(*map));
  CheckPageFlagForMap(map,
                      MemoryChunk::kPointersToHereAreInterestingMask,
                      zero,
                      &done,
                      Label::kNear);

  // Delay the initialization of |address| and |value| for the stub until it's
  // known that the will be needed. Up until this point their values are not
  // needed since they are embedded in the operands of instructions that need
  // them.
  lea(address, FieldOperand(object, HeapObject::kMapOffset));
  mov(value, Immediate(map));
  RecordWriteStub stub(object, value, address, OMIT_REMEMBERED_SET, save_fp);
  CallStub(&stub);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(BitCast<int32_t>(kZapValue)));
    mov(scratch1, Immediate(BitCast<int32_t>(kZapValue)));
    mov(scratch2, Immediate(BitCast<int32_t>(kZapValue)));
  }
}


void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASSERT(!object.is(value));
  ASSERT(!object.is(address));
  ASSERT(!value.is(address));
  AssertNotSmi(object);

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    Label ok;
    cmp(value, Operand(address, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    // Skip barrier if writing a smi.
    JumpIfSmi(value, &done, Label::kNear);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  RecordWriteStub stub(object, value, address, remembered_set_action, fp_mode);
  CallStub(&stub);

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Immediate(BitCast<int32_t>(kZapValue)));
    mov(value, Immediate(BitCast<int32_t>(kZapValue)));
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  Set(eax, Immediate(0));
  mov(ebx, Immediate(ExternalReference(Runtime::kDebugBreak, isolate())));
  CEntryStub ces(1);
  call(ces.GetCode(isolate()), RelocInfo::DEBUG_BREAK);
}
#endif


void MacroAssembler::Set(Register dst, const Immediate& x) {
  if (x.is_zero()) {
    xor_(dst, dst);  // Shorter than mov.
  } else {
    mov(dst, x);
  }
}


void MacroAssembler::Set(const Operand& dst, const Immediate& x) {
  mov(dst, x);
}


bool MacroAssembler::IsUnsafeImmediate(const Immediate& x) {
  static const int kMaxImmediateBits = 17;
  if (!RelocInfo::IsNone(x.rmode_)) return false;
  return !is_intn(x.x_, kMaxImmediateBits);
}


void MacroAssembler::SafeSet(Register dst, const Immediate& x) {
  if (IsUnsafeImmediate(x) && jit_cookie() != 0) {
    Set(dst, Immediate(x.x_ ^ jit_cookie()));
    xor_(dst, jit_cookie());
  } else {
    Set(dst, x);
  }
}


void MacroAssembler::SafePush(const Immediate& x) {
  if (IsUnsafeImmediate(x) && jit_cookie() != 0) {
    push(Immediate(x.x_ ^ jit_cookie()));
    xor_(Operand(esp, 0), Immediate(jit_cookie()));
  } else {
    push(x);
  }
}


void MacroAssembler::CompareRoot(Register with, Heap::RootListIndex index) {
  // see ROOT_ACCESSOR macro in factory.h
  Handle<Object> value(&isolate()->heap()->roots_array_start()[index]);
  cmp(with, value);
}


void MacroAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  // see ROOT_ACCESSOR macro in factory.h
  Handle<Object> value(&isolate()->heap()->roots_array_start()[index]);
  cmp(with, value);
}


void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpb(FieldOperand(map, Map::kInstanceTypeOffset),
       static_cast<int8_t>(type));
}


void MacroAssembler::CheckFastElements(Register map,
                                       Label* fail,
                                       Label::Distance distance) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Map::kMaximumBitField2FastHoleyElementValue);
  j(above, fail, distance);
}


void MacroAssembler::CheckFastObjectElements(Register map,
                                             Label* fail,
                                             Label::Distance distance) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Map::kMaximumBitField2FastHoleySmiElementValue);
  j(below_equal, fail, distance);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Map::kMaximumBitField2FastHoleyElementValue);
  j(above, fail, distance);
}


void MacroAssembler::CheckFastSmiElements(Register map,
                                          Label* fail,
                                          Label::Distance distance) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Map::kMaximumBitField2FastHoleySmiElementValue);
  j(above, fail, distance);
}


void MacroAssembler::StoreNumberToDoubleElements(
    Register maybe_number,
    Register elements,
    Register key,
    Register scratch1,
    XMMRegister scratch2,
    Label* fail,
    bool specialize_for_processor,
    int elements_offset) {
  Label smi_value, done, maybe_nan, not_nan, is_nan, have_double_value;
  JumpIfSmi(maybe_number, &smi_value, Label::kNear);

  CheckMap(maybe_number,
           isolate()->factory()->heap_number_map(),
           fail,
           DONT_DO_SMI_CHECK);

  // Double value, canonicalize NaN.
  uint32_t offset = HeapNumber::kValueOffset + sizeof(kHoleNanLower32);
  cmp(FieldOperand(maybe_number, offset),
      Immediate(kNaNOrInfinityLowerBoundUpper32));
  j(greater_equal, &maybe_nan, Label::kNear);

  bind(&not_nan);
  ExternalReference canonical_nan_reference =
      ExternalReference::address_of_canonical_non_hole_nan();
  if (CpuFeatures::IsSupported(SSE2) && specialize_for_processor) {
    CpuFeatureScope use_sse2(this, SSE2);
    movdbl(scratch2, FieldOperand(maybe_number, HeapNumber::kValueOffset));
    bind(&have_double_value);
    movdbl(FieldOperand(elements, key, times_4,
                        FixedDoubleArray::kHeaderSize - elements_offset),
           scratch2);
  } else {
    fld_d(FieldOperand(maybe_number, HeapNumber::kValueOffset));
    bind(&have_double_value);
    fstp_d(FieldOperand(elements, key, times_4,
                        FixedDoubleArray::kHeaderSize - elements_offset));
  }
  jmp(&done);

  bind(&maybe_nan);
  // Could be NaN or Infinity. If fraction is not zero, it's NaN, otherwise
  // it's an Infinity, and the non-NaN code path applies.
  j(greater, &is_nan, Label::kNear);
  cmp(FieldOperand(maybe_number, HeapNumber::kValueOffset), Immediate(0));
  j(zero, &not_nan);
  bind(&is_nan);
  if (CpuFeatures::IsSupported(SSE2) && specialize_for_processor) {
    CpuFeatureScope use_sse2(this, SSE2);
    movdbl(scratch2, Operand::StaticVariable(canonical_nan_reference));
  } else {
    fld_d(Operand::StaticVariable(canonical_nan_reference));
  }
  jmp(&have_double_value, Label::kNear);

  bind(&smi_value);
  // Value is a smi. Convert to a double and store.
  // Preserve original value.
  mov(scratch1, maybe_number);
  SmiUntag(scratch1);
  if (CpuFeatures::IsSupported(SSE2) && specialize_for_processor) {
    CpuFeatureScope fscope(this, SSE2);
    cvtsi2sd(scratch2, scratch1);
    movdbl(FieldOperand(elements, key, times_4,
                        FixedDoubleArray::kHeaderSize - elements_offset),
           scratch2);
  } else {
    push(scratch1);
    fild_s(Operand(esp, 0));
    pop(scratch1);
    fstp_d(FieldOperand(elements, key, times_4,
                        FixedDoubleArray::kHeaderSize - elements_offset));
  }
  bind(&done);
}


void MacroAssembler::CompareMap(Register obj,
                                Handle<Map> map,
                                Label* early_success,
                                CompareMapMode mode) {
  cmp(FieldOperand(obj, HeapObject::kMapOffset), map);
  if (mode == ALLOW_ELEMENT_TRANSITION_MAPS) {
    ElementsKind kind = map->elements_kind();
    if (IsFastElementsKind(kind)) {
      bool packed = IsFastPackedElementsKind(kind);
      Map* current_map = *map;
      while (CanTransitionToMoreGeneralFastElementsKind(kind, packed)) {
        kind = GetNextMoreGeneralFastElementsKind(kind, packed);
        current_map = current_map->LookupElementsTransitionMap(kind);
        if (!current_map) break;
        j(equal, early_success, Label::kNear);
        cmp(FieldOperand(obj, HeapObject::kMapOffset),
            Handle<Map>(current_map));
      }
    }
  }
}


void MacroAssembler::CheckMap(Register obj,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type,
                              CompareMapMode mode) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, map, &success, mode);
  j(not_equal, fail);
  bind(&success);
}


void MacroAssembler::DispatchMap(Register obj,
                                 Register unused,
                                 Handle<Map> map,
                                 Handle<Code> success,
                                 SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  cmp(FieldOperand(obj, HeapObject::kMapOffset), Immediate(map));
  j(equal, success);

  bind(&fail);
}


Condition MacroAssembler::IsObjectStringType(Register heap_object,
                                             Register map,
                                             Register instance_type) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzx_b(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  test(instance_type, Immediate(kIsNotStringMask));
  return zero;
}


Condition MacroAssembler::IsObjectNameType(Register heap_object,
                                           Register map,
                                           Register instance_type) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzx_b(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  cmpb(instance_type, static_cast<uint8_t>(LAST_NAME_TYPE));
  return below_equal;
}


void MacroAssembler::IsObjectJSObjectType(Register heap_object,
                                          Register map,
                                          Register scratch,
                                          Label* fail) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map,
                                            Register scratch,
                                            Label* fail) {
  movzx_b(scratch, FieldOperand(map, Map::kInstanceTypeOffset));
  sub(scratch, Immediate(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  cmp(scratch,
      LAST_NONCALLABLE_SPEC_OBJECT_TYPE - FIRST_NONCALLABLE_SPEC_OBJECT_TYPE);
  j(above, fail);
}


void MacroAssembler::FCmp() {
  if (CpuFeatures::IsSupported(CMOV)) {
    fucomip();
    fstp(0);
  } else {
    fucompp();
    push(eax);
    fnstsw_ax();
    sahf();
    pop(eax);
  }
}


void MacroAssembler::AssertNumber(Register object) {
  if (emit_debug_code()) {
    Label ok;
    JumpIfSmi(object, &ok);
    cmp(FieldOperand(object, HeapObject::kMapOffset),
        isolate()->factory()->heap_number_map());
    Check(equal, "Operand not a number");
    bind(&ok);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(equal, "Operand is not a smi");
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, "Operand is a smi and not a string");
    push(object);
    mov(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(below, "Operand is not a string");
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, "Operand is a smi and not a name");
    push(object);
    mov(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, LAST_NAME_TYPE);
    pop(object);
    Check(below_equal, "Operand is not a name");
  }
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, "Operand is a smi");
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  push(ebp);
  mov(ebp, esp);
  push(esi);
  push(Immediate(Smi::FromInt(type)));
  push(Immediate(CodeObject()));
  if (emit_debug_code()) {
    cmp(Operand(esp, 0), Immediate(isolate()->factory()->undefined_value()));
    Check(not_equal, "code object not properly patched");
  }
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    cmp(Operand(ebp, StandardFrameConstants::kMarkerOffset),
        Immediate(Smi::FromInt(type)));
    Check(equal, "stack frame types must match");
  }
  leave();
}


void MacroAssembler::EnterExitFramePrologue() {
  // Set up the frame structure on the stack.
  ASSERT(ExitFrameConstants::kCallerSPDisplacement == +2 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerPCOffset == +1 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerFPOffset ==  0 * kPointerSize);
  push(ebp);
  mov(ebp, esp);

  // Reserve room for entry stack pointer and push the code object.
  ASSERT(ExitFrameConstants::kSPOffset  == -1 * kPointerSize);
  push(Immediate(0));  // Saved entry sp, patched before call.
  push(Immediate(CodeObject()));  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address(Isolate::kCEntryFPAddress,
                                       isolate());
  ExternalReference context_address(Isolate::kContextAddress,
                                    isolate());
  mov(Operand::StaticVariable(c_entry_fp_address), ebp);
  mov(Operand::StaticVariable(context_address), esi);
}


void MacroAssembler::EnterExitFrameEpilogue(int argc, bool save_doubles) {
  // Optionally save all XMM registers.
  if (save_doubles) {
    CpuFeatureScope scope(this, SSE2);
    int space = XMMRegister::kNumRegisters * kDoubleSize + argc * kPointerSize;
    sub(esp, Immediate(space));
    const int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movdbl(Operand(ebp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else {
    sub(esp, Immediate(argc * kPointerSize));
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    and_(esp, -kFrameAlignment);
  }

  // Patch the saved entry sp.
  mov(Operand(ebp, ExitFrameConstants::kSPOffset), esp);
}


void MacroAssembler::EnterExitFrame(bool save_doubles) {
  EnterExitFramePrologue();

  // Set up argc and argv in callee-saved registers.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  mov(edi, eax);
  lea(esi, Operand(ebp, eax, times_4, offset));

  // Reserve space for argc, argv and isolate.
  EnterExitFrameEpilogue(3, save_doubles);
}


void MacroAssembler::EnterApiExitFrame(int argc) {
  EnterExitFramePrologue();
  EnterExitFrameEpilogue(argc, false);
}


void MacroAssembler::LeaveExitFrame(bool save_doubles) {
  // Optionally restore all XMM registers.
  if (save_doubles) {
    CpuFeatureScope scope(this, SSE2);
    const int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movdbl(reg, Operand(ebp, offset - ((i + 1) * kDoubleSize)));
    }
  }

  // Get the return address from the stack and restore the frame pointer.
  mov(ecx, Operand(ebp, 1 * kPointerSize));
  mov(ebp, Operand(ebp, 0 * kPointerSize));

  // Pop the arguments and the receiver from the caller stack.
  lea(esp, Operand(esi, 1 * kPointerSize));

  // Push the return address to get ready to return.
  push(ecx);

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveExitFrameEpilogue() {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(Isolate::kContextAddress, isolate());
  mov(esi, Operand::StaticVariable(context_address));
#ifdef DEBUG
  mov(Operand::StaticVariable(context_address), Immediate(0));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(Isolate::kCEntryFPAddress,
                                       isolate());
  mov(Operand::StaticVariable(c_entry_fp_address), Immediate(0));
}


void MacroAssembler::LeaveApiExitFrame() {
  mov(esp, ebp);
  pop(ebp);

  LeaveExitFrameEpilogue();
}


void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // We will build up the handler from the bottom by pushing on the stack.
  // First push the frame pointer and context.
  if (kind == StackHandler::JS_ENTRY) {
    // The frame pointer does not point to a JS frame so we save NULL for
    // ebp. We expect the code throwing an exception to check ebp before
    // dereferencing it to restore the context.
    push(Immediate(0));  // NULL frame pointer.
    push(Immediate(Smi::FromInt(0)));  // No context.
  } else {
    push(ebp);
    push(esi);
  }
  // Push the state and the code object.
  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);
  push(Immediate(state));
  Push(CodeObject());

  // Link the current handler as the next handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  push(Operand::StaticVariable(handler_address));
  // Set this new handler as the current one.
  mov(Operand::StaticVariable(handler_address), esp);
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  pop(Operand::StaticVariable(handler_address));
  add(esp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


void MacroAssembler::JumpToHandlerEntry() {
  // Compute the handler entry address and jump to it.  The handler table is
  // a fixed array of (smi-tagged) code offsets.
  // eax = exception, edi = code object, edx = state.
  mov(ebx, FieldOperand(edi, Code::kHandlerTableOffset));
  shr(edx, StackHandler::kKindWidth);
  mov(edx, FieldOperand(ebx, edx, times_4, FixedArray::kHeaderSize));
  SmiUntag(edx);
  lea(edi, FieldOperand(edi, edx, times_1, Code::kHeaderSize));
  jmp(edi);
}


void MacroAssembler::Throw(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in eax.
  if (!value.is(eax)) {
    mov(eax, value);
  }
  // Drop the stack pointer to the top of the top handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  mov(esp, Operand::StaticVariable(handler_address));
  // Restore the next handler.
  pop(Operand::StaticVariable(handler_address));

  // Remove the code object and state, compute the handler address in edi.
  pop(edi);  // Code object.
  pop(edx);  // Index and state.

  // Restore the context and frame pointer.
  pop(esi);  // Context.
  pop(ebp);  // Frame pointer.

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (ebp == 0) == (esi == 0), so we could test either
  // ebp or esi.
  Label skip;
  test(esi, esi);
  j(zero, &skip, Label::kNear);
  mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
  bind(&skip);

  JumpToHandlerEntry();
}


void MacroAssembler::ThrowUncatchable(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in eax.
  if (!value.is(eax)) {
    mov(eax, value);
  }
  // Drop the stack pointer to the top of the top stack handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  mov(esp, Operand::StaticVariable(handler_address));

  // Unwind the handlers until the top ENTRY handler is found.
  Label fetch_next, check_kind;
  jmp(&check_kind, Label::kNear);
  bind(&fetch_next);
  mov(esp, Operand(esp, StackHandlerConstants::kNextOffset));

  bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  test(Operand(esp, StackHandlerConstants::kStateOffset),
       Immediate(StackHandler::KindField::kMask));
  j(not_zero, &fetch_next);

  // Set the top handler address to next handler past the top ENTRY handler.
  pop(Operand::StaticVariable(handler_address));

  // Remove the code object and state, compute the handler address in edi.
  pop(edi);  // Code object.
  pop(edx);  // Index and state.

  // Clear the context pointer and frame pointer (0 was saved in the handler).
  pop(esi);
  pop(ebp);

  JumpToHandlerEntry();
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss) {
  Label same_contexts;

  ASSERT(!holder_reg.is(scratch1));
  ASSERT(!holder_reg.is(scratch2));
  ASSERT(!scratch1.is(scratch2));

  // Load current lexical context from the stack frame.
  mov(scratch1, Operand(ebp, StandardFrameConstants::kContextOffset));

  // When generating debug code, make sure the lexical context is set.
  if (emit_debug_code()) {
    cmp(scratch1, Immediate(0));
    Check(not_equal, "we should not have an empty lexical context");
  }
  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  mov(scratch1, FieldOperand(scratch1, offset));
  mov(scratch1, FieldOperand(scratch1, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Read the first word and compare to native_context_map.
    cmp(FieldOperand(scratch1, HeapObject::kMapOffset),
        isolate()->factory()->native_context_map());
    Check(equal, "JSGlobalObject::native_context should be a native context.");
  }

  // Check if both contexts are the same.
  cmp(scratch1, FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  j(equal, &same_contexts);

  // Compare security tokens, save holder_reg on the stack so we can use it
  // as a temporary register.
  //
  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  mov(scratch2,
      FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    cmp(scratch2, isolate()->factory()->null_value());
    Check(not_equal, "JSGlobalProxy::context() should not be null.");

    // Read the first word and compare to native_context_map(),
    cmp(FieldOperand(scratch2, HeapObject::kMapOffset),
        isolate()->factory()->native_context_map());
    Check(equal, "JSGlobalObject::native_context should be a native context.");
  }

  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;
  mov(scratch1, FieldOperand(scratch1, token_offset));
  cmp(scratch1, FieldOperand(scratch2, token_offset));
  j(not_equal, miss);

  bind(&same_contexts);
}


// Compute the hash code from the untagged key.  This must be kept in sync
// with ComputeIntegerHash in utils.h.
//
// Note: r0 will contain hash code
void MacroAssembler::GetNumberHash(Register r0, Register scratch) {
  // Xor original key with a seed.
  if (Serializer::enabled()) {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    mov(scratch, Immediate(Heap::kHashSeedRootIndex));
    mov(scratch,
        Operand::StaticArray(scratch, times_pointer_size, roots_array_start));
    SmiUntag(scratch);
    xor_(r0, scratch);
  } else {
    int32_t seed = isolate()->heap()->HashSeed();
    xor_(r0, Immediate(seed));
  }

  // hash = ~hash + (hash << 15);
  mov(scratch, r0);
  not_(r0);
  shl(scratch, 15);
  add(r0, scratch);
  // hash = hash ^ (hash >> 12);
  mov(scratch, r0);
  shr(scratch, 12);
  xor_(r0, scratch);
  // hash = hash + (hash << 2);
  lea(r0, Operand(r0, r0, times_4, 0));
  // hash = hash ^ (hash >> 4);
  mov(scratch, r0);
  shr(scratch, 4);
  xor_(r0, scratch);
  // hash = hash * 2057;
  imul(r0, r0, 2057);
  // hash = hash ^ (hash >> 16);
  mov(scratch, r0);
  shr(scratch, 16);
  xor_(r0, scratch);
}



void MacroAssembler::LoadFromNumberDictionary(Label* miss,
                                              Register elements,
                                              Register key,
                                              Register r0,
                                              Register r1,
                                              Register r2,
                                              Register result) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver and is unchanged.
  //
  // key      - holds the smi key on entry and is unchanged.
  //
  // Scratch registers:
  //
  // r0 - holds the untagged key on entry and holds the hash once computed.
  //
  // r1 - used to hold the capacity mask of the dictionary
  //
  // r2 - used for the index into the dictionary.
  //
  // result - holds the result on exit if the load succeeds and we fall through.

  Label done;

  GetNumberHash(r0, r1);

  // Compute capacity mask.
  mov(r1, FieldOperand(elements, SeededNumberDictionary::kCapacityOffset));
  shr(r1, kSmiTagSize);  // convert smi to int
  dec(r1);

  // Generate an unrolled loop that performs a few probes before giving up.
  const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Use r2 for index calculations and keep the hash intact in r0.
    mov(r2, r0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      add(r2, Immediate(SeededNumberDictionary::GetProbeOffset(i)));
    }
    and_(r2, r1);

    // Scale the index by multiplying by the entry size.
    ASSERT(SeededNumberDictionary::kEntrySize == 3);
    lea(r2, Operand(r2, r2, times_2, 0));  // r2 = r2 * 3

    // Check if the key matches.
    cmp(key, FieldOperand(elements,
                          r2,
                          times_pointer_size,
                          SeededNumberDictionary::kElementsStartOffset));
    if (i != (kProbes - 1)) {
      j(equal, &done);
    } else {
      j(not_equal, miss);
    }
  }

  bind(&done);
  // Check that the value is a normal propety.
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  ASSERT_EQ(NORMAL, 0);
  test(FieldOperand(elements, r2, times_pointer_size, kDetailsOffset),
       Immediate(PropertyDetails::TypeField::kMask << kSmiTagSize));
  j(not_zero, miss);

  // Get the value at the masked, scaled index.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  mov(result, FieldOperand(elements, r2, times_pointer_size, kValueOffset));
}


void MacroAssembler::LoadAllocationTopHelper(Register result,
                                             Register scratch,
                                             AllocationFlags flags) {
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Just return if allocation top is already known.
  if ((flags & RESULT_CONTAINS_TOP) != 0) {
    // No use of scratch if allocation top is provided.
    ASSERT(scratch.is(no_reg));
#ifdef DEBUG
    // Assert that result actually contains top on entry.
    cmp(result, Operand::StaticVariable(allocation_top));
    Check(equal, "Unexpected allocation top");
#endif
    return;
  }

  // Move address of new object to result. Use scratch register if available.
  if (scratch.is(no_reg)) {
    mov(result, Operand::StaticVariable(allocation_top));
  } else {
    mov(scratch, Immediate(allocation_top));
    mov(result, Operand(scratch, 0));
  }
}


void MacroAssembler::UpdateAllocationTopHelper(Register result_end,
                                               Register scratch,
                                               AllocationFlags flags) {
  if (emit_debug_code()) {
    test(result_end, Immediate(kObjectAlignmentMask));
    Check(zero, "Unaligned allocation in new space");
  }

  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Update new top. Use scratch if available.
  if (scratch.is(no_reg)) {
    mov(Operand::StaticVariable(allocation_top), result_end);
  } else {
    mov(Operand(scratch, 0), result_end);
  }
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT((flags & (RESULT_CONTAINS_TOP | SIZE_IN_WORDS)) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      if (result_end.is_valid()) {
        mov(result_end, Immediate(0x7191));
      }
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  // Align the next allocation. Storing the filler map without checking top is
  // always safe because the limit of the heap is always aligned.
  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    ASSERT((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
    ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  Register top_reg = result_end.is_valid() ? result_end : result;

  // Calculate new top and bail out if space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  if (!top_reg.is(result)) {
    mov(top_reg, result);
  }
  add(top_reg, Immediate(object_size));
  j(carry, gc_required);
  cmp(top_reg, Operand::StaticVariable(allocation_limit));
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(top_reg, scratch, flags);

  // Tag result if requested.
  bool tag_result = (flags & TAG_OBJECT) != 0;
  if (top_reg.is(result)) {
    if (tag_result) {
      sub(result, Immediate(object_size - kHeapObjectTag));
    } else {
      sub(result, Immediate(object_size));
    }
  } else if (tag_result) {
    ASSERT(kHeapObjectTag == 1);
    inc(result);
  }
}


void MacroAssembler::Allocate(int header_size,
                              ScaleFactor element_size,
                              Register element_count,
                              RegisterValueType element_count_type,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT((flags & SIZE_IN_WORDS) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      mov(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
      // Register element_count is not modified by the function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  // Align the next allocation. Storing the filler map without checking top is
  // always safe because the limit of the heap is always aligned.
  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    ASSERT((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
    ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  // Calculate new top and bail out if space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  // We assume that element_count*element_size + header_size does not
  // overflow.
  if (element_count_type == REGISTER_VALUE_IS_SMI) {
    STATIC_ASSERT(static_cast<ScaleFactor>(times_2 - 1) == times_1);
    STATIC_ASSERT(static_cast<ScaleFactor>(times_4 - 1) == times_2);
    STATIC_ASSERT(static_cast<ScaleFactor>(times_8 - 1) == times_4);
    ASSERT(element_size >= times_2);
    ASSERT(kSmiTagSize == 1);
    element_size = static_cast<ScaleFactor>(element_size - 1);
  } else {
    ASSERT(element_count_type == REGISTER_VALUE_IS_INT32);
  }
  lea(result_end, Operand(element_count, element_size, header_size));
  add(result_end, result);
  j(carry, gc_required);
  cmp(result_end, Operand::StaticVariable(allocation_limit));
  j(above, gc_required);

  if ((flags & TAG_OBJECT) != 0) {
    ASSERT(kHeapObjectTag == 1);
    inc(result);
  }

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch, flags);
}


void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT((flags & (RESULT_CONTAINS_TOP | SIZE_IN_WORDS)) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      mov(result, Immediate(0x7091));
      mov(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        mov(scratch, Immediate(0x7291));
      }
      // object_size is left unchanged by this function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  // Align the next allocation. Storing the filler map without checking top is
  // always safe because the limit of the heap is always aligned.
  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    ASSERT((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
    ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    Label aligned;
    test(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    mov(Operand(result, 0),
        Immediate(isolate()->factory()->one_pointer_filler_map()));
    add(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }

  // Calculate new top and bail out if space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  if (!object_size.is(result_end)) {
    mov(result_end, object_size);
  }
  add(result_end, result);
  j(carry, gc_required);
  cmp(result_end, Operand::StaticVariable(allocation_limit));
  j(above, gc_required);

  // Tag result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    ASSERT(kHeapObjectTag == 1);
    inc(result);
  }

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch, flags);
}


void MacroAssembler::UndoAllocationInNewSpace(Register object) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  and_(object, Immediate(~kHeapObjectTagMask));
#ifdef DEBUG
  cmp(object, Operand::StaticVariable(new_space_allocation_top));
  Check(below, "Undo allocation of non allocated memory");
#endif
  mov(Operand::StaticVariable(new_space_allocation_top), object);
}


void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->heap_number_map()));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  ASSERT(kShortSize == 2);
  // scratch1 = length * 2 + kObjectAlignmentMask.
  lea(scratch1, Operand(length, length, times_1, kObjectAlignmentMask));
  and_(scratch1, Immediate(~kObjectAlignmentMask));

  // Allocate two byte string in new space.
  Allocate(SeqTwoByteString::kHeaderSize,
           times_1,
           scratch1,
           REGISTER_VALUE_IS_INT32,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->string_map()));
  mov(scratch1, length);
  SmiTag(scratch1);
  mov(FieldOperand(result, String::kLengthOffset), scratch1);
  mov(FieldOperand(result, String::kHashFieldOffset),
      Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  mov(scratch1, length);
  ASSERT(kCharSize == 1);
  add(scratch1, Immediate(kObjectAlignmentMask));
  and_(scratch1, Immediate(~kObjectAlignmentMask));

  // Allocate ASCII string in new space.
  Allocate(SeqOneByteString::kHeaderSize,
           times_1,
           scratch1,
           REGISTER_VALUE_IS_INT32,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->ascii_string_map()));
  mov(scratch1, length);
  SmiTag(scratch1);
  mov(FieldOperand(result, String::kLengthOffset), scratch1);
  mov(FieldOperand(result, String::kHashFieldOffset),
      Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         int length,
                                         Register scratch1,
                                         Register scratch2,
                                         Label* gc_required) {
  ASSERT(length > 0);

  // Allocate ASCII string in new space.
  Allocate(SeqOneByteString::SizeFor(length), result, scratch1, scratch2,
           gc_required, TAG_OBJECT);

  // Set the map, length and hash field.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->ascii_string_map()));
  mov(FieldOperand(result, String::kLengthOffset),
      Immediate(Smi::FromInt(length)));
  mov(FieldOperand(result, String::kHashFieldOffset),
      Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->cons_string_map()));
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  Label allocate_new_space, install_map;
  AllocationFlags flags = TAG_OBJECT;

  ExternalReference high_promotion_mode = ExternalReference::
      new_space_high_promotion_mode_active_address(isolate());

  test(Operand::StaticVariable(high_promotion_mode), Immediate(1));
  j(zero, &allocate_new_space);

  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           static_cast<AllocationFlags>(flags | PRETENURE_OLD_POINTER_SPACE));
  jmp(&install_map);

  bind(&allocate_new_space);
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           flags);

  bind(&install_map);
  // Set the map. The other fields are left uninitialized.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->cons_ascii_string_map()));
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                          Register scratch1,
                                          Register scratch2,
                                          Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->sliced_string_map()));
}


void MacroAssembler::AllocateAsciiSlicedString(Register result,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  mov(FieldOperand(result, HeapObject::kMapOffset),
      Immediate(isolate()->factory()->sliced_ascii_string_map()));
}


// Copy memory, byte-by-byte, from source to destination.  Not optimized for
// long or aligned copies.  The contents of scratch and length are destroyed.
// Source and destination are incremented by length.
// Many variants of movsb, loop unrolling, word moves, and indexed operands
// have been tried here already, and this is fastest.
// A simpler loop is faster on small copies, but 30% slower on large ones.
// The cld() instruction must have been emitted, to set the direction flag(),
// before calling this function.
void MacroAssembler::CopyBytes(Register source,
                               Register destination,
                               Register length,
                               Register scratch) {
  Label loop, done, short_string, short_loop;
  // Experimentation shows that the short string loop is faster if length < 10.
  cmp(length, Immediate(10));
  j(less_equal, &short_string);

  ASSERT(source.is(esi));
  ASSERT(destination.is(edi));
  ASSERT(length.is(ecx));

  // Because source is 4-byte aligned in our uses of this function,
  // we keep source aligned for the rep_movs call by copying the odd bytes
  // at the end of the ranges.
  mov(scratch, Operand(source, length, times_1, -4));
  mov(Operand(destination, length, times_1, -4), scratch);
  mov(scratch, ecx);
  shr(ecx, 2);
  rep_movs();
  and_(scratch, Immediate(0x3));
  add(destination, scratch);
  jmp(&done);

  bind(&short_string);
  test(length, length);
  j(zero, &done);

  bind(&short_loop);
  mov_b(scratch, Operand(source, 0));
  mov_b(Operand(destination, 0), scratch);
  inc(source);
  inc(destination);
  dec(length);
  j(not_zero, &short_loop);

  bind(&done);
}


void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label loop, entry;
  jmp(&entry);
  bind(&loop);
  mov(Operand(start_offset, 0), filler);
  add(start_offset, Immediate(kPointerSize));
  bind(&entry);
  cmp(start_offset, end_offset);
  j(less, &loop);
}


void MacroAssembler::BooleanBitTest(Register object,
                                    int field_offset,
                                    int bit_index) {
  bit_index += kSmiTagSize + kSmiShiftSize;
  ASSERT(IsPowerOf2(kBitsPerByte));
  int byte_index = bit_index / kBitsPerByte;
  int byte_bit_index = bit_index & (kBitsPerByte - 1);
  test_b(FieldOperand(object, field_offset + byte_index),
         static_cast<byte>(1 << byte_bit_index));
}



void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op,
                                      Label* then_label) {
  Label ok;
  test(result, result);
  j(not_zero, &ok);
  test(op, op);
  j(sign, then_label);
  bind(&ok);
}


void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op1,
                                      Register op2,
                                      Register scratch,
                                      Label* then_label) {
  Label ok;
  test(result, result);
  j(not_zero, &ok);
  mov(scratch, op1);
  or_(scratch, op2);
  j(sign, then_label);
  bind(&ok);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss,
                                             bool miss_on_bound_function) {
  // Check that the receiver isn't a smi.
  JumpIfSmi(function, miss);

  // Check that the function really is a function.
  CmpObjectType(function, JS_FUNCTION_TYPE, result);
  j(not_equal, miss);

  if (miss_on_bound_function) {
    // If a bound function, go to miss label.
    mov(scratch,
        FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
    BooleanBitTest(scratch, SharedFunctionInfo::kCompilerHintsOffset,
                   SharedFunctionInfo::kBoundFunction);
    j(not_zero, miss);
  }

  // Make sure that the function has an instance prototype.
  Label non_instance;
  movzx_b(scratch, FieldOperand(result, Map::kBitFieldOffset));
  test(scratch, Immediate(1 << Map::kHasNonInstancePrototype));
  j(not_zero, &non_instance);

  // Get the prototype or initial map from the function.
  mov(result,
      FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  cmp(result, Immediate(isolate()->factory()->the_hole_value()));
  j(equal, miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CmpObjectType(result, MAP_TYPE, scratch);
  j(not_equal, &done);

  // Get the prototype from the initial map.
  mov(result, FieldOperand(result, Map::kPrototypeOffset));
  jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  mov(result, FieldOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id) {
  ASSERT(AllowThisStubCall(stub));  // Calls are not allowed in some stubs.
  call(stub->GetCode(isolate()), RelocInfo::CODE_TARGET, ast_id);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls_ ||
         stub->CompilingCallsToThisStubIsGCSafe(isolate()));
  jmp(stub->GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  ASSERT(argc >= 1 && generating_stub());
  ret((argc - 1) * kPointerSize);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  if (!has_frame_ && stub->SometimesSetsUpAFrame()) return false;
  return allow_stub_calls_ || stub->CompilingCallsToThisStubIsGCSafe(isolate());
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    add(esp, Immediate(num_arguments * kPointerSize));
  }
  mov(eax, Immediate(isolate()->factory()->undefined_value()));
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // The assert checks that the constants for the maximum number of digits
  // for an array index cached in the hash field and the number of bits
  // reserved for it does not conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key.  kArrayIndexValueMask has zeros in
  // the low kHashShift bits.
  and_(hash, String::kArrayIndexValueMask);
  STATIC_ASSERT(String::kHashShift >= kSmiTagSize && kSmiTag == 0);
  if (String::kHashShift > kSmiTagSize) {
    shr(hash, String::kHashShift - kSmiTagSize);
  }
  if (!index.is(hash)) {
    mov(index, hash);
  }
}


void MacroAssembler::CallRuntime(Runtime::FunctionId id, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(id), num_arguments);
}


void MacroAssembler::CallRuntimeSaveDoubles(Runtime::FunctionId id) {
  const Runtime::Function* function = Runtime::FunctionForId(id);
  Set(eax, Immediate(function->nargs));
  mov(ebx, Immediate(ExternalReference(function, isolate())));
  CEntryStub ces(1, CpuFeatures::IsSupported(SSE2) ? kSaveFPRegs
                                                   : kDontSaveFPRegs);
  CallStub(&ces);
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments) {
  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    IllegalOperation(num_arguments);
    return;
  }

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ExternalReference(f, isolate())));
  CEntryStub ces(1);
  CallStub(&ces);
}


void MacroAssembler::CallExternalReference(ExternalReference ref,
                                           int num_arguments) {
  mov(eax, Immediate(num_arguments));
  mov(ebx, Immediate(ref));

  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(eax, Immediate(num_arguments));
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()),
                            num_arguments,
                            result_size);
}


// If true, a Handle<T> returned by value from a function with cdecl calling
// convention will be returned directly as a value of location_ field in a
// register eax.
// If false, it is returned as a pointer to a preallocated by caller memory
// region. Pointer to this region should be passed to a function as an
// implicit first argument.
#if defined(USING_BSD_ABI) || defined(__MINGW32__) || defined(__CYGWIN__)
static const bool kReturnHandlesDirectly = true;
#else
static const bool kReturnHandlesDirectly = false;
#endif


Operand ApiParameterOperand(int index) {
  return Operand(
      esp, (index + (kReturnHandlesDirectly ? 0 : 1)) * kPointerSize);
}


void MacroAssembler::PrepareCallApiFunction(int argc) {
  if (kReturnHandlesDirectly) {
    EnterApiExitFrame(argc);
    // When handles are returned directly we don't have to allocate extra
    // space for and pass an out parameter.
    if (emit_debug_code()) {
      mov(esi, Immediate(BitCast<int32_t>(kZapValue)));
    }
  } else {
    // We allocate two additional slots: return value and pointer to it.
    EnterApiExitFrame(argc + 2);

    // The argument slots are filled as follows:
    //
    //   n + 1: output slot
    //   n: arg n
    //   ...
    //   1: arg1
    //   0: pointer to the output slot

    lea(esi, Operand(esp, (argc + 1) * kPointerSize));
    mov(Operand(esp, 0 * kPointerSize), esi);
    if (emit_debug_code()) {
      mov(Operand(esi, 0), Immediate(0));
    }
  }
}


void MacroAssembler::CallApiFunctionAndReturn(Address function_address,
                                              int stack_space) {
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate());
  ExternalReference limit_address =
      ExternalReference::handle_scope_limit_address(isolate());
  ExternalReference level_address =
      ExternalReference::handle_scope_level_address(isolate());

  // Allocate HandleScope in callee-save registers.
  mov(ebx, Operand::StaticVariable(next_address));
  mov(edi, Operand::StaticVariable(limit_address));
  add(Operand::StaticVariable(level_address), Immediate(1));

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1, eax);
    mov(Operand(esp, 0),
        Immediate(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_enter_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  // Call the api function.
  call(function_address, RelocInfo::RUNTIME_ENTRY);

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1, eax);
    mov(Operand(esp, 0),
        Immediate(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_leave_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  if (!kReturnHandlesDirectly) {
    // PrepareCallApiFunction saved pointer to the output slot into
    // callee-save register esi.
    mov(eax, Operand(esi, 0));
  }

  Label empty_handle;
  Label prologue;
  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  // Check if the result handle holds 0.
  test(eax, eax);
  j(zero, &empty_handle);
  // It was non-zero.  Dereference to get the result value.
  mov(eax, Operand(eax, 0));
  bind(&prologue);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  mov(Operand::StaticVariable(next_address), ebx);
  sub(Operand::StaticVariable(level_address), Immediate(1));
  Assert(above_equal, "Invalid HandleScope level");
  cmp(edi, Operand::StaticVariable(limit_address));
  j(not_equal, &delete_allocated_handles);
  bind(&leave_exit_frame);

  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate());
  cmp(Operand::StaticVariable(scheduled_exception_address),
      Immediate(isolate()->factory()->the_hole_value()));
  j(not_equal, &promote_scheduled_exception);

#if ENABLE_EXTRA_CHECKS
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = eax;
  Register map = ecx;

  JumpIfSmi(return_value, &ok, Label::kNear);
  mov(map, FieldOperand(return_value, HeapObject::kMapOffset));

  CmpInstanceType(map, FIRST_NONSTRING_TYPE);
  j(below, &ok, Label::kNear);

  CmpInstanceType(map, FIRST_SPEC_OBJECT_TYPE);
  j(above_equal, &ok, Label::kNear);

  cmp(map, isolate()->factory()->heap_number_map());
  j(equal, &ok, Label::kNear);

  cmp(return_value, isolate()->factory()->undefined_value());
  j(equal, &ok, Label::kNear);

  cmp(return_value, isolate()->factory()->true_value());
  j(equal, &ok, Label::kNear);

  cmp(return_value, isolate()->factory()->false_value());
  j(equal, &ok, Label::kNear);

  cmp(return_value, isolate()->factory()->null_value());
  j(equal, &ok, Label::kNear);

  Abort("API call returned invalid object");

  bind(&ok);
#endif

  LeaveApiExitFrame();
  ret(stack_space * kPointerSize);

  bind(&empty_handle);
  // It was zero; the result is undefined.
  mov(eax, isolate()->factory()->undefined_value());
  jmp(&prologue);

  bind(&promote_scheduled_exception);
  TailCallRuntime(Runtime::kPromoteScheduledException, 0, 1);

  // HandleScope limit has changed. Delete allocated extensions.
  ExternalReference delete_extensions =
      ExternalReference::delete_handle_scope_extensions(isolate());
  bind(&delete_allocated_handles);
  mov(Operand::StaticVariable(limit_address), edi);
  mov(edi, eax);
  mov(Operand(esp, 0),
      Immediate(ExternalReference::isolate_address(isolate())));
  mov(eax, Immediate(delete_extensions));
  call(eax);
  mov(eax, edi);
  jmp(&leave_exit_frame);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& ext) {
  // Set the entry point and jump to the C entry runtime stub.
  mov(ebx, Immediate(ext));
  CEntryStub ces(1);
  jmp(ces.GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::SetCallKind(Register dst, CallKind call_kind) {
  // This macro takes the dst register to make the code more readable
  // at the call sites. However, the dst register has to be ecx to
  // follow the calling convention which requires the call type to be
  // in ecx.
  ASSERT(dst.is(ecx));
  if (call_kind == CALL_AS_FUNCTION) {
    // Set to some non-zero smi by updating the least significant
    // byte.
    mov_b(dst, 1 << kSmiTagSize);
  } else {
    // Set to smi zero by clearing the register.
    xor_(dst, dst);
  }
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    const Operand& code_operand,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance done_near,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label invoke;
  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      mov(eax, actual.immediate());
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        mov(ebx, expected.immediate());
      }
    }
  } else {
    if (actual.is_immediate()) {
      // Expected is in register, actual is immediate. This is the
      // case when we invoke function values without going through the
      // IC mechanism.
      cmp(expected.reg(), actual.immediate());
      j(equal, &invoke);
      ASSERT(expected.reg().is(ebx));
      mov(eax, actual.immediate());
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmp(expected.reg(), actual.reg());
      j(equal, &invoke);
      ASSERT(actual.reg().is(eax));
      ASSERT(expected.reg().is(ebx));
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor =
        isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (!code_constant.is_null()) {
      mov(edx, Immediate(code_constant));
      add(edx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    } else if (!code_operand.is_reg(edx)) {
      mov(edx, code_operand);
    }

    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor, RelocInfo::CODE_TARGET));
      SetCallKind(ecx, call_kind);
      call(adaptor, RelocInfo::CODE_TARGET);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        jmp(done, done_near);
      }
    } else {
      SetCallKind(ecx, call_kind);
      jmp(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}


void MacroAssembler::InvokeCode(const Operand& code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper,
                                CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, Handle<Code>::null(), code,
                 &done, &definitely_mismatches, flag, Label::kNear,
                 call_wrapper, call_kind);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      SetCallKind(ecx, call_kind);
      call(code);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      SetCallKind(ecx, call_kind);
      jmp(code);
    }
    bind(&done);
  }
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper,
                                CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;
  Operand dummy(eax, 0);
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, code, dummy, &done, &definitely_mismatches,
                 flag, Label::kNear, call_wrapper, call_kind);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code, rmode));
      SetCallKind(ecx, call_kind);
      call(code, rmode);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      SetCallKind(ecx, call_kind);
      jmp(code, rmode);
    }
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  ASSERT(fun.is(edi));
  mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  mov(ebx, FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  SmiUntag(ebx);

  ParameterCount expected(ebx);
  InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
             expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Get the function and setup the context.
  LoadHeapObject(edi, function);
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
             expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Rely on the assertion to check that the number of provided
  // arguments match the expected number of arguments. Fake a
  // parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  GetBuiltinFunction(edi, id);
  InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
             expected, expected, flag, call_wrapper, CALL_AS_METHOD);
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the JavaScript builtin function from the builtins object.
  mov(target, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  mov(target, FieldOperand(target, GlobalObject::kBuiltinsOffset));
  mov(target, FieldOperand(target,
                           JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(edi));
  // Load the JavaScript builtin function from the builtins object.
  GetBuiltinFunction(edi, id);
  // Load the code entry point from the function into the target register.
  mov(target, FieldOperand(edi, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    mov(dst, Operand(esi, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      mov(dst, Operand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    mov(dst, esi);
  }

  // We should not have found a with context by walking the context chain
  // (i.e., the static scope chain and runtime context chain do not agree).
  // A variable occurring in such a scope should have slot type LOOKUP and
  // not CONTEXT.
  if (emit_debug_code()) {
    cmp(FieldOperand(dst, HeapObject::kMapOffset),
        isolate()->factory()->with_context_map());
    Check(not_equal, "Variable resolved to with context.");
  }
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch,
    Label* no_map_match) {
  // Load the global or builtins object from the current context.
  mov(scratch, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  mov(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  mov(scratch, Operand(scratch,
                       Context::SlotOffset(Context::JS_ARRAY_MAPS_INDEX)));

  size_t offset = expected_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  cmp(map_in_out, FieldOperand(scratch, offset));
  j(not_equal, no_map_match);

  // Use the transitioned cached map.
  offset = transitioned_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  mov(map_in_out, FieldOperand(scratch, offset));
}


void MacroAssembler::LoadInitialArrayMap(
    Register function_in, Register scratch,
    Register map_out, bool can_have_holes) {
  ASSERT(!function_in.is(map_out));
  Label done;
  mov(map_out, FieldOperand(function_in,
                            JSFunction::kPrototypeOrInitialMapOffset));
  if (!FLAG_smi_only_arrays) {
    ElementsKind kind = can_have_holes ? FAST_HOLEY_ELEMENTS : FAST_ELEMENTS;
    LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                        kind,
                                        map_out,
                                        scratch,
                                        &done);
  } else if (can_have_holes) {
    LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                        FAST_HOLEY_SMI_ELEMENTS,
                                        map_out,
                                        scratch,
                                        &done);
  }
  bind(&done);
}


void MacroAssembler::LoadGlobalContext(Register global_context) {
  // Load the global or builtins object from the current context.
  mov(global_context,
      Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  mov(global_context,
      FieldOperand(global_context, GlobalObject::kNativeContextOffset));
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  mov(function,
      Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  mov(function,
      FieldOperand(function, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  mov(function, Operand(function, Context::SlotOffset(index)));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map) {
  // Load the initial map.  The global functions all have initial maps.
  mov(map, FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, isolate()->factory()->meta_map(), &fail, DO_SMI_CHECK);
    jmp(&ok);
    bind(&fail);
    Abort("Global functions must have initial map");
    bind(&ok);
  }
}


// Store the value in register src in the safepoint register stack
// slot for register dst.
void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Register src) {
  mov(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Immediate src) {
  mov(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  mov(dst, SafepointRegisterSlot(src));
}


Operand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return Operand(esp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the lowest encoding,
  // which means that lowest encodings are furthest away from
  // the stack pointer.
  ASSERT(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return kNumSafepointRegisters - reg_code - 1;
}


void MacroAssembler::LoadHeapObject(Register result,
                                    Handle<HeapObject> object) {
  ALLOW_HANDLE_DEREF(isolate(), "embedding raw address");
  if (isolate()->heap()->InNewSpace(*object)) {
    Handle<JSGlobalPropertyCell> cell =
        isolate()->factory()->NewJSGlobalPropertyCell(object);
    mov(result, Operand::Cell(cell));
  } else {
    mov(result, object);
  }
}


void MacroAssembler::PushHeapObject(Handle<HeapObject> object) {
  ALLOW_HANDLE_DEREF(isolate(), "using raw address");
  if (isolate()->heap()->InNewSpace(*object)) {
    Handle<JSGlobalPropertyCell> cell =
        isolate()->factory()->NewJSGlobalPropertyCell(object);
    push(Operand::Cell(cell));
  } else {
    Push(object);
  }
}


void MacroAssembler::Ret() {
  ret(0);
}


void MacroAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    pop(scratch);
    add(esp, Immediate(bytes_dropped));
    push(scratch);
    ret(0);
  }
}


void MacroAssembler::VerifyX87StackDepth(uint32_t depth) {
  // Make sure the floating point stack is either empty or has depth items.
  ASSERT(depth <= 7);

  // The top-of-stack (tos) is 7 if there is one item pushed.
  int tos = (8 - depth) % 8;
  const int kTopMask = 0x3800;
  push(eax);
  fwait();
  fnstsw_ax();
  and_(eax, kTopMask);
  shr(eax, 11);
  cmp(eax, Immediate(tos));
  Check(equal, "Unexpected FPU stack depth after instruction");
  fnclex();
  pop(eax);
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    add(esp, Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    mov(dst, src);
  }
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(Operand::StaticVariable(ExternalReference(counter)), Immediate(value));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      inc(operand);
    } else {
      add(operand, Immediate(value));
    }
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = Operand::StaticVariable(ExternalReference(counter));
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}


void MacroAssembler::IncrementCounter(Condition cc,
                                      StatsCounter* counter,
                                      int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Label skip;
    j(NegateCondition(cc), &skip);
    pushfd();
    IncrementCounter(counter, value);
    popfd();
    bind(&skip);
  }
}


void MacroAssembler::DecrementCounter(Condition cc,
                                      StatsCounter* counter,
                                      int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Label skip;
    j(NegateCondition(cc), &skip);
    pushfd();
    DecrementCounter(counter, value);
    popfd();
    bind(&skip);
  }
}


void MacroAssembler::Assert(Condition cc, const char* msg) {
  if (emit_debug_code()) Check(cc, msg);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    Factory* factory = isolate()->factory();
    Label ok;
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(factory->fixed_array_map()));
    j(equal, &ok);
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(factory->fixed_double_array_map()));
    j(equal, &ok);
    cmp(FieldOperand(elements, HeapObject::kMapOffset),
        Immediate(factory->fixed_cow_array_map()));
    j(equal, &ok);
    Abort("JSObject with fast elements map has slow elements");
    bind(&ok);
  }
}


void MacroAssembler::Check(Condition cc, const char* msg) {
  Label L;
  j(cc, &L);
  Abort(msg);
  // will not return here
  bind(&L);
}


void MacroAssembler::CheckStackAlignment() {
  int frame_alignment = OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    ASSERT(IsPowerOf2(frame_alignment));
    Label alignment_as_expected;
    test(esp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}


void MacroAssembler::Abort(const char* msg) {
  // We want to pass the msg string like a smi to avoid GC
  // problems, however msg is not guaranteed to be aligned
  // properly. Instead, we pass an aligned pointer that is
  // a proper v8 smi, but also pass the alignment difference
  // from the real pointer as a smi.
  intptr_t p1 = reinterpret_cast<intptr_t>(msg);
  intptr_t p0 = (p1 & ~kSmiTagMask) + kSmiTag;
  ASSERT(reinterpret_cast<Object*>(p0)->IsSmi());
#ifdef DEBUG
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }
#endif

  push(eax);
  push(Immediate(p0));
  push(Immediate(reinterpret_cast<intptr_t>(Smi::FromInt(p1 - p0))));
  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 2);
  } else {
    CallRuntime(Runtime::kAbort, 2);
  }
  // will not return here
  int3();
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  mov(descriptors, FieldOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  mov(dst, FieldOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::LoadPowerOf2(XMMRegister dst,
                                  Register scratch,
                                  int power) {
  ASSERT(is_uintn(power + HeapNumber::kExponentBias,
                  HeapNumber::kExponentBits));
  mov(scratch, Immediate(power + HeapNumber::kExponentBias));
  movd(dst, scratch);
  psllq(dst, HeapNumber::kMantissaBits);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(
    Register instance_type,
    Register scratch,
    Label* failure) {
  if (!scratch.is(instance_type)) {
    mov(scratch, instance_type);
  }
  and_(scratch,
       kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask);
  cmp(scratch, kStringTag | kSeqStringTag | kOneByteStringTag);
  j(not_equal, failure);
}


void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register object1,
                                                         Register object2,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Label* failure) {
  // Check that both objects are not smis.
  STATIC_ASSERT(kSmiTag == 0);
  mov(scratch1, object1);
  and_(scratch1, object2);
  JumpIfSmi(scratch1, failure);

  // Load instance type for both strings.
  mov(scratch1, FieldOperand(object1, HeapObject::kMapOffset));
  mov(scratch2, FieldOperand(object2, HeapObject::kMapOffset));
  movzx_b(scratch1, FieldOperand(scratch1, Map::kInstanceTypeOffset));
  movzx_b(scratch2, FieldOperand(scratch2, Map::kInstanceTypeOffset));

  // Check that both are flat ASCII strings.
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  // Interleave bits from both instance types and compare them in one check.
  ASSERT_EQ(0, kFlatAsciiStringMask & (kFlatAsciiStringMask << 3));
  and_(scratch1, kFlatAsciiStringMask);
  and_(scratch2, kFlatAsciiStringMask);
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmp(scratch1, kFlatAsciiStringTag | (kFlatAsciiStringTag << 3));
  j(not_equal, failure);
}


void MacroAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frame_alignment = OS::ActivationFrameAlignment();
  if (frame_alignment != 0) {
    // Make stack end at alignment and make room for num_arguments words
    // and the original value of esp.
    mov(scratch, esp);
    sub(esp, Immediate((num_arguments + 1) * kPointerSize));
    ASSERT(IsPowerOf2(frame_alignment));
    and_(esp, -frame_alignment);
    mov(Operand(esp, num_arguments * kPointerSize), scratch);
  } else {
    sub(esp, Immediate(num_arguments * kPointerSize));
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  // Trashing eax is ok as it will be the return value.
  mov(eax, Immediate(function));
  CallCFunction(eax, num_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_arguments) {
  ASSERT(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
    CheckStackAlignment();
  }

  call(function);
  if (OS::ActivationFrameAlignment() != 0) {
    mov(esp, Operand(esp, num_arguments * kPointerSize));
  } else {
    add(esp, Immediate(num_arguments * kPointerSize));
  }
}


bool AreAliased(Register r1, Register r2, Register r3, Register r4) {
  if (r1.is(r2)) return true;
  if (r1.is(r3)) return true;
  if (r1.is(r4)) return true;
  if (r2.is(r3)) return true;
  if (r2.is(r4)) return true;
  if (r3.is(r4)) return true;
  return false;
}


CodePatcher::CodePatcher(byte* address, int size)
    : address_(address),
      size_(size),
      masm_(NULL, address, size + Assembler::kGap) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  CPU::FlushICache(address_, size_);

  // Check that the code was patched as expected.
  ASSERT(masm_.pc_ == address_ + size_);
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,
    int mask,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  ASSERT(cc == zero || cc == not_zero);
  if (scratch.is(object)) {
    and_(scratch, Immediate(~Page::kPageAlignmentMask));
  } else {
    mov(scratch, Immediate(~Page::kPageAlignmentMask));
    and_(scratch, object);
  }
  if (mask < (1 << kBitsPerByte)) {
    test_b(Operand(scratch, MemoryChunk::kFlagsOffset),
           static_cast<uint8_t>(mask));
  } else {
    test(Operand(scratch, MemoryChunk::kFlagsOffset), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}


void MacroAssembler::CheckPageFlagForMap(
    Handle<Map> map,
    int mask,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  ASSERT(cc == zero || cc == not_zero);
  Page* page = Page::FromAddress(map->address());
  ExternalReference reference(ExternalReference::page_flags(page));
  // The inlined static address check of the page's flags relies
  // on maps never being compacted.
  ASSERT(!isolate()->heap()->mark_compact_collector()->
         IsOnEvacuationCandidate(*map));
  if (mask < (1 << kBitsPerByte)) {
    test_b(Operand::StaticVariable(reference), static_cast<uint8_t>(mask));
  } else {
    test(Operand::StaticVariable(reference), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}


void MacroAssembler::CheckMapDeprecated(Handle<Map> map,
                                        Register scratch,
                                        Label* if_deprecated) {
  if (map->CanBeDeprecated()) {
    mov(scratch, map);
    mov(scratch, FieldOperand(scratch, Map::kBitField3Offset));
    and_(scratch, Immediate(Smi::FromInt(Map::Deprecated::kMask)));
    j(not_zero, if_deprecated);
  }
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black,
                                 Label::Distance on_black_near) {
  HasColor(object, scratch0, scratch1,
           on_black, on_black_near,
           1, 0);  // kBlackBitPattern.
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register mask_scratch,
                              Label* has_color,
                              Label::Distance has_color_distance,
                              int first_bit,
                              int second_bit) {
  ASSERT(!AreAliased(object, bitmap_scratch, mask_scratch, ecx));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  j(first_bit == 1 ? zero : not_zero, &other_color, Label::kNear);
  add(mask_scratch, mask_scratch);  // Shift left 1 by adding.
  j(zero, &word_boundary, Label::kNear);
  test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  j(second_bit == 1 ? not_zero : zero, has_color, has_color_distance);
  jmp(&other_color, Label::kNear);

  bind(&word_boundary);
  test_b(Operand(bitmap_scratch, MemoryChunk::kHeaderSize + kPointerSize), 1);

  j(second_bit == 1 ? not_zero : zero, has_color, has_color_distance);
  bind(&other_color);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  ASSERT(!AreAliased(addr_reg, mask_reg, bitmap_reg, ecx));
  mov(bitmap_reg, Immediate(~Page::kPageAlignmentMask));
  and_(bitmap_reg, addr_reg);
  mov(ecx, addr_reg);
  int shift =
      Bitmap::kBitsPerCellLog2 + kPointerSizeLog2 - Bitmap::kBytesPerCellLog2;
  shr(ecx, shift);
  and_(ecx,
       (Page::kPageAlignmentMask >> shift) & ~(Bitmap::kBytesPerCell - 1));

  add(bitmap_reg, ecx);
  mov(ecx, addr_reg);
  shr(ecx, kPointerSizeLog2);
  and_(ecx, (1 << Bitmap::kBitsPerCellLog2) - 1);
  mov(mask_reg, Immediate(1));
  shl_cl(mask_reg);
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register mask_scratch,
    Label* value_is_white_and_not_data,
    Label::Distance distance) {
  ASSERT(!AreAliased(value, bitmap_scratch, mask_scratch, ecx));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  j(not_zero, &done, Label::kNear);

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    push(mask_scratch);
    // shl.  May overflow making the check conservative.
    add(mask_scratch, mask_scratch);
    test(mask_scratch, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
    pop(mask_scratch);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = ecx;  // Holds map while checking type.
  Register length = ecx;  // Holds length of object after checking type.
  Label not_heap_number;
  Label is_data_object;

  // Check for heap-number
  mov(map, FieldOperand(value, HeapObject::kMapOffset));
  cmp(map, FACTORY->heap_number_map());
  j(not_equal, &not_heap_number, Label::kNear);
  mov(length, Immediate(HeapNumber::kSize));
  jmp(&is_data_object, Label::kNear);

  bind(&not_heap_number);
  // Check for strings.
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = ecx;
  movzx_b(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  test_b(instance_type, kIsIndirectStringMask | kIsNotStringMask);
  j(not_zero, value_is_white_and_not_data);
  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  Label not_external;
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  ASSERT_EQ(0, kSeqStringTag & kExternalStringTag);
  ASSERT_EQ(0, kConsStringTag & kExternalStringTag);
  test_b(instance_type, kExternalStringTag);
  j(zero, &not_external, Label::kNear);
  mov(length, Immediate(ExternalString::kSize));
  jmp(&is_data_object, Label::kNear);

  bind(&not_external);
  // Sequential string, either ASCII or UC16.
  ASSERT(kOneByteStringTag == 0x04);
  and_(length, Immediate(kStringEncodingMask));
  xor_(length, Immediate(kStringEncodingMask));
  add(length, Immediate(0x04));
  // Value now either 4 (if ASCII) or 8 (if UC16), i.e., char-size shifted
  // by 2. If we multiply the string length as smi by this, it still
  // won't overflow a 32-bit value.
  ASSERT_EQ(SeqOneByteString::kMaxSize, SeqTwoByteString::kMaxSize);
  ASSERT(SeqOneByteString::kMaxSize <=
         static_cast<int>(0xffffffffu >> (2 + kSmiTagSize)));
  imul(length, FieldOperand(value, String::kLengthOffset));
  shr(length, 2 + kSmiTagSize + kSmiShiftSize);
  add(length, Immediate(SeqString::kHeaderSize + kObjectAlignmentMask));
  and_(length, Immediate(~kObjectAlignmentMask));

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  or_(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);

  and_(bitmap_scratch, Immediate(~Page::kPageAlignmentMask));
  add(Operand(bitmap_scratch, MemoryChunk::kLiveBytesOffset),
      length);
  if (emit_debug_code()) {
    mov(length, Operand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));
    cmp(length, Operand(bitmap_scratch, MemoryChunk::kSizeOffset));
    Check(less_equal, "Live Bytes Count overflow chunk size");
  }

  bind(&done);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  mov(dst, FieldOperand(map, Map::kBitField3Offset));
  and_(dst, Immediate(Smi::FromInt(Map::EnumLengthBits::kMask)));
}


void MacroAssembler::CheckEnumCache(Label* call_runtime) {
  Label next, start;
  mov(ecx, eax);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  mov(ebx, FieldOperand(ecx, HeapObject::kMapOffset));

  EnumLength(edx, ebx);
  cmp(edx, Immediate(Smi::FromInt(Map::kInvalidEnumCache)));
  j(equal, call_runtime);

  jmp(&start);

  bind(&next);
  mov(ebx, FieldOperand(ecx, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(edx, ebx);
  cmp(edx, Immediate(Smi::FromInt(0)));
  j(not_equal, call_runtime);

  bind(&start);

  // Check that there are no elements. Register rcx contains the current JS
  // object we've reached through the prototype chain.
  mov(ecx, FieldOperand(ecx, JSObject::kElementsOffset));
  cmp(ecx, isolate()->factory()->empty_fixed_array());
  j(not_equal, call_runtime);

  mov(ecx, FieldOperand(ebx, Map::kPrototypeOffset));
  cmp(ecx, isolate()->factory()->null_value());
  j(not_equal, &next);
}


void MacroAssembler::TestJSArrayForAllocationSiteInfo(
    Register receiver_reg,
    Register scratch_reg) {
  Label no_info_available;

  ExternalReference new_space_start =
      ExternalReference::new_space_start(isolate());
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  lea(scratch_reg, Operand(receiver_reg,
      JSArray::kSize + AllocationSiteInfo::kSize - kHeapObjectTag));
  cmp(scratch_reg, Immediate(new_space_start));
  j(less, &no_info_available);
  cmp(scratch_reg, Operand::StaticVariable(new_space_allocation_top));
  j(greater, &no_info_available);
  cmp(MemOperand(scratch_reg, -AllocationSiteInfo::kSize),
      Immediate(Handle<Map>(isolate()->heap()->allocation_site_info_map())));
  bind(&no_info_available);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
