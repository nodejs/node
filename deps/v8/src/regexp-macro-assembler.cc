// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"
#include "ast.h"
#include "assembler.h"
#include "regexp-stack.h"
#include "regexp-macro-assembler.h"
#include "simulator.h"

namespace v8 {
namespace internal {

RegExpMacroAssembler::RegExpMacroAssembler(Zone* zone)
  : slow_safe_compiler_(false),
    global_mode_(NOT_GLOBAL),
    zone_(zone) {
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


#ifndef V8_INTERPRETED_REGEXP  // Avoid unused code, e.g., on ARM.

NativeRegExpMacroAssembler::NativeRegExpMacroAssembler(Zone* zone)
    : RegExpMacroAssembler(zone) {
}


NativeRegExpMacroAssembler::~NativeRegExpMacroAssembler() {
}


bool NativeRegExpMacroAssembler::CanReadUnaligned() {
  return FLAG_enable_unaligned_accesses && !slow_safe();
}

const byte* NativeRegExpMacroAssembler::StringCharacterPosition(
    String* subject,
    int start_index) {
  // Not just flat, but ultra flat.
  ASSERT(subject->IsExternalString() || subject->IsSeqString());
  ASSERT(start_index >= 0);
  ASSERT(start_index <= subject->length());
  if (subject->IsOneByteRepresentation()) {
    const byte* address;
    if (StringShape(subject).IsExternal()) {
      const uint8_t* data = ExternalAsciiString::cast(subject)->GetChars();
      address = reinterpret_cast<const byte*>(data);
    } else {
      ASSERT(subject->IsSeqOneByteString());
      const uint8_t* data = SeqOneByteString::cast(subject)->GetChars();
      address = reinterpret_cast<const byte*>(data);
    }
    return address + start_index;
  }
  const uc16* data;
  if (StringShape(subject).IsExternal()) {
    data = ExternalTwoByteString::cast(subject)->GetChars();
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
    int previous_index,
    Isolate* isolate) {

  ASSERT(subject->IsFlat());
  ASSERT(previous_index >= 0);
  ASSERT(previous_index <= subject->length());

  // No allocations before calling the regexp, but we can't use
  // DisallowHeapAllocation, since regexps might be preempted, and another
  // thread might do allocation anyway.

  String* subject_ptr = *subject;
  // Character offsets into string.
  int start_offset = previous_index;
  int char_length = subject_ptr->length() - start_offset;
  int slice_offset = 0;

  // The string has been flattened, so if it is a cons string it contains the
  // full string in the first part.
  if (StringShape(subject_ptr).IsCons()) {
    ASSERT_EQ(0, ConsString::cast(subject_ptr)->second()->length());
    subject_ptr = ConsString::cast(subject_ptr)->first();
  } else if (StringShape(subject_ptr).IsSliced()) {
    SlicedString* slice = SlicedString::cast(subject_ptr);
    subject_ptr = slice->parent();
    slice_offset = slice->offset();
  }
  // Ensure that an underlying string has the same ASCII-ness.
  bool is_ascii = subject_ptr->IsOneByteRepresentation();
  ASSERT(subject_ptr->IsExternalString() || subject_ptr->IsSeqString());
  // String is now either Sequential or External
  int char_size_shift = is_ascii ? 0 : 1;

  const byte* input_start =
      StringCharacterPosition(subject_ptr, start_offset + slice_offset);
  int byte_length = char_length << char_size_shift;
  const byte* input_end = input_start + byte_length;
  Result res = Execute(*regexp_code,
                       *subject,
                       start_offset,
                       input_start,
                       input_end,
                       offsets_vector,
                       offsets_vector_length,
                       isolate);
  return res;
}


NativeRegExpMacroAssembler::Result NativeRegExpMacroAssembler::Execute(
    Code* code,
    String* input,  // This needs to be the unpacked (sliced, cons) string.
    int start_offset,
    const byte* input_start,
    const byte* input_end,
    int* output,
    int output_size,
    Isolate* isolate) {
  // Ensure that the minimum stack has been allocated.
  RegExpStackScope stack_scope(isolate);
  Address stack_base = stack_scope.stack()->stack_base();

  int direct_call = 0;
  int result = CALL_GENERATED_REGEXP_CODE(code->entry(),
                                          input,
                                          start_offset,
                                          input_start,
                                          input_end,
                                          output,
                                          output_size,
                                          stack_base,
                                          direct_call,
                                          isolate);
  ASSERT(result >= RETRY);

  if (result == EXCEPTION && !isolate->has_pending_exception()) {
    // We detected a stack overflow (on the backtrack stack) in RegExp code,
    // but haven't created the exception yet.
    isolate->StackOverflow();
  }
  return static_cast<Result>(result);
}


const byte NativeRegExpMacroAssembler::word_character_map[] = {
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,

    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // '0' - '7'
    0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,  // '8' - '9'

    0x00u, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // 'A' - 'G'
    0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // 'H' - 'O'
    0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // 'P' - 'W'
    0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu,  // 'X' - 'Z', '_'

    0x00u, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // 'a' - 'g'
    0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // 'h' - 'o'
    0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu,  // 'p' - 'w'
    0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,  // 'x' - 'z'
    // Latin-1 range
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,

    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,

    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,

    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};


int NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16(
    Address byte_offset1,
    Address byte_offset2,
    size_t byte_length,
    Isolate* isolate) {
  unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize =
      isolate->regexp_macro_assembler_canonicalize();
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
      canonicalize->get(c1, '\0', s1);
      if (s1[0] != c2) {
        unibrow::uchar s2[1] = { c2 };
        canonicalize->get(c2, '\0', s2);
        if (s1[0] != s2[0]) {
          return 0;
        }
      }
    }
  }
  return 1;
}


Address NativeRegExpMacroAssembler::GrowStack(Address stack_pointer,
                                              Address* stack_base,
                                              Isolate* isolate) {
  RegExpStack* regexp_stack = isolate->regexp_stack();
  size_t size = regexp_stack->stack_capacity();
  Address old_stack_base = regexp_stack->stack_base();
  ASSERT(old_stack_base == *stack_base);
  ASSERT(stack_pointer <= old_stack_base);
  ASSERT(static_cast<size_t>(old_stack_base - stack_pointer) <= size);
  Address new_stack_base = regexp_stack->EnsureCapacity(size * 2);
  if (new_stack_base == NULL) {
    return NULL;
  }
  *stack_base = new_stack_base;
  intptr_t stack_content_size = old_stack_base - stack_pointer;
  return new_stack_base - stack_content_size;
}

#endif  // V8_INTERPRETED_REGEXP

} }  // namespace v8::internal
