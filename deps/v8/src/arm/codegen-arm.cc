// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm/codegen-arm.h"

#if V8_TARGET_ARCH_ARM

#include <memory>

#include "src/arm/assembler-arm-inl.h"
#include "src/arm/simulator-arm.h"
#include "src/codegen.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


#define __ masm.

#if defined(V8_HOST_ARCH_ARM)
MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return stub;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

  Register dest = r0;
  Register src = r1;
  Register chars = r2;
  Register temp1 = r3;
  Label less_4;

  if (CpuFeatures::IsSupported(NEON)) {
    CpuFeatureScope scope(&masm, NEON);
    Label loop, less_256, less_128, less_64, less_32, _16_or_less, _8_or_less;
    Label size_less_than_8;
    __ pld(MemOperand(src, 0));

    __ cmp(chars, Operand(8));
    __ b(lt, &size_less_than_8);
    __ cmp(chars, Operand(32));
    __ b(lt, &less_32);
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 32));
    }
    __ cmp(chars, Operand(64));
    __ b(lt, &less_64);
    __ pld(MemOperand(src, 64));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 96));
    }
    __ cmp(chars, Operand(128));
    __ b(lt, &less_128);
    __ pld(MemOperand(src, 128));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 160));
    }
    __ pld(MemOperand(src, 192));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 224));
    }
    __ cmp(chars, Operand(256));
    __ b(lt, &less_256);
    __ sub(chars, chars, Operand(256));

    __ bind(&loop);
    __ pld(MemOperand(src, 256));
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 256));
    }
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ sub(chars, chars, Operand(64), SetCC);
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));
    __ b(ge, &loop);
    __ add(chars, chars, Operand(256));

    __ bind(&less_256);
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ sub(chars, chars, Operand(128));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));
    __ cmp(chars, Operand(64));
    __ b(lt, &less_64);

    __ bind(&less_128);
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ sub(chars, chars, Operand(64));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));

    __ bind(&less_64);
    __ cmp(chars, Operand(32));
    __ b(lt, &less_32);
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ sub(chars, chars, Operand(32));

    __ bind(&less_32);
    __ cmp(chars, Operand(16));
    __ b(le, &_16_or_less);
    __ vld1(Neon8, NeonListOperand(d0, 2), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0, 2), NeonMemOperand(dest, PostIndex));
    __ sub(chars, chars, Operand(16));

    __ bind(&_16_or_less);
    __ cmp(chars, Operand(8));
    __ b(le, &_8_or_less);
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0), NeonMemOperand(dest, PostIndex));
    __ sub(chars, chars, Operand(8));

    // Do a last copy which may overlap with the previous copy (up to 8 bytes).
    __ bind(&_8_or_less);
    __ rsb(chars, chars, Operand(8));
    __ sub(src, src, Operand(chars));
    __ sub(dest, dest, Operand(chars));
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src));
    __ vst1(Neon8, NeonListOperand(d0), NeonMemOperand(dest));

    __ Ret();

    __ bind(&size_less_than_8);

    __ bic(temp1, chars, Operand(0x3), SetCC);
    __ b(&less_4, eq);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ str(temp1, MemOperand(dest, 4, PostIndex));
  } else {
    Register temp2 = ip;
    Label loop;

    __ bic(temp2, chars, Operand(0x3), SetCC);
    __ b(&less_4, eq);
    __ add(temp2, dest, temp2);

    __ bind(&loop);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ str(temp1, MemOperand(dest, 4, PostIndex));
    __ cmp(dest, temp2);
    __ b(&loop, ne);
  }

  __ bind(&less_4);
  __ mov(chars, Operand(chars, LSL, 31), SetCC);
  // bit0 => Z (ne), bit1 => C (cs)
  __ ldrh(temp1, MemOperand(src, 2, PostIndex), cs);
  __ strh(temp1, MemOperand(dest, 2, PostIndex), cs);
  __ ldrb(temp1, MemOperand(src), ne);
  __ strb(temp1, MemOperand(dest), ne);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<MemCopyUint8Function>(buffer);
#endif
}


// Convert 8 to 16. The number of character to copy must be at least 8.
MemCopyUint16Uint8Function CreateMemCopyUint16Uint8Function(
    Isolate* isolate, MemCopyUint16Uint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return stub;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

  Register dest = r0;
  Register src = r1;
  Register chars = r2;
  if (CpuFeatures::IsSupported(NEON)) {
    CpuFeatureScope scope(&masm, NEON);
    Register temp = r3;
    Label loop;

    __ bic(temp, chars, Operand(0x7));
    __ sub(chars, chars, Operand(temp));
    __ add(temp, dest, Operand(temp, LSL, 1));

    __ bind(&loop);
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src, PostIndex));
    __ vmovl(NeonU8, q0, d0);
    __ vst1(Neon16, NeonListOperand(d0, 2), NeonMemOperand(dest, PostIndex));
    __ cmp(dest, temp);
    __ b(&loop, ne);

    // Do a last copy which will overlap with the previous copy (1 to 8 bytes).
    __ rsb(chars, chars, Operand(8));
    __ sub(src, src, Operand(chars));
    __ sub(dest, dest, Operand(chars, LSL, 1));
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src));
    __ vmovl(NeonU8, q0, d0);
    __ vst1(Neon16, NeonListOperand(d0, 2), NeonMemOperand(dest));
    __ Ret();
  } else {
    Register temp1 = r3;
    Register temp2 = ip;
    Register temp3 = lr;
    Register temp4 = r4;
    Label loop;
    Label not_two;

    __ Push(lr, r4);
    __ bic(temp2, chars, Operand(0x3));
    __ add(temp2, dest, Operand(temp2, LSL, 1));

    __ bind(&loop);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ uxtb16(temp3, temp1);
    __ uxtb16(temp4, temp1, 8);
    __ pkhbt(temp1, temp3, Operand(temp4, LSL, 16));
    __ str(temp1, MemOperand(dest));
    __ pkhtb(temp1, temp4, Operand(temp3, ASR, 16));
    __ str(temp1, MemOperand(dest, 4));
    __ add(dest, dest, Operand(8));
    __ cmp(dest, temp2);
    __ b(&loop, ne);

    __ mov(chars, Operand(chars, LSL, 31), SetCC);  // bit0 => ne, bit1 => cs
    __ b(&not_two, cc);
    __ ldrh(temp1, MemOperand(src, 2, PostIndex));
    __ uxtb(temp3, temp1, 8);
    __ mov(temp3, Operand(temp3, LSL, 16));
    __ uxtab(temp3, temp3, temp1);
    __ str(temp3, MemOperand(dest, 4, PostIndex));
    __ bind(&not_two);
    __ ldrb(temp1, MemOperand(src), ne);
    __ strh(temp1, MemOperand(dest), ne);
    __ Pop(pc, r4);
  }

  CodeDesc desc;
  masm.GetCode(&desc);

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);

  return FUNCTION_CAST<MemCopyUint16Uint8Function>(buffer);
#endif
}
#endif

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

  __ MovFromFloatParameter(d0);
  __ vsqrt(d0, d0);
  __ MovToFloatResult(d0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#endif
}

#undef __


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  DCHECK(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  DCHECK(masm->has_frame());
  masm->set_has_frame(false);
}


// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  Label indirect_string_loaded;
  __ bind(&indirect_string_loaded);

  // Fetch the instance type of the receiver into result register.
  __ ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ tst(result, Operand(kIsIndirectStringMask));
  __ b(eq, &check_sequential);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string, thin_string;
  __ and_(result, result, Operand(kStringRepresentationMask));
  __ cmp(result, Operand(kConsStringTag));
  __ b(eq, &cons_string);
  __ cmp(result, Operand(kThinStringTag));
  __ b(eq, &thin_string);

  // Handle slices.
  __ ldr(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ ldr(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ add(index, index, Operand::SmiUntag(result));
  __ jmp(&indirect_string_loaded);

  // Handle thin strings.
  __ bind(&thin_string);
  __ ldr(string, FieldMemOperand(string, ThinString::kActualOffset));
  __ jmp(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ ldr(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ CompareRoot(result, Heap::kempty_stringRootIndex);
  __ b(ne, call_runtime);
  // Get the first of the two strings and load its instance type.
  __ ldr(string, FieldMemOperand(string, ConsString::kFirstOffset));
  __ jmp(&indirect_string_loaded);

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label external_string, check_encoding;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result, Operand(kStringRepresentationMask));
  __ b(ne, &external_string);

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ add(string,
         string,
         Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ jmp(&check_encoding);

  // Handle external strings.
  __ bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ tst(result, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ tst(result, Operand(kShortExternalStringMask));
  __ b(ne, call_runtime);
  __ ldr(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ tst(result, Operand(kStringEncodingMask));
  __ b(ne, &one_byte);
  // Two-byte string.
  __ ldrh(result, MemOperand(string, index, LSL, 1));
  __ jmp(&done);
  __ bind(&one_byte);
  // One-byte string.
  __ ldrb(result, MemOperand(string, index));
  __ bind(&done);
}

#undef __

#ifdef DEBUG
// add(r0, pc, Operand(-8))
static const uint32_t kCodeAgePatchFirstInstruction = 0xe24f0008;
#endif

CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // Since patcher is a large object, allocate it dynamically when needed,
  // to avoid overloading the stack in stress conditions.
  // DONT_FLUSH is used because the CodeAgingHelper is initialized early in
  // the process, before ARM simulator ICache is setup.
  std::unique_ptr<CodePatcher> patcher(
      new CodePatcher(isolate, young_sequence_.start(),
                      young_sequence_.length() / Assembler::kInstrSize,
                      CodePatcher::DONT_FLUSH));
  PredictableCodeSizeScope scope(patcher->masm(), young_sequence_.length());
  patcher->masm()->PushStandardFrame(r1);
  patcher->masm()->nop(ip.code());
}


#ifdef DEBUG
bool CodeAgingHelper::IsOld(byte* candidate) const {
  return Memory::uint32_at(candidate) == kCodeAgePatchFirstInstruction;
}
#endif


bool Code::IsYoungSequence(Isolate* isolate, byte* sequence) {
  bool result = isolate->code_aging_helper()->IsYoung(sequence);
  DCHECK(result || isolate->code_aging_helper()->IsOld(sequence));
  return result;
}

Code::Age Code::GetCodeAge(Isolate* isolate, byte* sequence) {
  if (IsYoungSequence(isolate, sequence)) return kNoAgeCodeAge;

  Address target_address = Memory::Address_at(
      sequence + (kNoCodeAgeSequenceLength - Assembler::kInstrSize));
  Code* stub = GetCodeFromTargetAddress(target_address);
  return GetAgeOfCodeAgeStub(stub);
}

void Code::PatchPlatformCodeAge(Isolate* isolate, byte* sequence,
                                Code::Age age) {
  uint32_t young_length = isolate->code_aging_helper()->young_sequence_length();
  if (age == kNoAgeCodeAge) {
    isolate->code_aging_helper()->CopyYoungSequenceTo(sequence);
    Assembler::FlushICache(isolate, sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age);
    PatchingAssembler patcher(Assembler::IsolateData(isolate), sequence,
                              young_length / Assembler::kInstrSize);
    patcher.add(r0, pc, Operand(-8));
    patcher.ldr(pc, MemOperand(pc, -4));
    patcher.emit_code_stub_address(stub);
    patcher.FlushICache(isolate);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
