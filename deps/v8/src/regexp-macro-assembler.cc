// Copyright 2008 the V8 project authors. All rights reserved.
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
#include "ast.h"
#include "assembler.h"
#include "regexp-stack.h"
#include "regexp-macro-assembler.h"
#include "simulator.h"

namespace v8 {
namespace internal {

RegExpMacroAssembler::RegExpMacroAssembler() {
}


RegExpMacroAssembler::~RegExpMacroAssembler() {
}


bool RegExpMacroAssembler::CanReadUnaligned() {
#ifdef V8_HOST_CAN_READ_UNALIGNED
  return true;
#else
  return false;
#endif
}


#ifdef V8_NATIVE_REGEXP  // Avoid unused code, e.g., on ARM.

NativeRegExpMacroAssembler::NativeRegExpMacroAssembler() {
}


NativeRegExpMacroAssembler::~NativeRegExpMacroAssembler() {
}


bool NativeRegExpMacroAssembler::CanReadUnaligned() {
#ifdef V8_TARGET_CAN_READ_UNALIGNED
  return true;
#else
  return false;
#endif
}

const byte* NativeRegExpMacroAssembler::StringCharacterPosition(
    String* subject,
    int start_index) {
  // Not just flat, but ultra flat.
  ASSERT(subject->IsExternalString() || subject->IsSeqString());
  ASSERT(start_index >= 0);
  ASSERT(start_index <= subject->length());
  if (subject->IsAsciiRepresentation()) {
    const byte* address;
    if (StringShape(subject).IsExternal()) {
      const char* data = ExternalAsciiString::cast(subject)->resource()->data();
      address = reinterpret_cast<const byte*>(data);
    } else {
      ASSERT(subject->IsSeqAsciiString());
      char* data = SeqAsciiString::cast(subject)->GetChars();
      address = reinterpret_cast<const byte*>(data);
    }
    return address + start_index;
  }
  const uc16* data;
  if (StringShape(subject).IsExternal()) {
    data = ExternalTwoByteString::cast(subject)->resource()->data();
  } else {
    ASSERT(subject->IsSeqTwoByteString());
    data = SeqTwoByteString::cast(subject)->GetChars();
  }
  return reinterpret_cast<const byte*>(data + start_index);
}


NativeRegExpMacroAssembler::Result NativeRegExpMacroAssembler::Match(
    Handle<Code> regexp_code,
    Handle<String> subject,
    int* offsets_vector,
    int offsets_vector_length,
    int previous_index) {

  ASSERT(subject->IsFlat());
  ASSERT(previous_index >= 0);
  ASSERT(previous_index <= subject->length());

  // No allocations before calling the regexp, but we can't use
  // AssertNoAllocation, since regexps might be preempted, and another thread
  // might do allocation anyway.

  String* subject_ptr = *subject;
  // Character offsets into string.
  int start_offset = previous_index;
  int end_offset = subject_ptr->length();

  bool is_ascii = subject->IsAsciiRepresentation();

  if (StringShape(subject_ptr).IsCons()) {
    subject_ptr = ConsString::cast(subject_ptr)->first();
  }
  // Ensure that an underlying string has the same ascii-ness.
  ASSERT(subject_ptr->IsAsciiRepresentation() == is_ascii);
  ASSERT(subject_ptr->IsExternalString() || subject_ptr->IsSeqString());
  // String is now either Sequential or External
  int char_size_shift = is_ascii ? 0 : 1;
  int char_length = end_offset - start_offset;

  const byte* input_start =
      StringCharacterPosition(subject_ptr, start_offset);
  int byte_length = char_length << char_size_shift;
  const byte* input_end = input_start + byte_length;
  Result res = Execute(*regexp_code,
                       subject_ptr,
                       start_offset,
                       input_start,
                       input_end,
                       offsets_vector,
                       previous_index == 0);
  return res;
}


NativeRegExpMacroAssembler::Result NativeRegExpMacroAssembler::Execute(
    Code* code,
    String* input,
    int start_offset,
    const byte* input_start,
    const byte* input_end,
    int* output,
    bool at_start) {
  typedef int (*matcher)(String*, int, const byte*,
                         const byte*, int*, int, Address, int);
  matcher matcher_func = FUNCTION_CAST<matcher>(code->entry());

  int at_start_val = at_start ? 1 : 0;

  // Ensure that the minimum stack has been allocated.
  RegExpStack stack;
  Address stack_base = RegExpStack::stack_base();

  int direct_call = 0;
  int result = CALL_GENERATED_REGEXP_CODE(matcher_func,
                                          input,
                                          start_offset,
                                          input_start,
                                          input_end,
                                          output,
                                          at_start_val,
                                          stack_base,
                                          direct_call);
  ASSERT(result <= SUCCESS);
  ASSERT(result >= RETRY);

  if (result == EXCEPTION && !Top::has_pending_exception()) {
    // We detected a stack overflow (on the backtrack stack) in RegExp code,
    // but haven't created the exception yet.
    Top::StackOverflow();
  }
  return static_cast<Result>(result);
}


static unibrow::Mapping<unibrow::Ecma262Canonicalize> canonicalize;

int NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16(
    Address byte_offset1,
    Address byte_offset2,
    size_t byte_length) {
  // This function is not allowed to cause a garbage collection.
  // A GC might move the calling generated code and invalidate the
  // return address on the stack.
  ASSERT(byte_length % 2 == 0);
  uc16* substring1 = reinterpret_cast<uc16*>(byte_offset1);
  uc16* substring2 = reinterpret_cast<uc16*>(byte_offset2);
  size_t length = byte_length >> 1;

  for (size_t i = 0; i < length; i++) {
    unibrow::uchar c1 = substring1[i];
    unibrow::uchar c2 = substring2[i];
    if (c1 != c2) {
      unibrow::uchar s1[1] = { c1 };
      canonicalize.get(c1, '\0', s1);
      if (s1[0] != c2) {
        unibrow::uchar s2[1] = { c2 };
        canonicalize.get(c2, '\0', s2);
        if (s1[0] != s2[0]) {
          return 0;
        }
      }
    }
  }
  return 1;
}


Address NativeRegExpMacroAssembler::GrowStack(Address stack_pointer,
                                              Address* stack_base) {
  size_t size = RegExpStack::stack_capacity();
  Address old_stack_base = RegExpStack::stack_base();
  ASSERT(old_stack_base == *stack_base);
  ASSERT(stack_pointer <= old_stack_base);
  ASSERT(static_cast<size_t>(old_stack_base - stack_pointer) <= size);
  Address new_stack_base = RegExpStack::EnsureCapacity(size * 2);
  if (new_stack_base == NULL) {
    return NULL;
  }
  *stack_base = new_stack_base;
  intptr_t stack_content_size = old_stack_base - stack_pointer;
  return new_stack_base - stack_content_size;
}

#endif  // V8_NATIVE_REGEXP
} }  // namespace v8::internal
