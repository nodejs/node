// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_X64)

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "assembler-x64.h"
#include "macro-assembler-x64.h"
#include "serialize.h"
#include "debug.h"
#include "heap.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(void* buffer, int size)
    : Assembler(buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true),
      code_object_(Heap::undefined_value()) {
}


void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index) {
  movq(destination, Operand(kRootRegister, index << kPointerSizeLog2));
}


void MacroAssembler::StoreRoot(Register source, Heap::RootListIndex index) {
  movq(Operand(kRootRegister, index << kPointerSizeLog2), source);
}


void MacroAssembler::PushRoot(Heap::RootListIndex index) {
  push(Operand(kRootRegister, index << kPointerSizeLog2));
}


void MacroAssembler::CompareRoot(Register with, Heap::RootListIndex index) {
  cmpq(with, Operand(kRootRegister, index << kPointerSizeLog2));
}


void MacroAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  ASSERT(!with.AddressUsesRegister(kScratchRegister));
  LoadRoot(kScratchRegister, index);
  cmpq(with, kScratchRegister);
}


void MacroAssembler::RecordWriteHelper(Register object,
                                       Register addr,
                                       Register scratch) {
  if (FLAG_debug_code) {
    // Check that the object is not in new space.
    NearLabel not_in_new_space;
    InNewSpace(object, scratch, not_equal, &not_in_new_space);
    Abort("new-space object passed to RecordWriteHelper");
    bind(&not_in_new_space);
  }

  // Compute the page start address from the heap object pointer, and reuse
  // the 'object' register for it.
  and_(object, Immediate(~Page::kPageAlignmentMask));

  // Compute number of region covering addr. See Page::GetRegionNumberForAddress
  // method for more details.
  shrl(addr, Immediate(Page::kRegionSizeLog2));
  andl(addr, Immediate(Page::kPageAlignmentMask >> Page::kRegionSizeLog2));

  // Set dirty mark for region.
  bts(Operand(object, Page::kDirtyFlagOffset), addr);
}


void MacroAssembler::RecordWrite(Register object,
                                 int offset,
                                 Register value,
                                 Register index) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are rsi.
  ASSERT(!object.is(rsi) && !value.is(rsi) && !index.is(rsi));

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;
  JumpIfSmi(value, &done);

  RecordWriteNonSmi(object, offset, value, index);
  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors. This clobbering repeats the
  // clobbering done inside RecordWriteNonSmi but it's necessary to
  // avoid having the fast case for smis leave the registers
  // unchanged.
  if (FLAG_debug_code) {
    movq(object, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    movq(value, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    movq(index, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
  }
}


void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are rsi.
  ASSERT(!object.is(rsi) && !value.is(rsi) && !address.is(rsi));

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;
  JumpIfSmi(value, &done);

  InNewSpace(object, value, equal, &done);

  RecordWriteHelper(object, address, value);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    movq(object, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    movq(address, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    movq(value, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
  }
}


void MacroAssembler::RecordWriteNonSmi(Register object,
                                       int offset,
                                       Register scratch,
                                       Register index) {
  Label done;

  if (FLAG_debug_code) {
    NearLabel okay;
    JumpIfNotSmi(object, &okay);
    Abort("MacroAssembler::RecordWriteNonSmi cannot deal with smis");
    bind(&okay);

    if (offset == 0) {
      // index must be int32.
      Register tmp = index.is(rax) ? rbx : rax;
      push(tmp);
      movl(tmp, index);
      cmpq(tmp, index);
      Check(equal, "Index register for RecordWrite must be untagged int32.");
      pop(tmp);
    }
  }

  // Test that the object address is not in the new space. We cannot
  // update page dirty marks for new space pages.
  InNewSpace(object, scratch, equal, &done);

  // The offset is relative to a tagged or untagged HeapObject pointer,
  // so either offset or offset + kHeapObjectTag must be a
  // multiple of kPointerSize.
  ASSERT(IsAligned(offset, kPointerSize) ||
         IsAligned(offset + kHeapObjectTag, kPointerSize));

  Register dst = index;
  if (offset != 0) {
    lea(dst, Operand(object, offset));
  } else {
    // array access: calculate the destination address in the same manner as
    // KeyedStoreIC::GenerateGeneric.
    lea(dst, FieldOperand(object,
                          index,
                          times_pointer_size,
                          FixedArray::kHeaderSize));
  }
  RecordWriteHelper(object, dst, scratch);

  bind(&done);

  // Clobber all input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (FLAG_debug_code) {
    movq(object, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    movq(scratch, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
    movq(index, BitCast<int64_t>(kZapValue), RelocInfo::NONE);
  }
}

void MacroAssembler::Assert(Condition cc, const char* msg) {
  if (FLAG_debug_code) Check(cc, msg);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (FLAG_debug_code) {
    NearLabel ok;
    CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                Heap::kFixedArrayMapRootIndex);
    j(equal, &ok);
    CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                Heap::kFixedCOWArrayMapRootIndex);
    j(equal, &ok);
    Abort("JSObject with fast elements map has slow elements");
    bind(&ok);
  }
}


void MacroAssembler::Check(Condition cc, const char* msg) {
  NearLabel L;
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
    NearLabel alignment_as_expected;
    testq(rsp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}


void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op,
                                      Label* then_label) {
  NearLabel ok;
  testl(result, result);
  j(not_zero, &ok);
  testl(op, op);
  j(sign, then_label);
  bind(&ok);
}


void MacroAssembler::Abort(const char* msg) {
  // We want to pass the msg string like a smi to avoid GC
  // problems, however msg is not guaranteed to be aligned
  // properly. Instead, we pass an aligned pointer that is
  // a proper v8 smi, but also pass the alignment difference
  // from the real pointer as a smi.
  intptr_t p1 = reinterpret_cast<intptr_t>(msg);
  intptr_t p0 = (p1 & ~kSmiTagMask) + kSmiTag;
  // Note: p0 might not be a valid Smi *value*, but it has a valid Smi tag.
  ASSERT(reinterpret_cast<Object*>(p0)->IsSmi());
#ifdef DEBUG
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }
#endif
  // Disable stub call restrictions to always allow calls to abort.
  AllowStubCallsScope allow_scope(this, true);

  push(rax);
  movq(kScratchRegister, p0, RelocInfo::NONE);
  push(kScratchRegister);
  movq(kScratchRegister,
       reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(p1 - p0))),
       RelocInfo::NONE);
  push(kScratchRegister);
  CallRuntime(Runtime::kAbort, 2);
  // will not return here
  int3();
}


void MacroAssembler::CallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // calls are not allowed in some stubs
  Call(stub->GetCode(), RelocInfo::CODE_TARGET);
}


MaybeObject* MacroAssembler::TryCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  MaybeObject* result = stub->TryGetCode();
  if (!result->IsFailure()) {
    call(Handle<Code>(Code::cast(result->ToObjectUnchecked())),
         RelocInfo::CODE_TARGET);
  }
  return result;
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}


MaybeObject* MacroAssembler::TryTailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls());  // Calls are not allowed in some stubs.
  MaybeObject* result = stub->TryGetCode();
  if (!result->IsFailure()) {
    jmp(Handle<Code>(Code::cast(result->ToObjectUnchecked())),
        RelocInfo::CODE_TARGET);
  }
  return result;
}


void MacroAssembler::StubReturn(int argc) {
  ASSERT(argc >= 1 && generating_stub());
  ret((argc - 1) * kPointerSize);
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    addq(rsp, Immediate(num_arguments * kPointerSize));
  }
  LoadRoot(rax, Heap::kUndefinedValueRootIndex);
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // The assert checks that the constants for the maximum number of digits
  // for an array index cached in the hash field and the number of bits
  // reserved for it does not conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key. Even if we subsequently go to
  // the slow case, converting the key to a smi is always valid.
  // key: string key
  // hash: key's hash field, including its array index value.
  and_(hash, Immediate(String::kArrayIndexValueMask));
  shr(hash, Immediate(String::kHashShift));
  // Here we actually clobber the key which will be used if calling into
  // runtime later. However as the new key is the numeric value of a string key
  // there is no difference in using either key.
  Integer32ToSmi(index, hash);
}


void MacroAssembler::CallRuntime(Runtime::FunctionId id, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(id), num_arguments);
}


void MacroAssembler::CallRuntimeSaveDoubles(Runtime::FunctionId id) {
  Runtime::Function* function = Runtime::FunctionForId(id);
  Set(rax, function->nargs);
  movq(rbx, ExternalReference(function));
  CEntryStub ces(1);
  ces.SaveDoubles();
  CallStub(&ces);
}


MaybeObject* MacroAssembler::TryCallRuntime(Runtime::FunctionId id,
                                            int num_arguments) {
  return TryCallRuntime(Runtime::FunctionForId(id), num_arguments);
}


void MacroAssembler::CallRuntime(Runtime::Function* f, int num_arguments) {
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
  Set(rax, num_arguments);
  movq(rbx, ExternalReference(f));
  CEntryStub ces(f->result_size);
  CallStub(&ces);
}


MaybeObject* MacroAssembler::TryCallRuntime(Runtime::Function* f,
                                            int num_arguments) {
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    IllegalOperation(num_arguments);
    // Since we did not call the stub, there was no allocation failure.
    // Return some non-failure object.
    return Heap::undefined_value();
  }

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, num_arguments);
  movq(rbx, ExternalReference(f));
  CEntryStub ces(f->result_size);
  return TryCallStub(&ces);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  Set(rax, num_arguments);
  movq(rbx, ext);

  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : argument num_arguments - 1
  //  ...
  //  -- rsp[8 * num_arguments] : argument 0 (receiver)
  // -----------------------------------

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, num_arguments);
  JumpToExternalReference(ext, result_size);
}


MaybeObject* MacroAssembler::TryTailCallExternalReference(
    const ExternalReference& ext, int num_arguments, int result_size) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : argument num_arguments - 1
  //  ...
  //  -- rsp[8 * num_arguments] : argument 0 (receiver)
  // -----------------------------------

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, num_arguments);
  return TryJumpToExternalReference(ext, result_size);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid), num_arguments, result_size);
}


MaybeObject* MacroAssembler::TryTailCallRuntime(Runtime::FunctionId fid,
                                                int num_arguments,
                                                int result_size) {
  return TryTailCallExternalReference(ExternalReference(fid),
                                      num_arguments,
                                      result_size);
}


static int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  ASSERT(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}


void MacroAssembler::PrepareCallApiFunction(int arg_stack_space) {
#ifdef _WIN64
  // We need to prepare a slot for result handle on stack and put
  // a pointer to it into 1st arg register.
  EnterApiExitFrame(arg_stack_space + 1);

  // rcx must be used to pass the pointer to the return value slot.
  lea(rcx, StackSpaceOperand(arg_stack_space));
#else
  EnterApiExitFrame(arg_stack_space);
#endif
}


MaybeObject* MacroAssembler::TryCallApiFunctionAndReturn(
    ApiFunction* function, int stack_space) {
  Label empty_result;
  Label prologue;
  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label write_back;

  ExternalReference next_address =
      ExternalReference::handle_scope_next_address();
  const int kNextOffset = 0;
  const int kLimitOffset = Offset(
      ExternalReference::handle_scope_limit_address(),
      next_address);
  const int kLevelOffset = Offset(
      ExternalReference::handle_scope_level_address(),
      next_address);
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address();

  // Allocate HandleScope in callee-save registers.
  Register prev_next_address_reg = r14;
  Register prev_limit_reg = rbx;
  Register base_reg = r12;
  movq(base_reg, next_address);
  movq(prev_next_address_reg, Operand(base_reg, kNextOffset));
  movq(prev_limit_reg, Operand(base_reg, kLimitOffset));
  addl(Operand(base_reg, kLevelOffset), Immediate(1));
  // Call the api function!
  movq(rax,
       reinterpret_cast<int64_t>(function->address()),
       RelocInfo::RUNTIME_ENTRY);
  call(rax);

#ifdef _WIN64
  // rax keeps a pointer to v8::Handle, unpack it.
  movq(rax, Operand(rax, 0));
#endif
  // Check if the result handle holds 0.
  testq(rax, rax);
  j(zero, &empty_result);
  // It was non-zero.  Dereference to get the result value.
  movq(rax, Operand(rax, 0));
  bind(&prologue);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  subl(Operand(base_reg, kLevelOffset), Immediate(1));
  movq(Operand(base_reg, kNextOffset), prev_next_address_reg);
  cmpq(prev_limit_reg, Operand(base_reg, kLimitOffset));
  j(not_equal, &delete_allocated_handles);
  bind(&leave_exit_frame);

  // Check if the function scheduled an exception.
  movq(rsi, scheduled_exception_address);
  Cmp(Operand(rsi, 0), Factory::the_hole_value());
  j(not_equal, &promote_scheduled_exception);

  LeaveApiExitFrame();
  ret(stack_space * kPointerSize);

  bind(&promote_scheduled_exception);
  MaybeObject* result = TryTailCallRuntime(Runtime::kPromoteScheduledException,
                                           0, 1);
  if (result->IsFailure()) {
    return result;
  }

  bind(&empty_result);
  // It was zero; the result is undefined.
  Move(rax, Factory::undefined_value());
  jmp(&prologue);

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  movq(Operand(base_reg, kLimitOffset), prev_limit_reg);
  movq(prev_limit_reg, rax);
  movq(rax, ExternalReference::delete_handle_scope_extensions());
  call(rax);
  movq(rax, prev_limit_reg);
  jmp(&leave_exit_frame);

  return result;
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             int result_size) {
  // Set the entry point and jump to the C entry runtime stub.
  movq(rbx, ext);
  CEntryStub ces(result_size);
  jmp(ces.GetCode(), RelocInfo::CODE_TARGET);
}


MaybeObject* MacroAssembler::TryJumpToExternalReference(
    const ExternalReference& ext, int result_size) {
  // Set the entry point and jump to the C entry runtime stub.
  movq(rbx, ext);
  CEntryStub ces(result_size);
  return TryTailCallStub(&ces);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   PostCallGenerator* post_call_generator) {
  // Calls are not allowed in some stubs.
  ASSERT(flag == JUMP_FUNCTION || allow_stub_calls());

  // Rely on the assertion to check that the number of provided
  // arguments match the expected number of arguments. Fake a
  // parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  GetBuiltinEntry(rdx, id);
  InvokeCode(rdx, expected, expected, flag, post_call_generator);
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  movq(target, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  movq(target, FieldOperand(target, GlobalObject::kBuiltinsOffset));
  movq(target, FieldOperand(target,
                            JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(rdi));
  // Load the JavaScript builtin function from the builtins object.
  GetBuiltinFunction(rdi, id);
  movq(target, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::Set(Register dst, int64_t x) {
  if (x == 0) {
    xorl(dst, dst);
  } else if (is_int32(x)) {
    movq(dst, Immediate(static_cast<int32_t>(x)));
  } else if (is_uint32(x)) {
    movl(dst, Immediate(static_cast<uint32_t>(x)));
  } else {
    movq(dst, x, RelocInfo::NONE);
  }
}

void MacroAssembler::Set(const Operand& dst, int64_t x) {
  if (is_int32(x)) {
    movq(dst, Immediate(static_cast<int32_t>(x)));
  } else {
    movq(kScratchRegister, x, RelocInfo::NONE);
    movq(dst, kScratchRegister);
  }
}

// ----------------------------------------------------------------------------
// Smi tagging, untagging and tag detection.

Register MacroAssembler::GetSmiConstant(Smi* source) {
  int value = source->value();
  if (value == 0) {
    xorl(kScratchRegister, kScratchRegister);
    return kScratchRegister;
  }
  if (value == 1) {
    return kSmiConstantRegister;
  }
  LoadSmiConstant(kScratchRegister, source);
  return kScratchRegister;
}

void MacroAssembler::LoadSmiConstant(Register dst, Smi* source) {
  if (FLAG_debug_code) {
    movq(dst,
         reinterpret_cast<uint64_t>(Smi::FromInt(kSmiConstantRegisterValue)),
         RelocInfo::NONE);
    cmpq(dst, kSmiConstantRegister);
    if (allow_stub_calls()) {
      Assert(equal, "Uninitialized kSmiConstantRegister");
    } else {
      NearLabel ok;
      j(equal, &ok);
      int3();
      bind(&ok);
    }
  }
  if (source->value() == 0) {
    xorl(dst, dst);
    return;
  }
  int value = source->value();
  bool negative = value < 0;
  unsigned int uvalue = negative ? -value : value;

  switch (uvalue) {
    case 9:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_8, 0));
      break;
    case 8:
      xorl(dst, dst);
      lea(dst, Operand(dst, kSmiConstantRegister, times_8, 0));
      break;
    case 4:
      xorl(dst, dst);
      lea(dst, Operand(dst, kSmiConstantRegister, times_4, 0));
      break;
    case 5:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_4, 0));
      break;
    case 3:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_2, 0));
      break;
    case 2:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_1, 0));
      break;
    case 1:
      movq(dst, kSmiConstantRegister);
      break;
    case 0:
      UNREACHABLE();
      return;
    default:
      movq(dst, reinterpret_cast<uint64_t>(source), RelocInfo::NONE);
      return;
  }
  if (negative) {
    neg(dst);
  }
}


void MacroAssembler::Integer32ToSmi(Register dst, Register src) {
  ASSERT_EQ(0, kSmiTag);
  if (!dst.is(src)) {
    movl(dst, src);
  }
  shl(dst, Immediate(kSmiShift));
}


void MacroAssembler::Integer32ToSmiField(const Operand& dst, Register src) {
  if (FLAG_debug_code) {
    testb(dst, Immediate(0x01));
    NearLabel ok;
    j(zero, &ok);
    if (allow_stub_calls()) {
      Abort("Integer32ToSmiField writing to non-smi location");
    } else {
      int3();
    }
    bind(&ok);
  }
  ASSERT(kSmiShift % kBitsPerByte == 0);
  movl(Operand(dst, kSmiShift / kBitsPerByte), src);
}


void MacroAssembler::Integer64PlusConstantToSmi(Register dst,
                                                Register src,
                                                int constant) {
  if (dst.is(src)) {
    addq(dst, Immediate(constant));
  } else {
    lea(dst, Operand(src, constant));
  }
  shl(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger32(Register dst, Register src) {
  ASSERT_EQ(0, kSmiTag);
  if (!dst.is(src)) {
    movq(dst, src);
  }
  shr(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger32(Register dst, const Operand& src) {
  movl(dst, Operand(src, kSmiShift / kBitsPerByte));
}


void MacroAssembler::SmiToInteger64(Register dst, Register src) {
  ASSERT_EQ(0, kSmiTag);
  if (!dst.is(src)) {
    movq(dst, src);
  }
  sar(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger64(Register dst, const Operand& src) {
  movsxlq(dst, Operand(src, kSmiShift / kBitsPerByte));
}


void MacroAssembler::SmiTest(Register src) {
  testq(src, src);
}


void MacroAssembler::SmiCompare(Register dst, Register src) {
  cmpq(dst, src);
}


void MacroAssembler::SmiCompare(Register dst, Smi* src) {
  ASSERT(!dst.is(kScratchRegister));
  if (src->value() == 0) {
    testq(dst, dst);
  } else {
    Register constant_reg = GetSmiConstant(src);
    cmpq(dst, constant_reg);
  }
}


void MacroAssembler::SmiCompare(Register dst, const Operand& src) {
  cmpq(dst, src);
}


void MacroAssembler::SmiCompare(const Operand& dst, Register src) {
  cmpq(dst, src);
}


void MacroAssembler::SmiCompare(const Operand& dst, Smi* src) {
  cmpl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(src->value()));
}


void MacroAssembler::SmiCompareInteger32(const Operand& dst, Register src) {
  cmpl(Operand(dst, kSmiShift / kBitsPerByte), src);
}


void MacroAssembler::PositiveSmiTimesPowerOfTwoToInteger64(Register dst,
                                                           Register src,
                                                           int power) {
  ASSERT(power >= 0);
  ASSERT(power < 64);
  if (power == 0) {
    SmiToInteger64(dst, src);
    return;
  }
  if (!dst.is(src)) {
    movq(dst, src);
  }
  if (power < kSmiShift) {
    sar(dst, Immediate(kSmiShift - power));
  } else if (power > kSmiShift) {
    shl(dst, Immediate(power - kSmiShift));
  }
}


void MacroAssembler::PositiveSmiDivPowerOfTwoToInteger32(Register dst,
                                                         Register src,
                                                         int power) {
  ASSERT((0 <= power) && (power < 32));
  if (dst.is(src)) {
    shr(dst, Immediate(power + kSmiShift));
  } else {
    UNIMPLEMENTED();  // Not used.
  }
}


Condition MacroAssembler::CheckSmi(Register src) {
  ASSERT_EQ(0, kSmiTag);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}


Condition MacroAssembler::CheckSmi(const Operand& src) {
  ASSERT_EQ(0, kSmiTag);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}


Condition MacroAssembler::CheckNonNegativeSmi(Register src) {
  ASSERT_EQ(0, kSmiTag);
  // Make mask 0x8000000000000001 and test that both bits are zero.
  movq(kScratchRegister, src);
  rol(kScratchRegister, Immediate(1));
  testb(kScratchRegister, Immediate(3));
  return zero;
}


Condition MacroAssembler::CheckBothSmi(Register first, Register second) {
  if (first.is(second)) {
    return CheckSmi(first);
  }
  ASSERT(kSmiTag == 0 && kHeapObjectTag == 1 && kHeapObjectTagMask == 3);
  leal(kScratchRegister, Operand(first, second, times_1, 0));
  testb(kScratchRegister, Immediate(0x03));
  return zero;
}


Condition MacroAssembler::CheckBothNonNegativeSmi(Register first,
                                                  Register second) {
  if (first.is(second)) {
    return CheckNonNegativeSmi(first);
  }
  movq(kScratchRegister, first);
  or_(kScratchRegister, second);
  rol(kScratchRegister, Immediate(1));
  testl(kScratchRegister, Immediate(3));
  return zero;
}


Condition MacroAssembler::CheckEitherSmi(Register first,
                                         Register second,
                                         Register scratch) {
  if (first.is(second)) {
    return CheckSmi(first);
  }
  if (scratch.is(second)) {
    andl(scratch, first);
  } else {
    if (!scratch.is(first)) {
      movl(scratch, first);
    }
    andl(scratch, second);
  }
  testb(scratch, Immediate(kSmiTagMask));
  return zero;
}


Condition MacroAssembler::CheckIsMinSmi(Register src) {
  ASSERT(!src.is(kScratchRegister));
  // If we overflow by subtracting one, it's the minimal smi value.
  cmpq(src, kSmiConstantRegister);
  return overflow;
}


Condition MacroAssembler::CheckInteger32ValidSmiValue(Register src) {
  // A 32-bit integer value can always be converted to a smi.
  return always;
}


Condition MacroAssembler::CheckUInteger32ValidSmiValue(Register src) {
  // An unsigned 32-bit integer value is valid as long as the high bit
  // is not set.
  testl(src, src);
  return positive;
}


void MacroAssembler::CheckSmiToIndicator(Register dst, Register src) {
  if (dst.is(src)) {
    andl(dst, Immediate(kSmiTagMask));
  } else {
    movl(dst, Immediate(kSmiTagMask));
    andl(dst, src);
  }
}


void MacroAssembler::CheckSmiToIndicator(Register dst, const Operand& src) {
  if (!(src.AddressUsesRegister(dst))) {
    movl(dst, Immediate(kSmiTagMask));
    andl(dst, src);
  } else {
    movl(dst, src);
    andl(dst, Immediate(kSmiTagMask));
  }
}


void MacroAssembler::SmiAddConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
    return;
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    switch (constant->value()) {
      case 1:
        addq(dst, kSmiConstantRegister);
        return;
      case 2:
        lea(dst, Operand(src, kSmiConstantRegister, times_2, 0));
        return;
      case 4:
        lea(dst, Operand(src, kSmiConstantRegister, times_4, 0));
        return;
      case 8:
        lea(dst, Operand(src, kSmiConstantRegister, times_8, 0));
        return;
      default:
        Register constant_reg = GetSmiConstant(constant);
        addq(dst, constant_reg);
        return;
    }
  } else {
    switch (constant->value()) {
      case 1:
        lea(dst, Operand(src, kSmiConstantRegister, times_1, 0));
        return;
      case 2:
        lea(dst, Operand(src, kSmiConstantRegister, times_2, 0));
        return;
      case 4:
        lea(dst, Operand(src, kSmiConstantRegister, times_4, 0));
        return;
      case 8:
        lea(dst, Operand(src, kSmiConstantRegister, times_8, 0));
        return;
      default:
        LoadSmiConstant(dst, constant);
        addq(dst, src);
        return;
    }
  }
}


void MacroAssembler::SmiAddConstant(const Operand& dst, Smi* constant) {
  if (constant->value() != 0) {
    addl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(constant->value()));
  }
}


void MacroAssembler::SmiSubConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    subq(dst, constant_reg);
  } else {
    if (constant->value() == Smi::kMinValue) {
      LoadSmiConstant(dst, constant);
      // Adding and subtracting the min-value gives the same result, it only
      // differs on the overflow bit, which we don't check here.
      addq(dst, src);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(dst, Smi::FromInt(-constant->value()));
      addq(dst, src);
    }
  }
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            Register src2) {
  // No overflow checking. Use only when it's known that
  // overflowing is impossible.
  ASSERT(!dst.is(src2));
  if (dst.is(src1)) {
    addq(dst, src2);
  } else {
    movq(dst, src1);
    addq(dst, src2);
  }
  Assert(no_overflow, "Smi addition overflow");
}


void MacroAssembler::SmiSub(Register dst, Register src1, Register src2) {
  // No overflow checking. Use only when it's known that
  // overflowing is impossible (e.g., subtracting two positive smis).
  ASSERT(!dst.is(src2));
  if (dst.is(src1)) {
    subq(dst, src2);
  } else {
    movq(dst, src1);
    subq(dst, src2);
  }
  Assert(no_overflow, "Smi subtraction overflow");
}


void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            const Operand& src2) {
  // No overflow checking. Use only when it's known that
  // overflowing is impossible (e.g., subtracting two positive smis).
  if (dst.is(src1)) {
    subq(dst, src2);
  } else {
    movq(dst, src1);
    subq(dst, src2);
  }
  Assert(no_overflow, "Smi subtraction overflow");
}


void MacroAssembler::SmiNot(Register dst, Register src) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src.is(kScratchRegister));
  // Set tag and padding bits before negating, so that they are zero afterwards.
  movl(kScratchRegister, Immediate(~0));
  if (dst.is(src)) {
    xor_(dst, kScratchRegister);
  } else {
    lea(dst, Operand(src, kScratchRegister, times_1, 0));
  }
  not_(dst);
}


void MacroAssembler::SmiAnd(Register dst, Register src1, Register src2) {
  ASSERT(!dst.is(src2));
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  and_(dst, src2);
}


void MacroAssembler::SmiAndConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    Set(dst, 0);
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    and_(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    and_(dst, src);
  }
}


void MacroAssembler::SmiOr(Register dst, Register src1, Register src2) {
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  or_(dst, src2);
}


void MacroAssembler::SmiOrConstant(Register dst, Register src, Smi* constant) {
  if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    or_(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    or_(dst, src);
  }
}


void MacroAssembler::SmiXor(Register dst, Register src1, Register src2) {
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  xor_(dst, src2);
}


void MacroAssembler::SmiXorConstant(Register dst, Register src, Smi* constant) {
  if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    xor_(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    xor_(dst, src);
  }
}


void MacroAssembler::SmiShiftArithmeticRightConstant(Register dst,
                                                     Register src,
                                                     int shift_value) {
  ASSERT(is_uint5(shift_value));
  if (shift_value > 0) {
    if (dst.is(src)) {
      sar(dst, Immediate(shift_value + kSmiShift));
      shl(dst, Immediate(kSmiShift));
    } else {
      UNIMPLEMENTED();  // Not used.
    }
  }
}


void MacroAssembler::SmiShiftLeftConstant(Register dst,
                                          Register src,
                                          int shift_value) {
  if (!dst.is(src)) {
    movq(dst, src);
  }
  if (shift_value > 0) {
    shl(dst, Immediate(shift_value));
  }
}


void MacroAssembler::SmiShiftLeft(Register dst,
                                  Register src1,
                                  Register src2) {
  ASSERT(!dst.is(rcx));
  NearLabel result_ok;
  // Untag shift amount.
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  SmiToInteger32(rcx, src2);
  // Shift amount specified by lower 5 bits, not six as the shl opcode.
  and_(rcx, Immediate(0x1f));
  shl_cl(dst);
}


void MacroAssembler::SmiShiftArithmeticRight(Register dst,
                                             Register src1,
                                             Register src2) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(rcx));
  if (src1.is(rcx)) {
    movq(kScratchRegister, src1);
  } else if (src2.is(rcx)) {
    movq(kScratchRegister, src2);
  }
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  SmiToInteger32(rcx, src2);
  orl(rcx, Immediate(kSmiShift));
  sar_cl(dst);  // Shift 32 + original rcx & 0x1f.
  shl(dst, Immediate(kSmiShift));
  if (src1.is(rcx)) {
    movq(src1, kScratchRegister);
  } else if (src2.is(rcx)) {
    movq(src2, kScratchRegister);
  }
}


SmiIndex MacroAssembler::SmiToIndex(Register dst,
                                    Register src,
                                    int shift) {
  ASSERT(is_uint6(shift));
  // There is a possible optimization if shift is in the range 60-63, but that
  // will (and must) never happen.
  if (!dst.is(src)) {
    movq(dst, src);
  }
  if (shift < kSmiShift) {
    sar(dst, Immediate(kSmiShift - shift));
  } else {
    shl(dst, Immediate(shift - kSmiShift));
  }
  return SmiIndex(dst, times_1);
}

SmiIndex MacroAssembler::SmiToNegativeIndex(Register dst,
                                            Register src,
                                            int shift) {
  // Register src holds a positive smi.
  ASSERT(is_uint6(shift));
  if (!dst.is(src)) {
    movq(dst, src);
  }
  neg(dst);
  if (shift < kSmiShift) {
    sar(dst, Immediate(kSmiShift - shift));
  } else {
    shl(dst, Immediate(shift - kSmiShift));
  }
  return SmiIndex(dst, times_1);
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    movq(dst, src);
  }
}


void MacroAssembler::Move(Register dst, Handle<Object> source) {
  ASSERT(!source->IsFailure());
  if (source->IsSmi()) {
    Move(dst, Smi::cast(*source));
  } else {
    movq(dst, source, RelocInfo::EMBEDDED_OBJECT);
  }
}


void MacroAssembler::Move(const Operand& dst, Handle<Object> source) {
  ASSERT(!source->IsFailure());
  if (source->IsSmi()) {
    Move(dst, Smi::cast(*source));
  } else {
    movq(kScratchRegister, source, RelocInfo::EMBEDDED_OBJECT);
    movq(dst, kScratchRegister);
  }
}


void MacroAssembler::Cmp(Register dst, Handle<Object> source) {
  if (source->IsSmi()) {
    SmiCompare(dst, Smi::cast(*source));
  } else {
    Move(kScratchRegister, source);
    cmpq(dst, kScratchRegister);
  }
}


void MacroAssembler::Cmp(const Operand& dst, Handle<Object> source) {
  if (source->IsSmi()) {
    SmiCompare(dst, Smi::cast(*source));
  } else {
    ASSERT(source->IsHeapObject());
    movq(kScratchRegister, source, RelocInfo::EMBEDDED_OBJECT);
    cmpq(dst, kScratchRegister);
  }
}


void MacroAssembler::Push(Handle<Object> source) {
  if (source->IsSmi()) {
    Push(Smi::cast(*source));
  } else {
    ASSERT(source->IsHeapObject());
    movq(kScratchRegister, source, RelocInfo::EMBEDDED_OBJECT);
    push(kScratchRegister);
  }
}


void MacroAssembler::Push(Smi* source) {
  intptr_t smi = reinterpret_cast<intptr_t>(source);
  if (is_int32(smi)) {
    push(Immediate(static_cast<int32_t>(smi)));
  } else {
    Register constant = GetSmiConstant(source);
    push(constant);
  }
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    addq(rsp, Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::Test(const Operand& src, Smi* source) {
  testl(Operand(src, kIntSize), Immediate(source->value()));
}


void MacroAssembler::Jump(ExternalReference ext) {
  movq(kScratchRegister, ext);
  jmp(kScratchRegister);
}


void MacroAssembler::Jump(Address destination, RelocInfo::Mode rmode) {
  movq(kScratchRegister, destination, rmode);
  jmp(kScratchRegister);
}


void MacroAssembler::Jump(Handle<Code> code_object, RelocInfo::Mode rmode) {
  // TODO(X64): Inline this
  jmp(code_object, rmode);
}


void MacroAssembler::Call(ExternalReference ext) {
  movq(kScratchRegister, ext);
  call(kScratchRegister);
}


void MacroAssembler::Call(Address destination, RelocInfo::Mode rmode) {
  movq(kScratchRegister, destination, rmode);
  call(kScratchRegister);
}


void MacroAssembler::Call(Handle<Code> code_object, RelocInfo::Mode rmode) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  call(code_object, rmode);
}


void MacroAssembler::Pushad() {
  push(rax);
  push(rcx);
  push(rdx);
  push(rbx);
  // Not pushing rsp or rbp.
  push(rsi);
  push(rdi);
  push(r8);
  push(r9);
  // r10 is kScratchRegister.
  push(r11);
  push(r12);
  // r13 is kRootRegister.
  push(r14);
  // r15 is kSmiConstantRegister
  STATIC_ASSERT(11 == kNumSafepointSavedRegisters);
  // Use lea for symmetry with Popad.
  int sp_delta =
      (kNumSafepointRegisters - kNumSafepointSavedRegisters) * kPointerSize;
  lea(rsp, Operand(rsp, -sp_delta));
}


void MacroAssembler::Popad() {
  // Popad must not change the flags, so use lea instead of addq.
  int sp_delta =
      (kNumSafepointRegisters - kNumSafepointSavedRegisters) * kPointerSize;
  lea(rsp, Operand(rsp, sp_delta));
  pop(r14);
  pop(r12);
  pop(r11);
  pop(r9);
  pop(r8);
  pop(rdi);
  pop(rsi);
  pop(rbx);
  pop(rdx);
  pop(rcx);
  pop(rax);
}


void MacroAssembler::Dropad() {
  addq(rsp, Immediate(kNumSafepointRegisters * kPointerSize));
}


// Order general registers are pushed by Pushad:
// rax, rcx, rdx, rbx, rsi, rdi, r8, r9, r11, r12, r14.
int MacroAssembler::kSafepointPushRegisterIndices[Register::kNumRegisters] = {
    0,
    1,
    2,
    3,
    -1,
    -1,
    4,
    5,
    6,
    7,
    -1,
    8,
    9,
    -1,
    10,
    -1
};


void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Register src) {
  movq(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  movq(dst, SafepointRegisterSlot(src));
}


Operand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return Operand(rsp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  // Adjust this code if not the case.
  ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize);

  // The pc (return address) is already on TOS.  This code pushes state,
  // frame pointer and current handler.  Check that they are expected
  // next on the stack, in that order.
  ASSERT_EQ(StackHandlerConstants::kStateOffset,
            StackHandlerConstants::kPCOffset - kPointerSize);
  ASSERT_EQ(StackHandlerConstants::kFPOffset,
            StackHandlerConstants::kStateOffset - kPointerSize);
  ASSERT_EQ(StackHandlerConstants::kNextOffset,
            StackHandlerConstants::kFPOffset - kPointerSize);

  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      push(Immediate(StackHandler::TRY_CATCH));
    } else {
      push(Immediate(StackHandler::TRY_FINALLY));
    }
    push(rbp);
  } else {
    ASSERT(try_location == IN_JS_ENTRY);
    // The frame pointer does not point to a JS frame so we save NULL
    // for rbp. We expect the code throwing an exception to check rbp
    // before dereferencing it to restore the context.
    push(Immediate(StackHandler::ENTRY));
    push(Immediate(0));  // NULL frame pointer.
  }
  // Save the current handler.
  movq(kScratchRegister, ExternalReference(Top::k_handler_address));
  push(Operand(kScratchRegister, 0));
  // Link this handler.
  movq(Operand(kScratchRegister, 0), rsp);
}


void MacroAssembler::PopTryHandler() {
  ASSERT_EQ(0, StackHandlerConstants::kNextOffset);
  // Unlink this handler.
  movq(kScratchRegister, ExternalReference(Top::k_handler_address));
  pop(Operand(kScratchRegister, 0));
  // Remove the remaining fields.
  addq(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


void MacroAssembler::Throw(Register value) {
  // Check that stack should contain next handler, frame pointer, state and
  // return address in that order.
  STATIC_ASSERT(StackHandlerConstants::kFPOffset + kPointerSize ==
            StackHandlerConstants::kStateOffset);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset + kPointerSize ==
            StackHandlerConstants::kPCOffset);
  // Keep thrown value in rax.
  if (!value.is(rax)) {
    movq(rax, value);
  }

  ExternalReference handler_address(Top::k_handler_address);
  movq(kScratchRegister, handler_address);
  movq(rsp, Operand(kScratchRegister, 0));
  // get next in chain
  pop(rcx);
  movq(Operand(kScratchRegister, 0), rcx);
  pop(rbp);  // pop frame pointer
  pop(rdx);  // remove state

  // Before returning we restore the context from the frame pointer if not NULL.
  // The frame pointer is NULL in the exception handler of a JS entry frame.
  Set(rsi, 0);  // Tentatively set context pointer to NULL
  NearLabel skip;
  cmpq(rbp, Immediate(0));
  j(equal, &skip);
  movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  bind(&skip);
  ret(0);
}


void MacroAssembler::ThrowUncatchable(UncatchableExceptionType type,
                                      Register value) {
  // Keep thrown value in rax.
  if (!value.is(rax)) {
    movq(rax, value);
  }
  // Fetch top stack handler.
  ExternalReference handler_address(Top::k_handler_address);
  movq(kScratchRegister, handler_address);
  movq(rsp, Operand(kScratchRegister, 0));

  // Unwind the handlers until the ENTRY handler is found.
  NearLabel loop, done;
  bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kStateOffset;
  cmpq(Operand(rsp, kStateOffset), Immediate(StackHandler::ENTRY));
  j(equal, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kNextOffset;
  movq(rsp, Operand(rsp, kNextOffset));
  jmp(&loop);
  bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  movq(kScratchRegister, handler_address);
  pop(Operand(kScratchRegister, 0));

  if (type == OUT_OF_MEMORY) {
    // Set external caught exception to false.
    ExternalReference external_caught(Top::k_external_caught_exception_address);
    movq(rax, Immediate(false));
    store_rax(external_caught);

    // Set pending exception and rax to out of memory exception.
    ExternalReference pending_exception(Top::k_pending_exception_address);
    movq(rax, Failure::OutOfMemoryException(), RelocInfo::NONE);
    store_rax(pending_exception);
  }

  // Clear the context pointer.
  Set(rsi, 0);

  // Restore registers from handler.
  STATIC_ASSERT(StackHandlerConstants::kNextOffset + kPointerSize ==
            StackHandlerConstants::kFPOffset);
  pop(rbp);  // FP
  STATIC_ASSERT(StackHandlerConstants::kFPOffset + kPointerSize ==
            StackHandlerConstants::kStateOffset);
  pop(rdx);  // State

  STATIC_ASSERT(StackHandlerConstants::kStateOffset + kPointerSize ==
            StackHandlerConstants::kPCOffset);
  ret(0);
}


void MacroAssembler::Ret() {
  ret(0);
}


void MacroAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    pop(scratch);
    addq(rsp, Immediate(bytes_dropped));
    push(scratch);
    ret(0);
  }
}


void MacroAssembler::FCmp() {
  fucomip();
  fstp(0);
}


void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  movq(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpb(FieldOperand(map, Map::kInstanceTypeOffset),
       Immediate(static_cast<int8_t>(type)));
}


void MacroAssembler::CheckMap(Register obj,
                              Handle<Map> map,
                              Label* fail,
                              bool is_heap_object) {
  if (!is_heap_object) {
    JumpIfSmi(obj, fail);
  }
  Cmp(FieldOperand(obj, HeapObject::kMapOffset), map);
  j(not_equal, fail);
}


void MacroAssembler::AbortIfNotNumber(Register object) {
  NearLabel ok;
  Condition is_smi = CheckSmi(object);
  j(is_smi, &ok);
  Cmp(FieldOperand(object, HeapObject::kMapOffset),
      Factory::heap_number_map());
  Assert(equal, "Operand not a number");
  bind(&ok);
}


void MacroAssembler::AbortIfSmi(Register object) {
  NearLabel ok;
  Condition is_smi = CheckSmi(object);
  Assert(NegateCondition(is_smi), "Operand is a smi");
}


void MacroAssembler::AbortIfNotSmi(Register object) {
  NearLabel ok;
  Condition is_smi = CheckSmi(object);
  Assert(is_smi, "Operand is not a smi");
}


void MacroAssembler::AbortIfNotString(Register object) {
  testb(object, Immediate(kSmiTagMask));
  Assert(not_equal, "Operand is not a string");
  push(object);
  movq(object, FieldOperand(object, HeapObject::kMapOffset));
  CmpInstanceType(object, FIRST_NONSTRING_TYPE);
  pop(object);
  Assert(below, "Operand is not a string");
}


void MacroAssembler::AbortIfNotRootValue(Register src,
                                         Heap::RootListIndex root_value_index,
                                         const char* message) {
  ASSERT(!src.is(kScratchRegister));
  LoadRoot(kScratchRegister, root_value_index);
  cmpq(src, kScratchRegister);
  Check(equal, message);
}



Condition MacroAssembler::IsObjectStringType(Register heap_object,
                                             Register map,
                                             Register instance_type) {
  movq(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzxbl(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  ASSERT(kNotStringTag != 0);
  testb(instance_type, Immediate(kIsNotStringMask));
  return zero;
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Label* miss) {
  // Check that the receiver isn't a smi.
  testl(function, Immediate(kSmiTagMask));
  j(zero, miss);

  // Check that the function really is a function.
  CmpObjectType(function, JS_FUNCTION_TYPE, result);
  j(not_equal, miss);

  // Make sure that the function has an instance prototype.
  NearLabel non_instance;
  testb(FieldOperand(result, Map::kBitFieldOffset),
        Immediate(1 << Map::kHasNonInstancePrototype));
  j(not_zero, &non_instance);

  // Get the prototype or initial map from the function.
  movq(result,
       FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  CompareRoot(result, Heap::kTheHoleValueRootIndex);
  j(equal, miss);

  // If the function does not have an initial map, we're done.
  NearLabel done;
  CmpObjectType(result, MAP_TYPE, kScratchRegister);
  j(not_equal, &done);

  // Get the prototype from the initial map.
  movq(result, FieldOperand(result, Map::kPrototypeOffset));
  jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  movq(result, FieldOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    movq(kScratchRegister, ExternalReference(counter));
    movl(Operand(kScratchRegister, 0), Immediate(value));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    movq(kScratchRegister, ExternalReference(counter));
    Operand operand(kScratchRegister, 0);
    if (value == 1) {
      incl(operand);
    } else {
      addl(operand, Immediate(value));
    }
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    movq(kScratchRegister, ExternalReference(counter));
    Operand operand(kScratchRegister, 0);
    if (value == 1) {
      decl(operand);
    } else {
      subl(operand, Immediate(value));
    }
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  ASSERT(allow_stub_calls());
  Set(rax, 0);  // No arguments.
  movq(rbx, ExternalReference(Runtime::kDebugBreak));
  CEntryStub ces(1);
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}
#endif  // ENABLE_DEBUGGER_SUPPORT


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                PostCallGenerator* post_call_generator) {
  NearLabel done;
  InvokePrologue(expected,
                 actual,
                 Handle<Code>::null(),
                 code,
                 &done,
                 flag,
                 post_call_generator);
  if (flag == CALL_FUNCTION) {
    call(code);
    if (post_call_generator != NULL) post_call_generator->Generate();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    jmp(code);
  }
  bind(&done);
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag,
                                PostCallGenerator* post_call_generator) {
  NearLabel done;
  Register dummy = rax;
  InvokePrologue(expected,
                 actual,
                 code,
                 dummy,
                 &done,
                 flag,
                 post_call_generator);
  if (flag == CALL_FUNCTION) {
    Call(code, rmode);
    if (post_call_generator != NULL) post_call_generator->Generate();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    Jump(code, rmode);
  }
  bind(&done);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    PostCallGenerator* post_call_generator) {
  ASSERT(function.is(rdi));
  movq(rdx, FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
  movq(rsi, FieldOperand(function, JSFunction::kContextOffset));
  movsxlq(rbx,
          FieldOperand(rdx, SharedFunctionInfo::kFormalParameterCountOffset));
  // Advances rdx to the end of the Code object header, to the start of
  // the executable code.
  movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));

  ParameterCount expected(rbx);
  InvokeCode(rdx, expected, actual, flag, post_call_generator);
}


void MacroAssembler::InvokeFunction(JSFunction* function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    PostCallGenerator* post_call_generator) {
  ASSERT(function->is_compiled());
  // Get the function and setup the context.
  Move(rdi, Handle<JSFunction>(function));
  movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  if (V8::UseCrankshaft()) {
    // Since Crankshaft can recompile a function, we need to load
    // the Code object every time we call the function.
    movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
    ParameterCount expected(function->shared()->formal_parameter_count());
    InvokeCode(rdx, expected, actual, flag, post_call_generator);
  } else {
    // Invoke the cached code.
    Handle<Code> code(function->code());
    ParameterCount expected(function->shared()->formal_parameter_count());
    InvokeCode(code,
               expected,
               actual,
               RelocInfo::CODE_TARGET,
               flag,
               post_call_generator);
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  push(rbp);
  movq(rbp, rsp);
  push(rsi);  // Context.
  Push(Smi::FromInt(type));
  movq(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  push(kScratchRegister);
  if (FLAG_debug_code) {
    movq(kScratchRegister,
         Factory::undefined_value(),
         RelocInfo::EMBEDDED_OBJECT);
    cmpq(Operand(rsp, 0), kScratchRegister);
    Check(not_equal, "code object not properly patched");
  }
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  if (FLAG_debug_code) {
    Move(kScratchRegister, Smi::FromInt(type));
    cmpq(Operand(rbp, StandardFrameConstants::kMarkerOffset), kScratchRegister);
    Check(equal, "stack frame types must match");
  }
  movq(rsp, rbp);
  pop(rbp);
}


void MacroAssembler::EnterExitFramePrologue(bool save_rax) {
  // Setup the frame structure on the stack.
  // All constants are relative to the frame pointer of the exit frame.
  ASSERT(ExitFrameConstants::kCallerSPDisplacement == +2 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerPCOffset == +1 * kPointerSize);
  ASSERT(ExitFrameConstants::kCallerFPOffset ==  0 * kPointerSize);
  push(rbp);
  movq(rbp, rsp);

  // Reserve room for entry stack pointer and push the code object.
  ASSERT(ExitFrameConstants::kSPOffset == -1 * kPointerSize);
  push(Immediate(0));  // Saved entry sp, patched before call.
  movq(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  push(kScratchRegister);  // Accessed from EditFrame::code_slot.

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address(Top::k_c_entry_fp_address);
  ExternalReference context_address(Top::k_context_address);
  if (save_rax) {
    movq(r14, rax);  // Backup rax before we use it.
  }

  movq(rax, rbp);
  store_rax(c_entry_fp_address);
  movq(rax, rsi);
  store_rax(context_address);
}


void MacroAssembler::EnterExitFrameEpilogue(int arg_stack_space,
                                            bool save_doubles) {
#ifdef _WIN64
  const int kShadowSpace = 4;
  arg_stack_space += kShadowSpace;
#endif
  // Optionally save all XMM registers.
  if (save_doubles) {
    CpuFeatures::Scope scope(SSE2);
    int space = XMMRegister::kNumRegisters * kDoubleSize +
        arg_stack_space * kPointerSize;
    subq(rsp, Immediate(space));
    int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; i++) {
      XMMRegister reg = XMMRegister::FromAllocationIndex(i);
      movsd(Operand(rbp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else if (arg_stack_space > 0) {
    subq(rsp, Immediate(arg_stack_space * kPointerSize));
  }

  // Get the required frame alignment for the OS.
  static const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    movq(kScratchRegister, Immediate(-kFrameAlignment));
    and_(rsp, kScratchRegister);
  }

  // Patch the saved entry sp.
  movq(Operand(rbp, ExitFrameConstants::kSPOffset), rsp);
}


void MacroAssembler::EnterExitFrame(int arg_stack_space, bool save_doubles) {
  EnterExitFramePrologue(true);

  // Setup argv in callee-saved register r12. It is reused in LeaveExitFrame,
  // so it must be retained across the C-call.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  lea(r12, Operand(rbp, r14, times_pointer_size, offset));

  EnterExitFrameEpilogue(arg_stack_space, save_doubles);
}


void MacroAssembler::EnterApiExitFrame(int arg_stack_space) {
  EnterExitFramePrologue(false);
  EnterExitFrameEpilogue(arg_stack_space, false);
}


void MacroAssembler::LeaveExitFrame(bool save_doubles) {
  // Registers:
  // r12 : argv
  if (save_doubles) {
    int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; i++) {
      XMMRegister reg = XMMRegister::FromAllocationIndex(i);
      movsd(reg, Operand(rbp, offset - ((i + 1) * kDoubleSize)));
    }
  }
  // Get the return address from the stack and restore the frame pointer.
  movq(rcx, Operand(rbp, 1 * kPointerSize));
  movq(rbp, Operand(rbp, 0 * kPointerSize));

  // Drop everything up to and including the arguments and the receiver
  // from the caller stack.
  lea(rsp, Operand(r12, 1 * kPointerSize));

  // Push the return address to get ready to return.
  push(rcx);

  LeaveExitFrameEpilogue();
}


void MacroAssembler::LeaveApiExitFrame() {
  movq(rsp, rbp);
  pop(rbp);

  LeaveExitFrameEpilogue();
}


void MacroAssembler::LeaveExitFrameEpilogue() {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(Top::k_context_address);
  movq(kScratchRegister, context_address);
  movq(rsi, Operand(kScratchRegister, 0));
#ifdef DEBUG
  movq(Operand(kScratchRegister, 0), Immediate(0));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(Top::k_c_entry_fp_address);
  movq(kScratchRegister, c_entry_fp_address);
  movq(Operand(kScratchRegister, 0), Immediate(0));
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  ASSERT(!holder_reg.is(scratch));
  ASSERT(!scratch.is(kScratchRegister));
  // Load current lexical context from the stack frame.
  movq(scratch, Operand(rbp, StandardFrameConstants::kContextOffset));

  // When generating debug code, make sure the lexical context is set.
  if (FLAG_debug_code) {
    cmpq(scratch, Immediate(0));
    Check(not_equal, "we should not have an empty lexical context");
  }
  // Load the global context of the current context.
  int offset = Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  movq(scratch, FieldOperand(scratch, offset));
  movq(scratch, FieldOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check the context is a global context.
  if (FLAG_debug_code) {
    Cmp(FieldOperand(scratch, HeapObject::kMapOffset),
        Factory::global_context_map());
    Check(equal, "JSGlobalObject::global_context should be a global context.");
  }

  // Check if both contexts are the same.
  cmpq(scratch, FieldOperand(holder_reg, JSGlobalProxy::kContextOffset));
  j(equal, &same_contexts);

  // Compare security tokens.
  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.

  // Check the context is a global context.
  if (FLAG_debug_code) {
    // Preserve original value of holder_reg.
    push(holder_reg);
    movq(holder_reg, FieldOperand(holder_reg, JSGlobalProxy::kContextOffset));
    CompareRoot(holder_reg, Heap::kNullValueRootIndex);
    Check(not_equal, "JSGlobalProxy::context() should not be null.");

    // Read the first word and compare to global_context_map(),
    movq(holder_reg, FieldOperand(holder_reg, HeapObject::kMapOffset));
    CompareRoot(holder_reg, Heap::kGlobalContextMapRootIndex);
    Check(equal, "JSGlobalObject::global_context should be a global context.");
    pop(holder_reg);
  }

  movq(kScratchRegister,
       FieldOperand(holder_reg, JSGlobalProxy::kContextOffset));
  int token_offset =
      Context::kHeaderSize + Context::SECURITY_TOKEN_INDEX * kPointerSize;
  movq(scratch, FieldOperand(scratch, token_offset));
  cmpq(scratch, FieldOperand(kScratchRegister, token_offset));
  j(not_equal, miss);

  bind(&same_contexts);
}


void MacroAssembler::LoadAllocationTopHelper(Register result,
                                             Register scratch,
                                             AllocationFlags flags) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Just return if allocation top is already known.
  if ((flags & RESULT_CONTAINS_TOP) != 0) {
    // No use of scratch if allocation top is provided.
    ASSERT(!scratch.is_valid());
#ifdef DEBUG
    // Assert that result actually contains top on entry.
    movq(kScratchRegister, new_space_allocation_top);
    cmpq(result, Operand(kScratchRegister, 0));
    Check(equal, "Unexpected allocation top");
#endif
    return;
  }

  // Move address of new object to result. Use scratch register if available,
  // and keep address in scratch until call to UpdateAllocationTopHelper.
  if (scratch.is_valid()) {
    movq(scratch, new_space_allocation_top);
    movq(result, Operand(scratch, 0));
  } else if (result.is(rax)) {
    load_rax(new_space_allocation_top);
  } else {
    movq(kScratchRegister, new_space_allocation_top);
    movq(result, Operand(kScratchRegister, 0));
  }
}


void MacroAssembler::UpdateAllocationTopHelper(Register result_end,
                                               Register scratch) {
  if (FLAG_debug_code) {
    testq(result_end, Immediate(kObjectAlignmentMask));
    Check(zero, "Unaligned allocation in new space");
  }

  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Update new top.
  if (result_end.is(rax)) {
    // rax can be stored directly to a memory location.
    store_rax(new_space_allocation_top);
  } else {
    // Register required - use scratch provided if available.
    if (scratch.is_valid()) {
      movq(Operand(scratch, 0), result_end);
    } else {
      movq(kScratchRegister, new_space_allocation_top);
      movq(Operand(kScratchRegister, 0), result_end);
    }
  }
}


void MacroAssembler::AllocateInNewSpace(int object_size,
                                        Register result,
                                        Register result_end,
                                        Register scratch,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (FLAG_debug_code) {
      // Trash the registers to simulate an allocation failure.
      movl(result, Immediate(0x7091));
      if (result_end.is_valid()) {
        movl(result_end, Immediate(0x7191));
      }
      if (scratch.is_valid()) {
        movl(scratch, Immediate(0x7291));
      }
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();

  Register top_reg = result_end.is_valid() ? result_end : result;

  if (!top_reg.is(result)) {
    movq(top_reg, result);
  }
  addq(top_reg, Immediate(object_size));
  j(carry, gc_required);
  movq(kScratchRegister, new_space_allocation_limit);
  cmpq(top_reg, Operand(kScratchRegister, 0));
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(top_reg, scratch);

  if (top_reg.is(result)) {
    if ((flags & TAG_OBJECT) != 0) {
      subq(result, Immediate(object_size - kHeapObjectTag));
    } else {
      subq(result, Immediate(object_size));
    }
  } else if ((flags & TAG_OBJECT) != 0) {
    // Tag the result if requested.
    addq(result, Immediate(kHeapObjectTag));
  }
}


void MacroAssembler::AllocateInNewSpace(int header_size,
                                        ScaleFactor element_size,
                                        Register element_count,
                                        Register result,
                                        Register result_end,
                                        Register scratch,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (FLAG_debug_code) {
      // Trash the registers to simulate an allocation failure.
      movl(result, Immediate(0x7091));
      movl(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        movl(scratch, Immediate(0x7291));
      }
      // Register element_count is not modified by the function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();

  // We assume that element_count*element_size + header_size does not
  // overflow.
  lea(result_end, Operand(element_count, element_size, header_size));
  addq(result_end, result);
  j(carry, gc_required);
  movq(kScratchRegister, new_space_allocation_limit);
  cmpq(result_end, Operand(kScratchRegister, 0));
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch);

  // Tag the result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    addq(result, Immediate(kHeapObjectTag));
  }
}


void MacroAssembler::AllocateInNewSpace(Register object_size,
                                        Register result,
                                        Register result_end,
                                        Register scratch,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (FLAG_debug_code) {
      // Trash the registers to simulate an allocation failure.
      movl(result, Immediate(0x7091));
      movl(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        movl(scratch, Immediate(0x7291));
      }
      // object_size is left unchanged by this function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address();
  if (!object_size.is(result_end)) {
    movq(result_end, object_size);
  }
  addq(result_end, result);
  j(carry, gc_required);
  movq(kScratchRegister, new_space_allocation_limit);
  cmpq(result_end, Operand(kScratchRegister, 0));
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch);

  // Tag the result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    addq(result, Immediate(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address();

  // Make sure the object has no tag before resetting top.
  and_(object, Immediate(~kHeapObjectTagMask));
  movq(kScratchRegister, new_space_allocation_top);
#ifdef DEBUG
  cmpq(object, Operand(kScratchRegister, 0));
  Check(below, "Undo allocation of non allocated memory");
#endif
  movq(Operand(kScratchRegister, 0), object);
}


void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  AllocateInNewSpace(HeapNumber::kSize,
                     result,
                     scratch,
                     no_reg,
                     gc_required,
                     TAG_OBJECT);

  // Set the map.
  LoadRoot(kScratchRegister, Heap::kHeapNumberMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  const int kHeaderAlignment = SeqTwoByteString::kHeaderSize &
                               kObjectAlignmentMask;
  ASSERT(kShortSize == 2);
  // scratch1 = length * 2 + kObjectAlignmentMask.
  lea(scratch1, Operand(length, length, times_1, kObjectAlignmentMask +
                kHeaderAlignment));
  and_(scratch1, Immediate(~kObjectAlignmentMask));
  if (kHeaderAlignment > 0) {
    subq(scratch1, Immediate(kHeaderAlignment));
  }

  // Allocate two byte string in new space.
  AllocateInNewSpace(SeqTwoByteString::kHeaderSize,
                     times_1,
                     scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  LoadRoot(kScratchRegister, Heap::kStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
  Integer32ToSmi(scratch1, length);
  movq(FieldOperand(result, String::kLengthOffset), scratch1);
  movq(FieldOperand(result, String::kHashFieldOffset),
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
  const int kHeaderAlignment = SeqAsciiString::kHeaderSize &
                               kObjectAlignmentMask;
  movl(scratch1, length);
  ASSERT(kCharSize == 1);
  addq(scratch1, Immediate(kObjectAlignmentMask + kHeaderAlignment));
  and_(scratch1, Immediate(~kObjectAlignmentMask));
  if (kHeaderAlignment > 0) {
    subq(scratch1, Immediate(kHeaderAlignment));
  }

  // Allocate ascii string in new space.
  AllocateInNewSpace(SeqAsciiString::kHeaderSize,
                     times_1,
                     scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  LoadRoot(kScratchRegister, Heap::kAsciiStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
  Integer32ToSmi(scratch1, length);
  movq(FieldOperand(result, String::kLengthOffset), scratch1);
  movq(FieldOperand(result, String::kHashFieldOffset),
       Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateConsString(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kConsStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  // Allocate heap number in new space.
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kConsAsciiStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    movq(dst, Operand(rsi, Context::SlotOffset(Context::CLOSURE_INDEX)));
    // Load the function context (which is the incoming, outer context).
    movq(dst, FieldOperand(dst, JSFunction::kContextOffset));
    for (int i = 1; i < context_chain_length; i++) {
      movq(dst, Operand(dst, Context::SlotOffset(Context::CLOSURE_INDEX)));
      movq(dst, FieldOperand(dst, JSFunction::kContextOffset));
    }
    // The context may be an intermediate context, not a function context.
    movq(dst, Operand(dst, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in rsi).
    movq(dst, rsi);
  }

  // We should not have found a 'with' context by walking the context chain
  // (i.e., the static scope chain and runtime context chain do not agree).
  // A variable occurring in such a scope should have slot type LOOKUP and
  // not CONTEXT.
  if (FLAG_debug_code) {
    cmpq(dst, Operand(dst, Context::SlotOffset(Context::FCONTEXT_INDEX)));
    Check(equal, "Yo dawg, I heard you liked function contexts "
                 "so I put function contexts in all your contexts");
  }
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  movq(function, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  movq(function, FieldOperand(function, GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  movq(function, Operand(function, Context::SlotOffset(index)));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map) {
  // Load the initial map.  The global functions all have initial maps.
  movq(map, FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (FLAG_debug_code) {
    Label ok, fail;
    CheckMap(map, Factory::meta_map(), &fail, false);
    jmp(&ok);
    bind(&fail);
    Abort("Global functions must have initial map");
    bind(&ok);
  }
}


int MacroAssembler::ArgumentStackSlotsForCFunctionCall(int num_arguments) {
  // On Windows 64 stack slots are reserved by the caller for all arguments
  // including the ones passed in registers, and space is always allocated for
  // the four register arguments even if the function takes fewer than four
  // arguments.
  // On AMD64 ABI (Linux/Mac) the first six arguments are passed in registers
  // and the caller does not reserve stack slots for them.
  ASSERT(num_arguments >= 0);
#ifdef _WIN64
  static const int kMinimumStackSlots = 4;
  if (num_arguments < kMinimumStackSlots) return kMinimumStackSlots;
  return num_arguments;
#else
  static const int kRegisterPassedArguments = 6;
  if (num_arguments < kRegisterPassedArguments) return 0;
  return num_arguments - kRegisterPassedArguments;
#endif
}


void MacroAssembler::PrepareCallCFunction(int num_arguments) {
  int frame_alignment = OS::ActivationFrameAlignment();
  ASSERT(frame_alignment != 0);
  ASSERT(num_arguments >= 0);
  // Make stack end at alignment and allocate space for arguments and old rsp.
  movq(kScratchRegister, rsp);
  ASSERT(IsPowerOf2(frame_alignment));
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  subq(rsp, Immediate((argument_slots_on_stack + 1) * kPointerSize));
  and_(rsp, Immediate(-frame_alignment));
  movq(Operand(rsp, argument_slots_on_stack * kPointerSize), kScratchRegister);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  movq(rax, function);
  CallCFunction(rax, num_arguments);
}


void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  // Check stack alignment.
  if (FLAG_debug_code) {
    CheckStackAlignment();
  }

  call(function);
  ASSERT(OS::ActivationFrameAlignment() != 0);
  ASSERT(num_arguments >= 0);
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  movq(rsp, Operand(rsp, argument_slots_on_stack * kPointerSize));
}


CodePatcher::CodePatcher(byte* address, int size)
    : address_(address), size_(size), masm_(address, size + Assembler::kGap) {
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

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
