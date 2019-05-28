// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-macro-assembler.h"

#include "src/assembler.h"
#include "src/isolate-inl.h"
#include "src/regexp/regexp-stack.h"
#include "src/simulator.h"
#include "src/unicode-inl.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

RegExpMacroAssembler::RegExpMacroAssembler(Isolate* isolate, Zone* zone)
    : slow_safe_compiler_(false),
      global_mode_(NOT_GLOBAL),
      isolate_(isolate),
      zone_(zone) {}

RegExpMacroAssembler::~RegExpMacroAssembler() = default;

int RegExpMacroAssembler::CaseInsensitiveCompareUC16(Address byte_offset1,
                                                     Address byte_offset2,
                                                     size_t byte_length,
                                                     Isolate* isolate) {
  // This function is not allowed to cause a garbage collection.
  // A GC might move the calling generated code and invalidate the
  // return address on the stack.
  DCHECK_EQ(0, byte_length % 2);

#ifdef V8_INTL_SUPPORT
  int32_t length = (int32_t)(byte_length >> 1);
  icu::UnicodeString uni_str_1(reinterpret_cast<const char16_t*>(byte_offset1),
                               length);
  return uni_str_1.caseCompare(reinterpret_cast<const char16_t*>(byte_offset2),
                               length, U_FOLD_CASE_DEFAULT) == 0;
#else
  uc16* substring1 = reinterpret_cast<uc16*>(byte_offset1);
  uc16* substring2 = reinterpret_cast<uc16*>(byte_offset2);
  size_t length = byte_length >> 1;
  DCHECK_NOT_NULL(isolate);
  unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize =
      isolate->regexp_macro_assembler_canonicalize();
  for (size_t i = 0; i < length; i++) {
    unibrow::uchar c1 = substring1[i];
    unibrow::uchar c2 = substring2[i];
    if (c1 != c2) {
      unibrow::uchar s1[1] = {c1};
      canonicalize->get(c1, '\0', s1);
      if (s1[0] != c2) {
        unibrow::uchar s2[1] = {c2};
        canonicalize->get(c2, '\0', s2);
        if (s1[0] != s2[0]) {
          return 0;
        }
      }
    }
  }
  return 1;
#endif  // V8_INTL_SUPPORT
}


void RegExpMacroAssembler::CheckNotInSurrogatePair(int cp_offset,
                                                   Label* on_failure) {
  Label ok;
  // Check that current character is not a trail surrogate.
  LoadCurrentCharacter(cp_offset, &ok);
  CheckCharacterNotInRange(kTrailSurrogateStart, kTrailSurrogateEnd, &ok);
  // Check that previous character is not a lead surrogate.
  LoadCurrentCharacter(cp_offset - 1, &ok);
  CheckCharacterInRange(kLeadSurrogateStart, kLeadSurrogateEnd, on_failure);
  Bind(&ok);
}

void RegExpMacroAssembler::CheckPosition(int cp_offset,
                                         Label* on_outside_input) {
  LoadCurrentCharacter(cp_offset, on_outside_input, true);
}

bool RegExpMacroAssembler::CheckSpecialCharacterClass(uc16 type,
                                                      Label* on_no_match) {
  return false;
}

NativeRegExpMacroAssembler::NativeRegExpMacroAssembler(Isolate* isolate,
                                                       Zone* zone)
    : RegExpMacroAssembler(isolate, zone) {}

NativeRegExpMacroAssembler::~NativeRegExpMacroAssembler() = default;

bool NativeRegExpMacroAssembler::CanReadUnaligned() {
  return FLAG_enable_regexp_unaligned_accesses && !slow_safe();
}

const byte* NativeRegExpMacroAssembler::StringCharacterPosition(
    String subject, int start_index, const DisallowHeapAllocation& no_gc) {
  if (subject->IsConsString()) {
    subject = ConsString::cast(subject)->first();
  } else if (subject->IsSlicedString()) {
    start_index += SlicedString::cast(subject)->offset();
    subject = SlicedString::cast(subject)->parent();
  }
  if (subject->IsThinString()) {
    subject = ThinString::cast(subject)->actual();
  }
  DCHECK_LE(0, start_index);
  DCHECK_LE(start_index, subject->length());
  if (subject->IsSeqOneByteString()) {
    return reinterpret_cast<const byte*>(
        SeqOneByteString::cast(subject)->GetChars(no_gc) + start_index);
  } else if (subject->IsSeqTwoByteString()) {
    return reinterpret_cast<const byte*>(
        SeqTwoByteString::cast(subject)->GetChars(no_gc) + start_index);
  } else if (subject->IsExternalOneByteString()) {
    return reinterpret_cast<const byte*>(
        ExternalOneByteString::cast(subject)->GetChars() + start_index);
  } else {
    DCHECK(subject->IsExternalTwoByteString());
    return reinterpret_cast<const byte*>(
        ExternalTwoByteString::cast(subject)->GetChars() + start_index);
  }
}

int NativeRegExpMacroAssembler::CheckStackGuardState(
    Isolate* isolate, int start_index, bool is_direct_call,
    Address* return_address, Code re_code, Address* subject,
    const byte** input_start, const byte** input_end) {
  AllowHeapAllocation allow_allocation;
  DCHECK(re_code->raw_instruction_start() <= *return_address);
  DCHECK(*return_address <= re_code->raw_instruction_end());
  int return_value = 0;
  // Prepare for possible GC.
  HandleScope handles(isolate);
  Handle<Code> code_handle(re_code, isolate);
  Handle<String> subject_handle(String::cast(Object(*subject)), isolate);
  bool is_one_byte = String::IsOneByteRepresentationUnderneath(*subject_handle);

  StackLimitCheck check(isolate);
  bool js_has_overflowed = check.JsHasOverflowed();

  if (is_direct_call) {
    // Direct calls from JavaScript can be interrupted in two ways:
    // 1. A real stack overflow, in which case we let the caller throw the
    //    exception.
    // 2. The stack guard was used to interrupt execution for another purpose,
    //    forcing the call through the runtime system.
    return_value = js_has_overflowed ? EXCEPTION : RETRY;
  } else if (js_has_overflowed) {
    isolate->StackOverflow();
    return_value = EXCEPTION;
  } else {
    Object result = isolate->stack_guard()->HandleInterrupts();
    if (result->IsException(isolate)) return_value = EXCEPTION;
  }

  DisallowHeapAllocation no_gc;

  if (*code_handle != re_code) {  // Return address no longer valid
    intptr_t delta = code_handle->address() - re_code->address();
    // Overwrite the return address on the stack.
    *return_address += delta;
  }

  // If we continue, we need to update the subject string addresses.
  if (return_value == 0) {
    // String encoding might have changed.
    if (String::IsOneByteRepresentationUnderneath(*subject_handle) !=
        is_one_byte) {
      // If we changed between an LATIN1 and an UC16 string, the specialized
      // code cannot be used, and we need to restart regexp matching from
      // scratch (including, potentially, compiling a new version of the code).
      return_value = RETRY;
    } else {
      *subject = subject_handle->ptr();
      intptr_t byte_length = *input_end - *input_start;
      *input_start =
          StringCharacterPosition(*subject_handle, start_index, no_gc);
      *input_end = *input_start + byte_length;
    }
  }
  return return_value;
}

// Returns a {Result} sentinel, or the number of successful matches.
int NativeRegExpMacroAssembler::Match(Handle<Code> regexp_code,
                                      Handle<String> subject,
                                      int* offsets_vector,
                                      int offsets_vector_length,
                                      int previous_index, Isolate* isolate) {
  DCHECK(subject->IsFlat());
  DCHECK_LE(0, previous_index);
  DCHECK_LE(previous_index, subject->length());

  // No allocations before calling the regexp, but we can't use
  // DisallowHeapAllocation, since regexps might be preempted, and another
  // thread might do allocation anyway.

  String subject_ptr = *subject;
  // Character offsets into string.
  int start_offset = previous_index;
  int char_length = subject_ptr->length() - start_offset;
  int slice_offset = 0;

  // The string has been flattened, so if it is a cons string it contains the
  // full string in the first part.
  if (StringShape(subject_ptr).IsCons()) {
    DCHECK_EQ(0, ConsString::cast(subject_ptr)->second()->length());
    subject_ptr = ConsString::cast(subject_ptr)->first();
  } else if (StringShape(subject_ptr).IsSliced()) {
    SlicedString slice = SlicedString::cast(subject_ptr);
    subject_ptr = slice->parent();
    slice_offset = slice->offset();
  }
  if (StringShape(subject_ptr).IsThin()) {
    subject_ptr = ThinString::cast(subject_ptr)->actual();
  }
  // Ensure that an underlying string has the same representation.
  bool is_one_byte = subject_ptr->IsOneByteRepresentation();
  DCHECK(subject_ptr->IsExternalString() || subject_ptr->IsSeqString());
  // String is now either Sequential or External
  int char_size_shift = is_one_byte ? 0 : 1;

  DisallowHeapAllocation no_gc;
  const byte* input_start =
      StringCharacterPosition(subject_ptr, start_offset + slice_offset, no_gc);
  int byte_length = char_length << char_size_shift;
  const byte* input_end = input_start + byte_length;
  return Execute(*regexp_code, *subject, start_offset, input_start, input_end,
                 offsets_vector, offsets_vector_length, isolate);
}

// Returns a {Result} sentinel, or the number of successful matches.
int NativeRegExpMacroAssembler::Execute(
    Code code,
    String input,  // This needs to be the unpacked (sliced, cons) string.
    int start_offset, const byte* input_start, const byte* input_end,
    int* output, int output_size, Isolate* isolate) {
  // Ensure that the minimum stack has been allocated.
  RegExpStackScope stack_scope(isolate);
  Address stack_base = stack_scope.stack()->stack_base();

  int direct_call = 0;

  using RegexpMatcherSig = int(
      Address input_string, int start_offset,  // NOLINT(readability/casting)
      const byte* input_start, const byte* input_end, int* output,
      int output_size, Address stack_base, int direct_call, Isolate* isolate);

  auto fn = GeneratedCode<RegexpMatcherSig>::FromCode(code);
  int result =
      fn.CallIrregexp(input.ptr(), start_offset, input_start, input_end, output,
                      output_size, stack_base, direct_call, isolate);
  DCHECK(result >= RETRY);

  if (result == EXCEPTION && !isolate->has_pending_exception()) {
    // We detected a stack overflow (on the backtrack stack) in RegExp code,
    // but haven't created the exception yet. Additionally, we allow heap
    // allocation because even though it invalidates {input_start} and
    // {input_end}, we are about to return anyway.
    AllowHeapAllocation allow_allocation;
    isolate->StackOverflow();
  }
  return result;
}

// clang-format off
const byte NativeRegExpMacroAssembler::word_character_map[] = {
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,

    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // '0' - '7'
    0xFFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,  // '8' - '9'

    0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 'A' - 'G'
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 'H' - 'O'
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 'P' - 'W'
    0xFFu, 0xFFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u, 0xFFu,  // 'X' - 'Z', '_'

    0x00u, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 'a' - 'g'
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 'h' - 'o'
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu,  // 'p' - 'w'
    0xFFu, 0xFFu, 0xFFu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,  // 'x' - 'z'
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
// clang-format on

Address NativeRegExpMacroAssembler::GrowStack(Address stack_pointer,
                                              Address* stack_base,
                                              Isolate* isolate) {
  RegExpStack* regexp_stack = isolate->regexp_stack();
  size_t size = regexp_stack->stack_capacity();
  Address old_stack_base = regexp_stack->stack_base();
  DCHECK(old_stack_base == *stack_base);
  DCHECK(stack_pointer <= old_stack_base);
  DCHECK(static_cast<size_t>(old_stack_base - stack_pointer) <= size);
  Address new_stack_base = regexp_stack->EnsureCapacity(size * 2);
  if (new_stack_base == kNullAddress) {
    return kNullAddress;
  }
  *stack_base = new_stack_base;
  intptr_t stack_content_size = old_stack_base - stack_pointer;
  return new_stack_base - stack_content_size;
}

}  // namespace internal
}  // namespace v8
