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

#include "codegen.h"
#include "heap.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  ASSERT(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  ASSERT(masm->has_frame());
  masm->set_has_frame(false);
}


#define __ masm.


UnaryMathFunction CreateTranscendentalFunction(TranscendentalCache::Type type) {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB,
                                                 &actual_size,
                                                 true));
  if (buffer == NULL) {
    // Fallback to library function if function cannot be created.
    switch (type) {
      case TranscendentalCache::SIN: return &sin;
      case TranscendentalCache::COS: return &cos;
      case TranscendentalCache::TAN: return &tan;
      case TranscendentalCache::LOG: return &log;
      default: UNIMPLEMENTED();
    }
  }

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));
  // esp[1 * kPointerSize]: raw double input
  // esp[0 * kPointerSize]: return address
  // Move double input into registers.

  __ push(ebx);
  __ push(edx);
  __ push(edi);
  __ fld_d(Operand(esp, 4 * kPointerSize));
  __ mov(ebx, Operand(esp, 4 * kPointerSize));
  __ mov(edx, Operand(esp, 5 * kPointerSize));
  TranscendentalCacheStub::GenerateOperation(&masm, type);
  // The return value is expected to be on ST(0) of the FPU stack.
  __ pop(edi);
  __ pop(edx);
  __ pop(ebx);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(desc.reloc_size == 0);

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
}


UnaryMathFunction CreateSqrtFunction() {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB,
                                                 &actual_size,
                                                 true));
  // If SSE2 is not available, we can use libc's implementation to ensure
  // consistency since code by fullcodegen's calls into runtime in that case.
  if (buffer == NULL || !CpuFeatures::IsSupported(SSE2)) return &sqrt;
  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));
  // esp[1 * kPointerSize]: raw double input
  // esp[0 * kPointerSize]: return address
  // Move double input into registers.
  {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(xmm0, Operand(esp, 1 * kPointerSize));
    __ sqrtsd(xmm0, xmm0);
    __ movdbl(Operand(esp, 1 * kPointerSize), xmm0);
    // Load result into floating point register as return value.
    __ fld_d(Operand(esp, 1 * kPointerSize));
    __ Ret();
  }

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(desc.reloc_size == 0);

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
}


static void MemCopyWrapper(void* dest, const void* src, size_t size) {
  memcpy(dest, src, size);
}


OS::MemCopyFunction CreateMemCopyFunction() {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB,
                                                 &actual_size,
                                                 true));
  if (buffer == NULL) return &MemCopyWrapper;
  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  // Generated code is put into a fixed, unmovable, buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // 32-bit C declaration function calls pass arguments on stack.

  // Stack layout:
  // esp[12]: Third argument, size.
  // esp[8]: Second argument, source pointer.
  // esp[4]: First argument, destination pointer.
  // esp[0]: return address

  const int kDestinationOffset = 1 * kPointerSize;
  const int kSourceOffset = 2 * kPointerSize;
  const int kSizeOffset = 3 * kPointerSize;

  int stack_offset = 0;  // Update if we change the stack height.

  if (FLAG_debug_code) {
    __ cmp(Operand(esp, kSizeOffset + stack_offset),
           Immediate(OS::kMinComplexMemCopy));
    Label ok;
    __ j(greater_equal, &ok);
    __ int3();
    __ bind(&ok);
  }
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope enable(SSE2);
    __ push(edi);
    __ push(esi);
    stack_offset += 2 * kPointerSize;
    Register dst = edi;
    Register src = esi;
    Register count = ecx;
    __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
    __ mov(src, Operand(esp, stack_offset + kSourceOffset));
    __ mov(count, Operand(esp, stack_offset + kSizeOffset));


    __ movdqu(xmm0, Operand(src, 0));
    __ movdqu(Operand(dst, 0), xmm0);
    __ mov(edx, dst);
    __ and_(edx, 0xF);
    __ neg(edx);
    __ add(edx, Immediate(16));
    __ add(dst, edx);
    __ add(src, edx);
    __ sub(count, edx);

    // edi is now aligned. Check if esi is also aligned.
    Label unaligned_source;
    __ test(src, Immediate(0x0F));
    __ j(not_zero, &unaligned_source);
    {
      // Copy loop for aligned source and destination.
      __ mov(edx, count);
      Register loop_count = ecx;
      Register count = edx;
      __ shr(loop_count, 5);
      {
        // Main copy loop.
        Label loop;
        __ bind(&loop);
        __ prefetch(Operand(src, 0x20), 1);
        __ movdqa(xmm0, Operand(src, 0x00));
        __ movdqa(xmm1, Operand(src, 0x10));
        __ add(src, Immediate(0x20));

        __ movdqa(Operand(dst, 0x00), xmm0);
        __ movdqa(Operand(dst, 0x10), xmm1);
        __ add(dst, Immediate(0x20));

        __ dec(loop_count);
        __ j(not_zero, &loop);
      }

      // At most 31 bytes to copy.
      Label move_less_16;
      __ test(count, Immediate(0x10));
      __ j(zero, &move_less_16);
      __ movdqa(xmm0, Operand(src, 0));
      __ add(src, Immediate(0x10));
      __ movdqa(Operand(dst, 0), xmm0);
      __ add(dst, Immediate(0x10));
      __ bind(&move_less_16);

      // At most 15 bytes to copy. Copy 16 bytes at end of string.
      __ and_(count, 0xF);
      __ movdqu(xmm0, Operand(src, count, times_1, -0x10));
      __ movdqu(Operand(dst, count, times_1, -0x10), xmm0);

      __ mov(eax, Operand(esp, stack_offset + kDestinationOffset));
      __ pop(esi);
      __ pop(edi);
      __ ret(0);
    }
    __ Align(16);
    {
      // Copy loop for unaligned source and aligned destination.
      // If source is not aligned, we can't read it as efficiently.
      __ bind(&unaligned_source);
      __ mov(edx, ecx);
      Register loop_count = ecx;
      Register count = edx;
      __ shr(loop_count, 5);
      {
        // Main copy loop
        Label loop;
        __ bind(&loop);
        __ prefetch(Operand(src, 0x20), 1);
        __ movdqu(xmm0, Operand(src, 0x00));
        __ movdqu(xmm1, Operand(src, 0x10));
        __ add(src, Immediate(0x20));

        __ movdqa(Operand(dst, 0x00), xmm0);
        __ movdqa(Operand(dst, 0x10), xmm1);
        __ add(dst, Immediate(0x20));

        __ dec(loop_count);
        __ j(not_zero, &loop);
      }

      // At most 31 bytes to copy.
      Label move_less_16;
      __ test(count, Immediate(0x10));
      __ j(zero, &move_less_16);
      __ movdqu(xmm0, Operand(src, 0));
      __ add(src, Immediate(0x10));
      __ movdqa(Operand(dst, 0), xmm0);
      __ add(dst, Immediate(0x10));
      __ bind(&move_less_16);

      // At most 15 bytes to copy. Copy 16 bytes at end of string.
      __ and_(count, 0x0F);
      __ movdqu(xmm0, Operand(src, count, times_1, -0x10));
      __ movdqu(Operand(dst, count, times_1, -0x10), xmm0);

      __ mov(eax, Operand(esp, stack_offset + kDestinationOffset));
      __ pop(esi);
      __ pop(edi);
      __ ret(0);
    }

  } else {
    // SSE2 not supported. Unlikely to happen in practice.
    __ push(edi);
    __ push(esi);
    stack_offset += 2 * kPointerSize;
    __ cld();
    Register dst = edi;
    Register src = esi;
    Register count = ecx;
    __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
    __ mov(src, Operand(esp, stack_offset + kSourceOffset));
    __ mov(count, Operand(esp, stack_offset + kSizeOffset));

    // Copy the first word.
    __ mov(eax, Operand(src, 0));
    __ mov(Operand(dst, 0), eax);

    // Increment src,dstso that dst is aligned.
    __ mov(edx, dst);
    __ and_(edx, 0x03);
    __ neg(edx);
    __ add(edx, Immediate(4));  // edx = 4 - (dst & 3)
    __ add(dst, edx);
    __ add(src, edx);
    __ sub(count, edx);
    // edi is now aligned, ecx holds number of remaning bytes to copy.

    __ mov(edx, count);
    count = edx;
    __ shr(ecx, 2);  // Make word count instead of byte count.
    __ rep_movs();

    // At most 3 bytes left to copy. Copy 4 bytes at end of string.
    __ and_(count, 3);
    __ mov(eax, Operand(src, count, times_1, -4));
    __ mov(Operand(dst, count, times_1, -4), eax);

    __ mov(eax, Operand(esp, stack_offset + kDestinationOffset));
    __ pop(esi);
    __ pop(edi);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(desc.reloc_size == 0);

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<OS::MemCopyFunction>(buffer);
}

#undef __

// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateSmiOnlyToObject(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ebx    : target map
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  // Set transitioned map.
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiOnlyToDouble(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ebx    : target map
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map;

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(edi, Immediate(masm->isolate()->factory()->empty_fixed_array()));
  __ j(equal, &only_change_map);

  __ push(eax);
  __ push(ebx);

  __ mov(edi, FieldOperand(edi, FixedArray::kLengthOffset));

  // Allocate new FixedDoubleArray.
  // edx: receiver
  // edi: length of source FixedArray (smi-tagged)
  __ lea(esi, Operand(edi,
                      times_4,
                      FixedDoubleArray::kHeaderSize + kPointerSize));
  __ AllocateInNewSpace(esi, eax, ebx, no_reg, &gc_required, TAG_OBJECT);

  Label aligned, aligned_done;
  __ test(eax, Immediate(kDoubleAlignmentMask - kHeapObjectTag));
  __ j(zero, &aligned, Label::kNear);
  __ mov(FieldOperand(eax, 0),
         Immediate(masm->isolate()->factory()->one_pointer_filler_map()));
  __ add(eax, Immediate(kPointerSize));
  __ jmp(&aligned_done);

  __ bind(&aligned);
  __ mov(Operand(eax, esi, times_1, -kPointerSize-1),
         Immediate(masm->isolate()->factory()->one_pointer_filler_map()));

  __ bind(&aligned_done);

  // eax: destination FixedDoubleArray
  // edi: number of elements
  // edx: receiver
  __ mov(FieldOperand(eax, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->fixed_double_array_map()));
  __ mov(FieldOperand(eax, FixedDoubleArray::kLengthOffset), edi);
  __ mov(esi, FieldOperand(edx, JSObject::kElementsOffset));
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ mov(FieldOperand(edx, JSObject::kElementsOffset), eax);
  __ mov(ebx, eax);
  __ RecordWriteField(edx,
                      JSObject::kElementsOffset,
                      ebx,
                      edi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  __ mov(edi, FieldOperand(esi, FixedArray::kLengthOffset));

  // Prepare for conversion loop.
  ExternalReference canonical_the_hole_nan_reference =
      ExternalReference::address_of_the_hole_nan();
  XMMRegister the_hole_nan = xmm1;
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(the_hole_nan,
              Operand::StaticVariable(canonical_the_hole_nan_reference));
  }
  __ jmp(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  // Restore registers before jumping into runtime.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ pop(ebx);
  __ pop(eax);
  __ jmp(fail);

  // Convert and copy elements
  // esi: source FixedArray
  __ bind(&loop);
  __ mov(ebx, FieldOperand(esi, edi, times_2, FixedArray::kHeaderSize));
  // ebx: current element from source
  // edi: index of current element
  __ JumpIfNotSmi(ebx, &convert_hole);

  // Normal smi, convert it to double and store.
  __ SmiUntag(ebx);
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope fscope(SSE2);
    __ cvtsi2sd(xmm0, ebx);
    __ movdbl(FieldOperand(eax, edi, times_4, FixedDoubleArray::kHeaderSize),
              xmm0);
  } else {
    __ push(ebx);
    __ fild_s(Operand(esp, 0));
    __ pop(ebx);
    __ fstp_d(FieldOperand(eax, edi, times_4, FixedDoubleArray::kHeaderSize));
  }
  __ jmp(&entry);

  // Found hole, store hole_nan_as_double instead.
  __ bind(&convert_hole);

  if (FLAG_debug_code) {
    __ cmp(ebx, masm->isolate()->factory()->the_hole_value());
    __ Assert(equal, "object found in smi-only array");
  }

  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(FieldOperand(eax, edi, times_4, FixedDoubleArray::kHeaderSize),
              the_hole_nan);
  } else {
    __ fld_d(Operand::StaticVariable(canonical_the_hole_nan_reference));
    __ fstp_d(FieldOperand(eax, edi, times_4, FixedDoubleArray::kHeaderSize));
  }

  __ bind(&entry);
  __ sub(edi, Immediate(Smi::FromInt(1)));
  __ j(not_sign, &loop);

  __ pop(ebx);
  __ pop(eax);

  // Restore esi.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  __ bind(&only_change_map);
  // eax: value
  // ebx: target map
  // Set transitioned map.
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ebx    : target map
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map, success;

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(edi, Immediate(masm->isolate()->factory()->empty_fixed_array()));
  __ j(equal, &only_change_map);

  __ push(eax);
  __ push(edx);
  __ push(ebx);

  __ mov(ebx, FieldOperand(edi, FixedDoubleArray::kLengthOffset));

  // Allocate new FixedArray.
  // ebx: length of source FixedDoubleArray (smi-tagged)
  __ lea(edi, Operand(ebx, times_2, FixedArray::kHeaderSize));
  __ AllocateInNewSpace(edi, eax, esi, no_reg, &gc_required, TAG_OBJECT);

  // eax: destination FixedArray
  // ebx: number of elements
  __ mov(FieldOperand(eax, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->fixed_array_map()));
  __ mov(FieldOperand(eax, FixedArray::kLengthOffset), ebx);
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));

  __ jmp(&entry);

  // ebx: target map
  // edx: receiver
  // Set transitioned map.
  __ bind(&only_change_map);
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&success);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ pop(ebx);
  __ pop(edx);
  __ pop(eax);
  __ jmp(fail);

  // Box doubles into heap numbers.
  // edi: source FixedDoubleArray
  // eax: destination FixedArray
  __ bind(&loop);
  // ebx: index of current element (smi-tagged)
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmp(FieldOperand(edi, ebx, times_4, offset), Immediate(kHoleNanUpper32));
  __ j(equal, &convert_hole);

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(edx, esi, no_reg, &gc_required);
  // edx: new heap number
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope fscope(SSE2);
    __ movdbl(xmm0,
              FieldOperand(edi, ebx, times_4, FixedDoubleArray::kHeaderSize));
    __ movdbl(FieldOperand(edx, HeapNumber::kValueOffset), xmm0);
  } else {
    __ mov(esi, FieldOperand(edi, ebx, times_4, FixedDoubleArray::kHeaderSize));
    __ mov(FieldOperand(edx, HeapNumber::kValueOffset), esi);
    __ mov(esi, FieldOperand(edi, ebx, times_4, offset));
    __ mov(FieldOperand(edx, HeapNumber::kValueOffset + kPointerSize), esi);
  }
  __ mov(FieldOperand(eax, ebx, times_2, FixedArray::kHeaderSize), edx);
  __ mov(esi, ebx);
  __ RecordWriteArray(eax,
                      edx,
                      esi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&entry, Label::kNear);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ mov(FieldOperand(eax, ebx, times_2, FixedArray::kHeaderSize),
         masm->isolate()->factory()->the_hole_value());

  __ bind(&entry);
  __ sub(ebx, Immediate(Smi::FromInt(1)));
  __ j(not_sign, &loop);

  __ pop(ebx);
  __ pop(edx);
  // ebx: target map
  // edx: receiver
  // Set transitioned map.
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ mov(FieldOperand(edx, JSObject::kElementsOffset), eax);
  __ RecordWriteField(edx,
                      JSObject::kElementsOffset,
                      eax,
                      edi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Restore registers.
  __ pop(eax);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  __ bind(&success);
}


void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Factory* factory,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  // Fetch the instance type of the receiver into result register.
  __ mov(result, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(result, FieldOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ test(result, Immediate(kIsIndirectStringMask));
  __ j(zero, &check_sequential, Label::kNear);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ test(result, Immediate(kSlicedNotConsMask));
  __ j(zero, &cons_string, Label::kNear);

  // Handle slices.
  Label indirect_string_loaded;
  __ mov(result, FieldOperand(string, SlicedString::kOffsetOffset));
  __ SmiUntag(result);
  __ add(index, result);
  __ mov(string, FieldOperand(string, SlicedString::kParentOffset));
  __ jmp(&indirect_string_loaded, Label::kNear);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ cmp(FieldOperand(string, ConsString::kSecondOffset),
         Immediate(factory->empty_string()));
  __ j(not_equal, call_runtime);
  __ mov(string, FieldOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ mov(result, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(result, FieldOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label seq_string;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test(result, Immediate(kStringRepresentationMask));
  __ j(zero, &seq_string, Label::kNear);

  // Handle external strings.
  Label ascii_external, done;
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ test(result, Immediate(kIsIndirectStringMask));
    __ Assert(zero, "external string expected, but not found");
  }
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ test_b(result, kShortExternalStringMask);
  __ j(not_zero, call_runtime);
  // Check encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ test_b(result, kStringEncodingMask);
  __ mov(result, FieldOperand(string, ExternalString::kResourceDataOffset));
  __ j(not_equal, &ascii_external, Label::kNear);
  // Two-byte string.
  __ movzx_w(result, Operand(result, index, times_2, 0));
  __ jmp(&done, Label::kNear);
  __ bind(&ascii_external);
  // Ascii string.
  __ movzx_b(result, Operand(result, index, times_1, 0));
  __ jmp(&done, Label::kNear);

  // Dispatch on the encoding: ASCII or two-byte.
  Label ascii;
  __ bind(&seq_string);
  STATIC_ASSERT((kStringEncodingMask & kAsciiStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ test(result, Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii, Label::kNear);

  // Two-byte string.
  // Load the two-byte character code into the result register.
  __ movzx_w(result, FieldOperand(string,
                                  index,
                                  times_2,
                                  SeqTwoByteString::kHeaderSize));
  __ jmp(&done, Label::kNear);

  // Ascii string.
  // Load the byte into the result register.
  __ bind(&ascii);
  __ movzx_b(result, FieldOperand(string,
                                  index,
                                  times_1,
                                  SeqAsciiString::kHeaderSize));
  __ bind(&done);
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
