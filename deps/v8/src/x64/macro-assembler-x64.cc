// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "macro-assembler-x64.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(void* buffer, int size)
  : Assembler(buffer, size),
    unresolved_(0),
    generating_stub_(false),
    allow_stub_calls_(true),
    code_object_(Heap::undefined_value()) {
}


void MacroAssembler::TailCallRuntime(ExternalReference const& a, int b) {
  UNIMPLEMENTED();
}


void MacroAssembler::Set(Register dst, int64_t x) {
  if (is_int32(x)) {
    movq(dst, Immediate(x));
  } else if (is_uint32(x)) {
    movl(dst, Immediate(x));
  } else {
    movq(dst, x, RelocInfo::NONE);
  }
}


void MacroAssembler::Set(const Operand& dst, int64_t x) {
  if (is_int32(x)) {
    movq(kScratchRegister, Immediate(x));
  } else if (is_uint32(x)) {
    movl(kScratchRegister, Immediate(x));
  } else {
    movq(kScratchRegister, x, RelocInfo::NONE);
  }
  movq(dst, kScratchRegister);
}


void MacroAssembler::PushTryHandler(CodeLocation try_location,
                                    HandlerType type) {
  // The pc (return address) is already on TOS.
  // This code pushes state, code, frame pointer and parameter pointer.
  // Check that they are expected next on the stack, int that order.
  ASSERT_EQ(StackHandlerConstants::kStateOffset,
            StackHandlerConstants::kPCOffset - kPointerSize);
  ASSERT_EQ(StackHandlerConstants::kCodeOffset,
            StackHandlerConstants::kStateOffset - kPointerSize);
  ASSERT_EQ(StackHandlerConstants::kFPOffset,
            StackHandlerConstants::kCodeOffset - kPointerSize);
  ASSERT_EQ(StackHandlerConstants::kPPOffset,
            StackHandlerConstants::kFPOffset - kPointerSize);

  if (try_location == IN_JAVASCRIPT) {
    if (type == TRY_CATCH_HANDLER) {
      push(Immediate(StackHandler::TRY_CATCH));
    } else {
      push(Immediate(StackHandler::TRY_FINALLY));
    }
    push(Immediate(Smi::FromInt(StackHandler::kCodeNotPresent)));
    push(rbp);
    push(rdi);
  } else {
    ASSERT(try_location == IN_JS_ENTRY);
    // The parameter pointer is meaningless here and ebp does not
    // point to a JS frame. So we save NULL for both pp and ebp. We
    // expect the code throwing an exception to check ebp before
    // dereferencing it to restore the context.
    push(Immediate(StackHandler::ENTRY));
    push(Immediate(Smi::FromInt(StackHandler::kCodeNotPresent)));
    push(Immediate(0));  // NULL frame pointer
    push(Immediate(0));  // NULL parameter pointer
  }
  movq(kScratchRegister, ExternalReference(Top::k_handler_address));
  // Cached TOS.
  movq(rax, Operand(kScratchRegister, 0));
  // Link this handler.
  movq(Operand(kScratchRegister, 0), rsp);
}


} }  // namespace v8::internal
