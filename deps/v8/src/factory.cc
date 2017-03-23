// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/compiler.h"
#include "src/conversions.h"
#include "src/isolate-inl.h"
#include "src/macro-assembler.h"
#include "src/objects/module-info.h"
#include "src/objects/scope-info.h"

namespace v8 {
namespace internal {


// Calls the FUNCTION_CALL function and retries it up to three times
// to guarantee that any allocations performed during the call will
// succeed if there's enough memory.
//
// Warning: Do not use the identifiers __object__, __maybe_object__,
// __allocation__ or __scope__ in a call to this macro.

#define RETURN_OBJECT_UNLESS_RETRY(ISOLATE, TYPE)         \
  if (__allocation__.To(&__object__)) {                   \
    DCHECK(__object__ != (ISOLATE)->heap()->exception()); \
    return Handle<TYPE>(TYPE::cast(__object__), ISOLATE); \
  }

#define CALL_HEAP_FUNCTION(ISOLATE, FUNCTION_CALL, TYPE)                      \
  do {                                                                        \
    AllocationResult __allocation__ = FUNCTION_CALL;                          \
    Object* __object__ = NULL;                                                \
    RETURN_OBJECT_UNLESS_RETRY(ISOLATE, TYPE)                                 \
    /* Two GCs before panicking.  In newspace will almost always succeed. */  \
    for (int __i__ = 0; __i__ < 2; __i__++) {                                 \
      (ISOLATE)->heap()->CollectGarbage(                                      \
          __allocation__.RetrySpace(),                                        \
          GarbageCollectionReason::kAllocationFailure);                       \
      __allocation__ = FUNCTION_CALL;                                         \
      RETURN_OBJECT_UNLESS_RETRY(ISOLATE, TYPE)                               \
    }                                                                         \
    (ISOLATE)->counters()->gc_last_resort_from_handles()->Increment();        \
    (ISOLATE)->heap()->CollectAllAvailableGarbage(                            \
        GarbageCollectionReason::kLastResort);                                \
    {                                                                         \
      AlwaysAllocateScope __scope__(ISOLATE);                                 \
      __allocation__ = FUNCTION_CALL;                                         \
    }                                                                         \
    RETURN_OBJECT_UNLESS_RETRY(ISOLATE, TYPE)                                 \
    /* TODO(1181417): Fix this. */                                            \
    v8::internal::Heap::FatalProcessOutOfMemory("CALL_AND_RETRY_LAST", true); \
    return Handle<TYPE>();                                                    \
  } while (false)

template<typename T>
Handle<T> Factory::New(Handle<Map> map, AllocationSpace space) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->Allocate(*map, space),
      T);
}


template<typename T>
Handle<T> Factory::New(Handle<Map> map,
                       AllocationSpace space,
                       Handle<AllocationSite> allocation_site) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->Allocate(*map, space, *allocation_site),
      T);
}


Handle<HeapObject> Factory::NewFillerObject(int size,
                                            bool double_align,
                                            AllocationSpace space) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateFillerObject(size, double_align, space),
      HeapObject);
}


Handle<Box> Factory::NewBox(Handle<Object> value) {
  Handle<Box> result = Handle<Box>::cast(NewStruct(BOX_TYPE));
  result->set_value(*value);
  return result;
}

Handle<PrototypeInfo> Factory::NewPrototypeInfo() {
  Handle<PrototypeInfo> result =
      Handle<PrototypeInfo>::cast(NewStruct(PROTOTYPE_INFO_TYPE));
  result->set_prototype_users(WeakFixedArray::Empty());
  result->set_registry_slot(PrototypeInfo::UNREGISTERED);
  result->set_validity_cell(Smi::kZero);
  result->set_bit_field(0);
  return result;
}

Handle<Tuple2> Factory::NewTuple2(Handle<Object> value1,
                                  Handle<Object> value2) {
  Handle<Tuple2> result = Handle<Tuple2>::cast(NewStruct(TUPLE2_TYPE));
  result->set_value1(*value1);
  result->set_value2(*value2);
  return result;
}

Handle<Tuple3> Factory::NewTuple3(Handle<Object> value1, Handle<Object> value2,
                                  Handle<Object> value3) {
  Handle<Tuple3> result = Handle<Tuple3>::cast(NewStruct(TUPLE3_TYPE));
  result->set_value1(*value1);
  result->set_value2(*value2);
  result->set_value3(*value3);
  return result;
}

Handle<ContextExtension> Factory::NewContextExtension(
    Handle<ScopeInfo> scope_info, Handle<Object> extension) {
  Handle<ContextExtension> result =
      Handle<ContextExtension>::cast(NewStruct(CONTEXT_EXTENSION_TYPE));
  result->set_scope_info(*scope_info);
  result->set_extension(*extension);
  return result;
}

Handle<ConstantElementsPair> Factory::NewConstantElementsPair(
    ElementsKind elements_kind, Handle<FixedArrayBase> constant_values) {
  Handle<ConstantElementsPair> result = Handle<ConstantElementsPair>::cast(
      NewStruct(CONSTANT_ELEMENTS_PAIR_TYPE));
  result->set_elements_kind(elements_kind);
  result->set_constant_values(*constant_values);
  return result;
}

Handle<Oddball> Factory::NewOddball(Handle<Map> map, const char* to_string,
                                    Handle<Object> to_number,
                                    const char* type_of, byte kind) {
  Handle<Oddball> oddball = New<Oddball>(map, OLD_SPACE);
  Oddball::Initialize(isolate(), oddball, to_string, to_number, type_of, kind);
  return oddball;
}


Handle<FixedArray> Factory::NewFixedArray(int size, PretenureFlag pretenure) {
  DCHECK(0 <= size);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateFixedArray(size, pretenure),
      FixedArray);
}

MaybeHandle<FixedArray> Factory::TryNewFixedArray(int size,
                                                  PretenureFlag pretenure) {
  DCHECK(0 <= size);
  AllocationResult allocation =
      isolate()->heap()->AllocateFixedArray(size, pretenure);
  Object* array = NULL;
  if (!allocation.To(&array)) return MaybeHandle<FixedArray>();
  return Handle<FixedArray>(FixedArray::cast(array), isolate());
}

Handle<FixedArray> Factory::NewFixedArrayWithHoles(int size,
                                                   PretenureFlag pretenure) {
  DCHECK(0 <= size);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateFixedArrayWithFiller(size,
                                                      pretenure,
                                                      *the_hole_value()),
      FixedArray);
}

Handle<FixedArray> Factory::NewUninitializedFixedArray(int size) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateUninitializedFixedArray(size),
      FixedArray);
}


Handle<FixedArrayBase> Factory::NewFixedDoubleArray(int size,
                                                    PretenureFlag pretenure) {
  DCHECK(0 <= size);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateUninitializedFixedDoubleArray(size, pretenure),
      FixedArrayBase);
}


Handle<FixedArrayBase> Factory::NewFixedDoubleArrayWithHoles(
    int size,
    PretenureFlag pretenure) {
  DCHECK(0 <= size);
  Handle<FixedArrayBase> array = NewFixedDoubleArray(size, pretenure);
  if (size > 0) {
    Handle<FixedDoubleArray>::cast(array)->FillWithHoles(0, size);
  }
  return array;
}

Handle<FrameArray> Factory::NewFrameArray(int number_of_frames,
                                          PretenureFlag pretenure) {
  DCHECK_LE(0, number_of_frames);
  Handle<FixedArray> result =
      NewFixedArrayWithHoles(FrameArray::LengthFor(number_of_frames));
  result->set(FrameArray::kFrameCountIndex, Smi::kZero);
  return Handle<FrameArray>::cast(result);
}

Handle<OrderedHashSet> Factory::NewOrderedHashSet() {
  return OrderedHashSet::Allocate(isolate(), OrderedHashSet::kMinCapacity);
}


Handle<OrderedHashMap> Factory::NewOrderedHashMap() {
  return OrderedHashMap::Allocate(isolate(), OrderedHashMap::kMinCapacity);
}


Handle<AccessorPair> Factory::NewAccessorPair() {
  Handle<AccessorPair> accessors =
      Handle<AccessorPair>::cast(NewStruct(ACCESSOR_PAIR_TYPE));
  accessors->set_getter(*null_value(), SKIP_WRITE_BARRIER);
  accessors->set_setter(*null_value(), SKIP_WRITE_BARRIER);
  return accessors;
}


Handle<TypeFeedbackInfo> Factory::NewTypeFeedbackInfo() {
  Handle<TypeFeedbackInfo> info =
      Handle<TypeFeedbackInfo>::cast(NewStruct(TYPE_FEEDBACK_INFO_TYPE));
  info->initialize_storage();
  return info;
}


// Internalized strings are created in the old generation (data space).
Handle<String> Factory::InternalizeUtf8String(Vector<const char> string) {
  Utf8StringKey key(string, isolate()->heap()->HashSeed());
  return InternalizeStringWithKey(&key);
}


Handle<String> Factory::InternalizeOneByteString(Vector<const uint8_t> string) {
  OneByteStringKey key(string, isolate()->heap()->HashSeed());
  return InternalizeStringWithKey(&key);
}


Handle<String> Factory::InternalizeOneByteString(
    Handle<SeqOneByteString> string, int from, int length) {
  SeqOneByteSubStringKey key(string, from, length);
  return InternalizeStringWithKey(&key);
}


Handle<String> Factory::InternalizeTwoByteString(Vector<const uc16> string) {
  TwoByteStringKey key(string, isolate()->heap()->HashSeed());
  return InternalizeStringWithKey(&key);
}


template<class StringTableKey>
Handle<String> Factory::InternalizeStringWithKey(StringTableKey* key) {
  return StringTable::LookupKey(isolate(), key);
}


MaybeHandle<String> Factory::NewStringFromOneByte(Vector<const uint8_t> string,
                                                  PretenureFlag pretenure) {
  int length = string.length();
  if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
  Handle<SeqOneByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(),
      result,
      NewRawOneByteString(string.length(), pretenure),
      String);

  DisallowHeapAllocation no_gc;
  // Copy the characters into the new object.
  CopyChars(SeqOneByteString::cast(*result)->GetChars(),
            string.start(),
            length);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8(Vector<const char> string,
                                               PretenureFlag pretenure) {
  // Check for ASCII first since this is the common case.
  const char* start = string.start();
  int length = string.length();
  int non_ascii_start = String::NonAsciiStart(start, length);
  if (non_ascii_start >= length) {
    // If the string is ASCII, we do not need to convert the characters
    // since UTF8 is backwards compatible with ASCII.
    return NewStringFromOneByte(Vector<const uint8_t>::cast(string), pretenure);
  }

  // Non-ASCII and we need to decode.
  Access<UnicodeCache::Utf8Decoder>
      decoder(isolate()->unicode_cache()->utf8_decoder());
  decoder->Reset(string.start() + non_ascii_start,
                 length - non_ascii_start);
  int utf16_length = static_cast<int>(decoder->Utf16Length());
  DCHECK(utf16_length > 0);
  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(non_ascii_start + utf16_length, pretenure),
      String);
  // Copy ASCII portion.
  uint16_t* data = result->GetChars();
  const char* ascii_data = string.start();
  for (int i = 0; i < non_ascii_start; i++) {
    *data++ = *ascii_data++;
  }
  // Now write the remainder.
  decoder->WriteUtf16(data, utf16_length);
  return result;
}

MaybeHandle<String> Factory::NewStringFromUtf8SubString(
    Handle<SeqOneByteString> str, int begin, int length,
    PretenureFlag pretenure) {
  // Check for ASCII first since this is the common case.
  const char* start = reinterpret_cast<const char*>(str->GetChars() + begin);
  int non_ascii_start = String::NonAsciiStart(start, length);
  if (non_ascii_start >= length) {
    // If the string is ASCII, we can just make a substring.
    // TODO(v8): the pretenure flag is ignored in this case.
    return NewSubString(str, begin, begin + length);
  }

  // Non-ASCII and we need to decode.
  Access<UnicodeCache::Utf8Decoder> decoder(
      isolate()->unicode_cache()->utf8_decoder());
  decoder->Reset(start + non_ascii_start, length - non_ascii_start);
  int utf16_length = static_cast<int>(decoder->Utf16Length());
  DCHECK(utf16_length > 0);
  // Allocate string.
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      NewRawTwoByteString(non_ascii_start + utf16_length, pretenure), String);

  // Reset the decoder, because the original {str} may have moved.
  const char* ascii_data =
      reinterpret_cast<const char*>(str->GetChars() + begin);
  decoder->Reset(ascii_data + non_ascii_start, length - non_ascii_start);
  // Copy ASCII portion.
  uint16_t* data = result->GetChars();
  for (int i = 0; i < non_ascii_start; i++) {
    *data++ = *ascii_data++;
  }
  // Now write the remainder.
  decoder->WriteUtf16(data, utf16_length);
  return result;
}

MaybeHandle<String> Factory::NewStringFromTwoByte(const uc16* string,
                                                  int length,
                                                  PretenureFlag pretenure) {
  if (String::IsOneByte(string, length)) {
    if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(),
        result,
        NewRawOneByteString(length, pretenure),
        String);
    CopyChars(result->GetChars(), string, length);
    return result;
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(),
        result,
        NewRawTwoByteString(length, pretenure),
        String);
    CopyChars(result->GetChars(), string, length);
    return result;
  }
}

MaybeHandle<String> Factory::NewStringFromTwoByte(Vector<const uc16> string,
                                                  PretenureFlag pretenure) {
  return NewStringFromTwoByte(string.start(), string.length(), pretenure);
}

MaybeHandle<String> Factory::NewStringFromTwoByte(
    const ZoneVector<uc16>* string, PretenureFlag pretenure) {
  return NewStringFromTwoByte(string->data(), static_cast<int>(string->size()),
                              pretenure);
}

Handle<String> Factory::NewInternalizedStringFromUtf8(Vector<const char> str,
                                                      int chars,
                                                      uint32_t hash_field) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateInternalizedStringFromUtf8(
          str, chars, hash_field),
      String);
}


MUST_USE_RESULT Handle<String> Factory::NewOneByteInternalizedString(
      Vector<const uint8_t> str,
      uint32_t hash_field) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateOneByteInternalizedString(str, hash_field),
      String);
}


MUST_USE_RESULT Handle<String> Factory::NewOneByteInternalizedSubString(
    Handle<SeqOneByteString> string, int offset, int length,
    uint32_t hash_field) {
  CALL_HEAP_FUNCTION(
      isolate(), isolate()->heap()->AllocateOneByteInternalizedString(
                     Vector<const uint8_t>(string->GetChars() + offset, length),
                     hash_field),
      String);
}


MUST_USE_RESULT Handle<String> Factory::NewTwoByteInternalizedString(
      Vector<const uc16> str,
      uint32_t hash_field) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateTwoByteInternalizedString(str, hash_field),
      String);
}


Handle<String> Factory::NewInternalizedStringImpl(
    Handle<String> string, int chars, uint32_t hash_field) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateInternalizedStringImpl(
          *string, chars, hash_field),
      String);
}

MaybeHandle<Map> Factory::InternalizedStringMapForString(
    Handle<String> string) {
  // If the string is in new space it cannot be used as internalized.
  if (isolate()->heap()->InNewSpace(*string)) return MaybeHandle<Map>();

  // Find the corresponding internalized string map for strings.
  switch (string->map()->instance_type()) {
    case STRING_TYPE:
      return internalized_string_map();
    case ONE_BYTE_STRING_TYPE:
      return one_byte_internalized_string_map();
    case EXTERNAL_STRING_TYPE:
      return external_internalized_string_map();
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
      return external_one_byte_internalized_string_map();
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return external_internalized_string_with_one_byte_data_map();
    case SHORT_EXTERNAL_STRING_TYPE:
      return short_external_internalized_string_map();
    case SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE:
      return short_external_one_byte_internalized_string_map();
    case SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return short_external_internalized_string_with_one_byte_data_map();
    default: return MaybeHandle<Map>();  // No match found.
  }
}


MaybeHandle<SeqOneByteString> Factory::NewRawOneByteString(
    int length, PretenureFlag pretenure) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqOneByteString);
  }
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateRawOneByteString(length, pretenure),
      SeqOneByteString);
}


MaybeHandle<SeqTwoByteString> Factory::NewRawTwoByteString(
    int length, PretenureFlag pretenure) {
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), SeqTwoByteString);
  }
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateRawTwoByteString(length, pretenure),
      SeqTwoByteString);
}


Handle<String> Factory::LookupSingleCharacterStringFromCode(uint32_t code) {
  if (code <= String::kMaxOneByteCharCodeU) {
    {
      DisallowHeapAllocation no_allocation;
      Object* value = single_character_string_cache()->get(code);
      if (value != *undefined_value()) {
        return handle(String::cast(value), isolate());
      }
    }
    uint8_t buffer[1];
    buffer[0] = static_cast<uint8_t>(code);
    Handle<String> result =
        InternalizeOneByteString(Vector<const uint8_t>(buffer, 1));
    single_character_string_cache()->set(code, *result);
    return result;
  }
  DCHECK(code <= String::kMaxUtf16CodeUnitU);

  Handle<SeqTwoByteString> result = NewRawTwoByteString(1).ToHandleChecked();
  result->SeqTwoByteStringSet(0, static_cast<uint16_t>(code));
  return result;
}


// Returns true for a character in a range.  Both limits are inclusive.
static inline bool Between(uint32_t character, uint32_t from, uint32_t to) {
  // This makes uses of the the unsigned wraparound.
  return character - from <= to - from;
}


static inline Handle<String> MakeOrFindTwoCharacterString(Isolate* isolate,
                                                          uint16_t c1,
                                                          uint16_t c2) {
  // Numeric strings have a different hash algorithm not known by
  // LookupTwoCharsStringIfExists, so we skip this step for such strings.
  if (!Between(c1, '0', '9') || !Between(c2, '0', '9')) {
    Handle<String> result;
    if (StringTable::LookupTwoCharsStringIfExists(isolate, c1, c2).
        ToHandle(&result)) {
      return result;
    }
  }

  // Now we know the length is 2, we might as well make use of that fact
  // when building the new string.
  if (static_cast<unsigned>(c1 | c2) <= String::kMaxOneByteCharCodeU) {
    // We can do this.
    DCHECK(base::bits::IsPowerOfTwo32(String::kMaxOneByteCharCodeU +
                                      1));  // because of this.
    Handle<SeqOneByteString> str =
        isolate->factory()->NewRawOneByteString(2).ToHandleChecked();
    uint8_t* dest = str->GetChars();
    dest[0] = static_cast<uint8_t>(c1);
    dest[1] = static_cast<uint8_t>(c2);
    return str;
  } else {
    Handle<SeqTwoByteString> str =
        isolate->factory()->NewRawTwoByteString(2).ToHandleChecked();
    uc16* dest = str->GetChars();
    dest[0] = c1;
    dest[1] = c2;
    return str;
  }
}


template<typename SinkChar, typename StringType>
Handle<String> ConcatStringContent(Handle<StringType> result,
                                   Handle<String> first,
                                   Handle<String> second) {
  DisallowHeapAllocation pointer_stays_valid;
  SinkChar* sink = result->GetChars();
  String::WriteToFlat(*first, sink, 0, first->length());
  String::WriteToFlat(*second, sink + first->length(), 0, second->length());
  return result;
}


MaybeHandle<String> Factory::NewConsString(Handle<String> left,
                                           Handle<String> right) {
  int left_length = left->length();
  if (left_length == 0) return right;
  int right_length = right->length();
  if (right_length == 0) return left;

  int length = left_length + right_length;

  if (length == 2) {
    uint16_t c1 = left->Get(0);
    uint16_t c2 = right->Get(0);
    return MakeOrFindTwoCharacterString(isolate(), c1, c2);
  }

  // Make sure that an out of memory exception is thrown if the length
  // of the new cons string is too large.
  if (length > String::kMaxLength || length < 0) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }

  bool left_is_one_byte = left->IsOneByteRepresentation();
  bool right_is_one_byte = right->IsOneByteRepresentation();
  bool is_one_byte = left_is_one_byte && right_is_one_byte;
  bool is_one_byte_data_in_two_byte_string = false;
  if (!is_one_byte) {
    // At least one of the strings uses two-byte representation so we
    // can't use the fast case code for short one-byte strings below, but
    // we can try to save memory if all chars actually fit in one-byte.
    is_one_byte_data_in_two_byte_string =
        left->HasOnlyOneByteChars() && right->HasOnlyOneByteChars();
    if (is_one_byte_data_in_two_byte_string) {
      isolate()->counters()->string_add_runtime_ext_to_one_byte()->Increment();
    }
  }

  // If the resulting string is small make a flat string.
  if (length < ConsString::kMinLength) {
    // Note that neither of the two inputs can be a slice because:
    STATIC_ASSERT(ConsString::kMinLength <= SlicedString::kMinLength);
    DCHECK(left->IsFlat());
    DCHECK(right->IsFlat());

    STATIC_ASSERT(ConsString::kMinLength <= String::kMaxLength);
    if (is_one_byte) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length).ToHandleChecked();
      DisallowHeapAllocation no_gc;
      uint8_t* dest = result->GetChars();
      // Copy left part.
      const uint8_t* src =
          left->IsExternalString()
              ? Handle<ExternalOneByteString>::cast(left)->GetChars()
              : Handle<SeqOneByteString>::cast(left)->GetChars();
      for (int i = 0; i < left_length; i++) *dest++ = src[i];
      // Copy right part.
      src = right->IsExternalString()
                ? Handle<ExternalOneByteString>::cast(right)->GetChars()
                : Handle<SeqOneByteString>::cast(right)->GetChars();
      for (int i = 0; i < right_length; i++) *dest++ = src[i];
      return result;
    }

    return (is_one_byte_data_in_two_byte_string)
        ? ConcatStringContent<uint8_t>(
            NewRawOneByteString(length).ToHandleChecked(), left, right)
        : ConcatStringContent<uc16>(
            NewRawTwoByteString(length).ToHandleChecked(), left, right);
  }

  Handle<ConsString> result =
      (is_one_byte || is_one_byte_data_in_two_byte_string)
          ? New<ConsString>(cons_one_byte_string_map(), NEW_SPACE)
          : New<ConsString>(cons_string_map(), NEW_SPACE);

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  result->set_hash_field(String::kEmptyHashField);
  result->set_length(length);
  result->set_first(*left, mode);
  result->set_second(*right, mode);
  return result;
}

Handle<String> Factory::NewSurrogatePairString(uint16_t lead, uint16_t trail) {
  DCHECK_GE(lead, 0xD800);
  DCHECK_LE(lead, 0xDBFF);
  DCHECK_GE(trail, 0xDC00);
  DCHECK_LE(trail, 0xDFFF);

  Handle<SeqTwoByteString> str =
      isolate()->factory()->NewRawTwoByteString(2).ToHandleChecked();
  uc16* dest = str->GetChars();
  dest[0] = lead;
  dest[1] = trail;
  return str;
}

Handle<String> Factory::NewProperSubString(Handle<String> str,
                                           int begin,
                                           int end) {
#if VERIFY_HEAP
  if (FLAG_verify_heap) str->StringVerify();
#endif
  DCHECK(begin > 0 || end < str->length());

  str = String::Flatten(str);

  int length = end - begin;
  if (length <= 0) return empty_string();
  if (length == 1) {
    return LookupSingleCharacterStringFromCode(str->Get(begin));
  }
  if (length == 2) {
    // Optimization for 2-byte strings often used as keys in a decompression
    // dictionary.  Check whether we already have the string in the string
    // table to prevent creation of many unnecessary strings.
    uint16_t c1 = str->Get(begin);
    uint16_t c2 = str->Get(begin + 1);
    return MakeOrFindTwoCharacterString(isolate(), c1, c2);
  }

  if (!FLAG_string_slices || length < SlicedString::kMinLength) {
    if (str->IsOneByteRepresentation()) {
      Handle<SeqOneByteString> result =
          NewRawOneByteString(length).ToHandleChecked();
      uint8_t* dest = result->GetChars();
      DisallowHeapAllocation no_gc;
      String::WriteToFlat(*str, dest, begin, end);
      return result;
    } else {
      Handle<SeqTwoByteString> result =
          NewRawTwoByteString(length).ToHandleChecked();
      uc16* dest = result->GetChars();
      DisallowHeapAllocation no_gc;
      String::WriteToFlat(*str, dest, begin, end);
      return result;
    }
  }

  int offset = begin;

  if (str->IsSlicedString()) {
    Handle<SlicedString> slice = Handle<SlicedString>::cast(str);
    str = Handle<String>(slice->parent(), isolate());
    offset += slice->offset();
  }

  DCHECK(str->IsSeqString() || str->IsExternalString());
  Handle<Map> map = str->IsOneByteRepresentation()
                        ? sliced_one_byte_string_map()
                        : sliced_string_map();
  Handle<SlicedString> slice = New<SlicedString>(map, NEW_SPACE);

  slice->set_hash_field(String::kEmptyHashField);
  slice->set_length(length);
  slice->set_parent(*str);
  slice->set_offset(offset);
  return slice;
}


MaybeHandle<String> Factory::NewExternalStringFromOneByte(
    const ExternalOneByteString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }

  Handle<Map> map;
  if (resource->IsCompressible()) {
    // TODO(hajimehoshi): Rename this to 'uncached_external_one_byte_string_map'
    map = short_external_one_byte_string_map();
  } else {
    map = external_one_byte_string_map();
  }
  Handle<ExternalOneByteString> external_string =
      New<ExternalOneByteString>(map, NEW_SPACE);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return external_string;
}


MaybeHandle<String> Factory::NewExternalStringFromTwoByte(
    const ExternalTwoByteString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    THROW_NEW_ERROR(isolate(), NewInvalidStringLengthError(), String);
  }

  // For small strings we check whether the resource contains only
  // one byte characters.  If yes, we use a different string map.
  static const size_t kOneByteCheckLengthLimit = 32;
  bool is_one_byte = length <= kOneByteCheckLengthLimit &&
      String::IsOneByte(resource->data(), static_cast<int>(length));
  Handle<Map> map;
  if (resource->IsCompressible()) {
    // TODO(hajimehoshi): Rename these to 'uncached_external_string_...'.
    map = is_one_byte ? short_external_string_with_one_byte_data_map()
                      : short_external_string_map();
  } else {
    map = is_one_byte ? external_string_with_one_byte_data_map()
                      : external_string_map();
  }
  Handle<ExternalTwoByteString> external_string =
      New<ExternalTwoByteString>(map, NEW_SPACE);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return external_string;
}

Handle<ExternalOneByteString> Factory::NewNativeSourceString(
    const ExternalOneByteString::Resource* resource) {
  size_t length = resource->length();
  DCHECK_LE(length, static_cast<size_t>(String::kMaxLength));

  Handle<Map> map = native_source_string_map();
  Handle<ExternalOneByteString> external_string =
      New<ExternalOneByteString>(map, OLD_SPACE);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return external_string;
}

Handle<JSStringIterator> Factory::NewJSStringIterator(Handle<String> string) {
  Handle<Map> map(isolate()->native_context()->string_iterator_map(),
                  isolate());
  Handle<String> flat_string = String::Flatten(string);
  Handle<JSStringIterator> iterator =
      Handle<JSStringIterator>::cast(NewJSObjectFromMap(map));
  iterator->set_string(*flat_string);
  iterator->set_index(0);

  return iterator;
}

Handle<Symbol> Factory::NewSymbol() {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateSymbol(),
      Symbol);
}


Handle<Symbol> Factory::NewPrivateSymbol() {
  Handle<Symbol> symbol = NewSymbol();
  symbol->set_is_private(true);
  return symbol;
}


Handle<Context> Factory::NewNativeContext() {
  Handle<FixedArray> array =
      NewFixedArray(Context::NATIVE_CONTEXT_SLOTS, TENURED);
  array->set_map_no_write_barrier(*native_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_native_context(*context);
  context->set_errors_thrown(Smi::kZero);
  context->set_math_random_index(Smi::kZero);
  Handle<WeakCell> weak_cell = NewWeakCell(context);
  context->set_self_weak_cell(*weak_cell);
  DCHECK(context->IsNativeContext());
  return context;
}


Handle<Context> Factory::NewScriptContext(Handle<JSFunction> function,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), SCRIPT_SCOPE);
  Handle<FixedArray> array =
      NewFixedArray(scope_info->ContextLength(), TENURED);
  array->set_map_no_write_barrier(*script_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(function->context());
  context->set_extension(*scope_info);
  context->set_native_context(function->native_context());
  DCHECK(context->IsScriptContext());
  return context;
}


Handle<ScriptContextTable> Factory::NewScriptContextTable() {
  Handle<FixedArray> array = NewFixedArray(1);
  array->set_map_no_write_barrier(*script_context_table_map());
  Handle<ScriptContextTable> context_table =
      Handle<ScriptContextTable>::cast(array);
  context_table->set_used(0);
  return context_table;
}

Handle<Context> Factory::NewModuleContext(Handle<Module> module,
                                          Handle<JSFunction> function,
                                          Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), MODULE_SCOPE);
  Handle<FixedArray> array =
      NewFixedArray(scope_info->ContextLength(), TENURED);
  array->set_map_no_write_barrier(*module_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(function->context());
  context->set_extension(*module);
  context->set_native_context(function->native_context());
  DCHECK(context->IsModuleContext());
  return context;
}

Handle<Context> Factory::NewFunctionContext(int length,
                                            Handle<JSFunction> function,
                                            ScopeType scope_type) {
  DCHECK(function->shared()->scope_info()->scope_type() == scope_type);
  DCHECK(length >= Context::MIN_CONTEXT_SLOTS);
  Handle<FixedArray> array = NewFixedArray(length);
  Handle<Map> map;
  switch (scope_type) {
    case EVAL_SCOPE:
      map = eval_context_map();
      break;
    case FUNCTION_SCOPE:
      map = function_context_map();
      break;
    default:
      UNREACHABLE();
  }
  array->set_map_no_write_barrier(*map);
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(function->context());
  context->set_extension(*the_hole_value());
  context->set_native_context(function->native_context());
  return context;
}

Handle<Context> Factory::NewCatchContext(Handle<JSFunction> function,
                                         Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info,
                                         Handle<String> name,
                                         Handle<Object> thrown_object) {
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == Context::THROWN_OBJECT_INDEX);
  Handle<ContextExtension> extension = NewContextExtension(scope_info, name);
  Handle<FixedArray> array = NewFixedArray(Context::MIN_CONTEXT_SLOTS + 1);
  array->set_map_no_write_barrier(*catch_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(*previous);
  context->set_extension(*extension);
  context->set_native_context(previous->native_context());
  context->set(Context::THROWN_OBJECT_INDEX, *thrown_object);
  return context;
}

Handle<Context> Factory::NewDebugEvaluateContext(Handle<Context> previous,
                                                 Handle<ScopeInfo> scope_info,
                                                 Handle<JSReceiver> extension,
                                                 Handle<Context> wrapped,
                                                 Handle<StringSet> whitelist) {
  STATIC_ASSERT(Context::WHITE_LIST_INDEX == Context::MIN_CONTEXT_SLOTS + 1);
  DCHECK(scope_info->IsDebugEvaluateScope());
  Handle<ContextExtension> context_extension = NewContextExtension(
      scope_info, extension.is_null() ? Handle<Object>::cast(undefined_value())
                                      : Handle<Object>::cast(extension));
  Handle<FixedArray> array = NewFixedArray(Context::MIN_CONTEXT_SLOTS + 2);
  array->set_map_no_write_barrier(*debug_evaluate_context_map());
  Handle<Context> c = Handle<Context>::cast(array);
  c->set_closure(wrapped.is_null() ? previous->closure() : wrapped->closure());
  c->set_previous(*previous);
  c->set_native_context(previous->native_context());
  c->set_extension(*context_extension);
  if (!wrapped.is_null()) c->set(Context::WRAPPED_CONTEXT_INDEX, *wrapped);
  if (!whitelist.is_null()) c->set(Context::WHITE_LIST_INDEX, *whitelist);
  return c;
}

Handle<Context> Factory::NewWithContext(Handle<JSFunction> function,
                                        Handle<Context> previous,
                                        Handle<ScopeInfo> scope_info,
                                        Handle<JSReceiver> extension) {
  Handle<ContextExtension> context_extension =
      NewContextExtension(scope_info, extension);
  Handle<FixedArray> array = NewFixedArray(Context::MIN_CONTEXT_SLOTS);
  array->set_map_no_write_barrier(*with_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(*previous);
  context->set_extension(*context_extension);
  context->set_native_context(previous->native_context());
  return context;
}


Handle<Context> Factory::NewBlockContext(Handle<JSFunction> function,
                                         Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info) {
  DCHECK_EQ(scope_info->scope_type(), BLOCK_SCOPE);
  Handle<FixedArray> array = NewFixedArray(scope_info->ContextLength());
  array->set_map_no_write_barrier(*block_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(*previous);
  context->set_extension(*scope_info);
  context->set_native_context(previous->native_context());
  return context;
}

Handle<Struct> Factory::NewStruct(InstanceType type) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateStruct(type),
      Struct);
}

Handle<AliasedArgumentsEntry> Factory::NewAliasedArgumentsEntry(
    int aliased_context_slot) {
  Handle<AliasedArgumentsEntry> entry = Handle<AliasedArgumentsEntry>::cast(
      NewStruct(ALIASED_ARGUMENTS_ENTRY_TYPE));
  entry->set_aliased_context_slot(aliased_context_slot);
  return entry;
}


Handle<AccessorInfo> Factory::NewAccessorInfo() {
  Handle<AccessorInfo> info =
      Handle<AccessorInfo>::cast(NewStruct(ACCESSOR_INFO_TYPE));
  info->set_flag(0);  // Must clear the flag, it was initialized as undefined.
  info->set_is_sloppy(true);
  return info;
}


Handle<Script> Factory::NewScript(Handle<String> source) {
  // Create and initialize script object.
  Heap* heap = isolate()->heap();
  Handle<Script> script = Handle<Script>::cast(NewStruct(SCRIPT_TYPE));
  script->set_source(*source);
  script->set_name(heap->undefined_value());
  script->set_id(isolate()->heap()->NextScriptId());
  script->set_line_offset(0);
  script->set_column_offset(0);
  script->set_context_data(heap->undefined_value());
  script->set_type(Script::TYPE_NORMAL);
  script->set_wrapper(heap->undefined_value());
  script->set_line_ends(heap->undefined_value());
  script->set_eval_from_shared(heap->undefined_value());
  script->set_eval_from_position(0);
  script->set_shared_function_infos(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  script->set_flags(0);

  heap->set_script_list(*WeakFixedArray::Add(script_list(), script));
  return script;
}


Handle<Foreign> Factory::NewForeign(Address addr, PretenureFlag pretenure) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->AllocateForeign(addr, pretenure),
                     Foreign);
}


Handle<Foreign> Factory::NewForeign(const AccessorDescriptor* desc) {
  return NewForeign((Address) desc, TENURED);
}


Handle<ByteArray> Factory::NewByteArray(int length, PretenureFlag pretenure) {
  DCHECK(0 <= length);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateByteArray(length, pretenure),
      ByteArray);
}


Handle<BytecodeArray> Factory::NewBytecodeArray(
    int length, const byte* raw_bytecodes, int frame_size, int parameter_count,
    Handle<FixedArray> constant_pool) {
  DCHECK(0 <= length);
  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->AllocateBytecodeArray(
                                    length, raw_bytecodes, frame_size,
                                    parameter_count, *constant_pool),
                     BytecodeArray);
}


Handle<FixedTypedArrayBase> Factory::NewFixedTypedArrayWithExternalPointer(
    int length, ExternalArrayType array_type, void* external_pointer,
    PretenureFlag pretenure) {
  DCHECK(0 <= length && length <= Smi::kMaxValue);
  CALL_HEAP_FUNCTION(
      isolate(), isolate()->heap()->AllocateFixedTypedArrayWithExternalPointer(
                     length, array_type, external_pointer, pretenure),
      FixedTypedArrayBase);
}


Handle<FixedTypedArrayBase> Factory::NewFixedTypedArray(
    int length, ExternalArrayType array_type, bool initialize,
    PretenureFlag pretenure) {
  DCHECK(0 <= length && length <= Smi::kMaxValue);
  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->AllocateFixedTypedArray(
                                    length, array_type, initialize, pretenure),
                     FixedTypedArrayBase);
}


Handle<Cell> Factory::NewCell(Handle<Object> value) {
  AllowDeferredHandleDereference convert_to_cell;
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateCell(*value),
      Cell);
}


Handle<PropertyCell> Factory::NewPropertyCell() {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocatePropertyCell(),
      PropertyCell);
}


Handle<WeakCell> Factory::NewWeakCell(Handle<HeapObject> value) {
  // It is safe to dereference the value because we are embedding it
  // in cell and not inspecting its fields.
  AllowDeferredHandleDereference convert_to_cell;
  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->AllocateWeakCell(*value),
                     WeakCell);
}


Handle<TransitionArray> Factory::NewTransitionArray(int capacity) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->AllocateTransitionArray(capacity),
                     TransitionArray);
}


Handle<AllocationSite> Factory::NewAllocationSite() {
  Handle<Map> map = allocation_site_map();
  Handle<AllocationSite> site = New<AllocationSite>(map, OLD_SPACE);
  site->Initialize();

  // Link the site
  site->set_weak_next(isolate()->heap()->allocation_sites_list());
  isolate()->heap()->set_allocation_sites_list(*site);
  return site;
}


Handle<Map> Factory::NewMap(InstanceType type,
                            int instance_size,
                            ElementsKind elements_kind) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateMap(type, instance_size, elements_kind),
      Map);
}


Handle<JSObject> Factory::CopyJSObject(Handle<JSObject> object) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyJSObject(*object, NULL),
                     JSObject);
}


Handle<JSObject> Factory::CopyJSObjectWithAllocationSite(
    Handle<JSObject> object,
    Handle<AllocationSite> site) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyJSObject(
                         *object,
                         site.is_null() ? NULL : *site),
                     JSObject);
}


Handle<FixedArray> Factory::CopyFixedArrayWithMap(Handle<FixedArray> array,
                                                  Handle<Map> map) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyFixedArrayWithMap(*array, *map),
                     FixedArray);
}


Handle<FixedArray> Factory::CopyFixedArrayAndGrow(Handle<FixedArray> array,
                                                  int grow_by,
                                                  PretenureFlag pretenure) {
  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->CopyFixedArrayAndGrow(
                                    *array, grow_by, pretenure),
                     FixedArray);
}

Handle<FixedArray> Factory::CopyFixedArrayUpTo(Handle<FixedArray> array,
                                               int new_len,
                                               PretenureFlag pretenure) {
  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->CopyFixedArrayUpTo(
                                    *array, new_len, pretenure),
                     FixedArray);
}

Handle<FixedArray> Factory::CopyFixedArray(Handle<FixedArray> array) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyFixedArray(*array),
                     FixedArray);
}


Handle<FixedArray> Factory::CopyAndTenureFixedCOWArray(
    Handle<FixedArray> array) {
  DCHECK(isolate()->heap()->InNewSpace(*array));
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyAndTenureFixedCOWArray(*array),
                     FixedArray);
}


Handle<FixedDoubleArray> Factory::CopyFixedDoubleArray(
    Handle<FixedDoubleArray> array) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyFixedDoubleArray(*array),
                     FixedDoubleArray);
}


Handle<Object> Factory::NewNumber(double value,
                                  PretenureFlag pretenure) {
  // Materialize as a SMI if possible
  int32_t int_value;
  if (DoubleToSmiInteger(value, &int_value)) {
    return handle(Smi::FromInt(int_value), isolate());
  }

  // Materialize the value in the heap.
  return NewHeapNumber(value, IMMUTABLE, pretenure);
}


Handle<Object> Factory::NewNumberFromInt(int32_t value,
                                         PretenureFlag pretenure) {
  if (Smi::IsValid(value)) return handle(Smi::FromInt(value), isolate());
  // Bypass NewNumber to avoid various redundant checks.
  return NewHeapNumber(FastI2D(value), IMMUTABLE, pretenure);
}


Handle<Object> Factory::NewNumberFromUint(uint32_t value,
                                          PretenureFlag pretenure) {
  int32_t int32v = static_cast<int32_t>(value);
  if (int32v >= 0 && Smi::IsValid(int32v)) {
    return handle(Smi::FromInt(int32v), isolate());
  }
  return NewHeapNumber(FastUI2D(value), IMMUTABLE, pretenure);
}


Handle<HeapNumber> Factory::NewHeapNumber(double value,
                                          MutableMode mode,
                                          PretenureFlag pretenure) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateHeapNumber(value, mode, pretenure),
      HeapNumber);
}


#define SIMD128_NEW_DEF(TYPE, Type, type, lane_count, lane_type)               \
  Handle<Type> Factory::New##Type(lane_type lanes[lane_count],                 \
                                  PretenureFlag pretenure) {                   \
    CALL_HEAP_FUNCTION(                                                        \
        isolate(), isolate()->heap()->Allocate##Type(lanes, pretenure), Type); \
  }
SIMD128_TYPES(SIMD128_NEW_DEF)
#undef SIMD128_NEW_DEF


Handle<Object> Factory::NewError(Handle<JSFunction> constructor,
                                 MessageTemplate::Template template_index,
                                 Handle<Object> arg0, Handle<Object> arg1,
                                 Handle<Object> arg2) {
  HandleScope scope(isolate());
  if (isolate()->bootstrapper()->IsActive()) {
    // During bootstrapping we cannot construct error objects.
    return scope.CloseAndEscape(NewStringFromAsciiChecked(
        MessageTemplate::TemplateString(template_index)));
  }

  if (arg0.is_null()) arg0 = undefined_value();
  if (arg1.is_null()) arg1 = undefined_value();
  if (arg2.is_null()) arg2 = undefined_value();

  Handle<Object> result;
  if (!ErrorUtils::MakeGenericError(isolate(), constructor, template_index,
                                    arg0, arg1, arg2, SKIP_NONE)
           .ToHandle(&result)) {
    // If an exception is thrown while
    // running the factory method, use the exception as the result.
    DCHECK(isolate()->has_pending_exception());
    result = handle(isolate()->pending_exception(), isolate());
    isolate()->clear_pending_exception();
  }

  return scope.CloseAndEscape(result);
}


Handle<Object> Factory::NewError(Handle<JSFunction> constructor,
                                 Handle<String> message) {
  // Construct a new error object. If an exception is thrown, use the exception
  // as the result.

  Handle<Object> no_caller;
  MaybeHandle<Object> maybe_error =
      ErrorUtils::Construct(isolate(), constructor, constructor, message,
                            SKIP_NONE, no_caller, false);
  if (maybe_error.is_null()) {
    DCHECK(isolate()->has_pending_exception());
    maybe_error = handle(isolate()->pending_exception(), isolate());
    isolate()->clear_pending_exception();
  }

  return maybe_error.ToHandleChecked();
}

Handle<Object> Factory::NewInvalidStringLengthError() {
  // Invalidate the "string length" protector.
  if (isolate()->IsStringLengthOverflowIntact()) {
    isolate()->InvalidateStringLengthOverflowProtector();
  }
  return NewRangeError(MessageTemplate::kInvalidStringLength);
}

#define DEFINE_ERROR(NAME, name)                                              \
  Handle<Object> Factory::New##NAME(MessageTemplate::Template template_index, \
                                    Handle<Object> arg0, Handle<Object> arg1, \
                                    Handle<Object> arg2) {                    \
    return NewError(isolate()->name##_function(), template_index, arg0, arg1, \
                    arg2);                                                    \
  }
DEFINE_ERROR(Error, error)
DEFINE_ERROR(EvalError, eval_error)
DEFINE_ERROR(RangeError, range_error)
DEFINE_ERROR(ReferenceError, reference_error)
DEFINE_ERROR(SyntaxError, syntax_error)
DEFINE_ERROR(TypeError, type_error)
DEFINE_ERROR(WasmCompileError, wasm_compile_error)
DEFINE_ERROR(WasmLinkError, wasm_link_error)
DEFINE_ERROR(WasmRuntimeError, wasm_runtime_error)
#undef DEFINE_ERROR

Handle<JSFunction> Factory::NewFunction(Handle<Map> map,
                                        Handle<SharedFunctionInfo> info,
                                        Handle<Object> context_or_undefined,
                                        PretenureFlag pretenure) {
  AllocationSpace space = pretenure == TENURED ? OLD_SPACE : NEW_SPACE;
  Handle<JSFunction> function = New<JSFunction>(map, space);
  DCHECK(context_or_undefined->IsContext() ||
         context_or_undefined->IsUndefined(isolate()));

  function->initialize_properties();
  function->initialize_elements();
  function->set_shared(*info);
  function->set_code(info->code());
  function->set_context(*context_or_undefined);
  function->set_prototype_or_initial_map(*the_hole_value());
  function->set_literals(LiteralsArray::cast(*empty_literals_array()));
  function->set_next_function_link(*undefined_value(), SKIP_WRITE_BARRIER);
  isolate()->heap()->InitializeJSObjectBody(*function, *map, JSFunction::kSize);
  return function;
}


Handle<JSFunction> Factory::NewFunction(Handle<Map> map,
                                        Handle<String> name,
                                        MaybeHandle<Code> code) {
  Handle<Context> context(isolate()->native_context());
  Handle<SharedFunctionInfo> info =
      NewSharedFunctionInfo(name, code, map->is_constructor());
  DCHECK(is_sloppy(info->language_mode()));
  DCHECK(!map->IsUndefined(isolate()));
  DCHECK(
      map.is_identical_to(isolate()->sloppy_function_map()) ||
      map.is_identical_to(isolate()->sloppy_function_without_prototype_map()) ||
      map.is_identical_to(
          isolate()->sloppy_function_with_readonly_prototype_map()) ||
      map.is_identical_to(isolate()->strict_function_map()) ||
      map.is_identical_to(isolate()->strict_function_without_prototype_map()) ||
      // TODO(titzer): wasm_function_map() could be undefined here. ugly.
      (*map == context->get(Context::WASM_FUNCTION_MAP_INDEX)) ||
      map.is_identical_to(isolate()->proxy_function_map()));
  return NewFunction(map, info, context);
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name) {
  return NewFunction(
      isolate()->sloppy_function_map(), name, MaybeHandle<Code>());
}


Handle<JSFunction> Factory::NewFunctionWithoutPrototype(Handle<String> name,
                                                        Handle<Code> code,
                                                        bool is_strict) {
  Handle<Map> map = is_strict
                        ? isolate()->strict_function_without_prototype_map()
                        : isolate()->sloppy_function_without_prototype_map();
  return NewFunction(map, name, code);
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name, Handle<Code> code,
                                        Handle<Object> prototype,
                                        bool is_strict) {
  Handle<Map> map = is_strict ? isolate()->strict_function_map()
                              : isolate()->sloppy_function_map();
  Handle<JSFunction> result = NewFunction(map, name, code);
  result->set_prototype_or_initial_map(*prototype);
  return result;
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name, Handle<Code> code,
                                        Handle<Object> prototype,
                                        InstanceType type, int instance_size,
                                        bool is_strict) {
  // Allocate the function
  Handle<JSFunction> function = NewFunction(name, code, prototype, is_strict);

  ElementsKind elements_kind =
      type == JS_ARRAY_TYPE ? FAST_SMI_ELEMENTS : FAST_HOLEY_SMI_ELEMENTS;
  Handle<Map> initial_map = NewMap(type, instance_size, elements_kind);
  // TODO(littledan): Why do we have this is_generator test when
  // NewFunctionPrototype already handles finding an appropriately
  // shared prototype?
  if (!IsResumableFunction(function->shared()->kind())) {
    if (prototype->IsTheHole(isolate())) {
      prototype = NewFunctionPrototype(function);
    }
  }

  JSFunction::SetInitialMap(function, initial_map,
                            Handle<JSReceiver>::cast(prototype));

  return function;
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name,
                                        Handle<Code> code,
                                        InstanceType type,
                                        int instance_size) {
  return NewFunction(name, code, the_hole_value(), type, instance_size);
}


Handle<JSObject> Factory::NewFunctionPrototype(Handle<JSFunction> function) {
  // Make sure to use globals from the function's context, since the function
  // can be from a different context.
  Handle<Context> native_context(function->context()->native_context());
  Handle<Map> new_map;
  if (IsResumableFunction(function->shared()->kind())) {
    // Generator and async function prototypes can share maps since they
    // don't have "constructor" properties.
    new_map = handle(native_context->generator_object_prototype_map());
  } else {
    // Each function prototype gets a fresh map to avoid unwanted sharing of
    // maps between prototypes of different constructors.
    Handle<JSFunction> object_function(native_context->object_function());
    DCHECK(object_function->has_initial_map());
    new_map = handle(object_function->initial_map());
  }

  DCHECK(!new_map->is_prototype_map());
  Handle<JSObject> prototype = NewJSObjectFromMap(new_map);

  if (!IsResumableFunction(function->shared()->kind())) {
    JSObject::AddProperty(prototype, constructor_string(), function, DONT_ENUM);
  }

  return prototype;
}


Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info,
    Handle<Context> context,
    PretenureFlag pretenure) {
  int map_index =
      Context::FunctionMapIndex(info->language_mode(), info->kind());
  Handle<Map> initial_map(Map::cast(context->native_context()->get(map_index)));

  return NewFunctionFromSharedFunctionInfo(initial_map, info, context,
                                           pretenure);
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info, Handle<Context> context,
    Handle<LiteralsArray> literals, PretenureFlag pretenure) {
  int map_index =
      Context::FunctionMapIndex(info->language_mode(), info->kind());
  Handle<Map> initial_map(Map::cast(context->native_context()->get(map_index)));

  return NewFunctionFromSharedFunctionInfo(initial_map, info, context, literals,
                                           pretenure);
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<Map> initial_map, Handle<SharedFunctionInfo> info,
    Handle<Object> context_or_undefined, PretenureFlag pretenure) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  Handle<JSFunction> result =
      NewFunction(initial_map, info, context_or_undefined, pretenure);

  if (info->ic_age() != isolate()->heap()->global_ic_age()) {
    info->ResetForNewContext(isolate()->heap()->global_ic_age());
  }

  if (context_or_undefined->IsContext()) {
    // Give compiler a chance to pre-initialize.
    Compiler::PostInstantiation(result, pretenure);
  }

  return result;
}

Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<Map> initial_map, Handle<SharedFunctionInfo> info,
    Handle<Object> context_or_undefined, Handle<LiteralsArray> literals,
    PretenureFlag pretenure) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  Handle<JSFunction> result =
      NewFunction(initial_map, info, context_or_undefined, pretenure);

  result->set_literals(*literals);
  if (info->ic_age() != isolate()->heap()->global_ic_age()) {
    info->ResetForNewContext(isolate()->heap()->global_ic_age());
  }

  if (context_or_undefined->IsContext()) {
    // Give compiler a chance to pre-initialize.
    Compiler::PostInstantiation(result, pretenure);
  }

  return result;
}

Handle<ScopeInfo> Factory::NewScopeInfo(int length) {
  Handle<FixedArray> array = NewFixedArray(length, TENURED);
  array->set_map_no_write_barrier(*scope_info_map());
  Handle<ScopeInfo> scope_info = Handle<ScopeInfo>::cast(array);
  return scope_info;
}

Handle<ModuleInfo> Factory::NewModuleInfo() {
  Handle<FixedArray> array = NewFixedArray(ModuleInfo::kLength, TENURED);
  array->set_map_no_write_barrier(*module_info_map());
  return Handle<ModuleInfo>::cast(array);
}

Handle<JSObject> Factory::NewExternal(void* value) {
  Handle<Foreign> foreign = NewForeign(static_cast<Address>(value));
  Handle<JSObject> external = NewJSObjectFromMap(external_map());
  external->SetInternalField(0, *foreign);
  return external;
}


Handle<Code> Factory::NewCodeRaw(int object_size, bool immovable) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->AllocateCode(object_size, immovable),
                     Code);
}


Handle<Code> Factory::NewCode(const CodeDesc& desc,
                              Code::Flags flags,
                              Handle<Object> self_ref,
                              bool immovable,
                              bool crankshafted,
                              int prologue_offset,
                              bool is_debug) {
  Handle<ByteArray> reloc_info = NewByteArray(desc.reloc_size, TENURED);

  bool has_unwinding_info = desc.unwinding_info != nullptr;
  DCHECK((has_unwinding_info && desc.unwinding_info_size > 0) ||
         (!has_unwinding_info && desc.unwinding_info_size == 0));

  // Compute size.
  int body_size = desc.instr_size;
  int unwinding_info_size_field_size = kInt64Size;
  if (has_unwinding_info) {
    body_size = RoundUp(body_size, kInt64Size) + desc.unwinding_info_size +
                unwinding_info_size_field_size;
  }
  int obj_size = Code::SizeFor(RoundUp(body_size, kObjectAlignment));

  Handle<Code> code = NewCodeRaw(obj_size, immovable);
  DCHECK(!isolate()->heap()->memory_allocator()->code_range()->valid() ||
         isolate()->heap()->memory_allocator()->code_range()->contains(
             code->address()) ||
         obj_size <= isolate()->heap()->code_space()->AreaSize());

  // The code object has not been fully initialized yet.  We rely on the
  // fact that no allocation will happen from this point on.
  DisallowHeapAllocation no_gc;
  code->set_gc_metadata(Smi::kZero);
  code->set_ic_age(isolate()->heap()->global_ic_age());
  code->set_instruction_size(desc.instr_size);
  code->set_relocation_info(*reloc_info);
  code->set_flags(flags);
  code->set_has_unwinding_info(has_unwinding_info);
  code->set_raw_kind_specific_flags1(0);
  code->set_raw_kind_specific_flags2(0);
  code->set_is_crankshafted(crankshafted);
  code->set_deoptimization_data(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  code->set_raw_type_feedback_info(Smi::kZero);
  code->set_next_code_link(*undefined_value(), SKIP_WRITE_BARRIER);
  code->set_handler_table(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  code->set_source_position_table(*empty_byte_array(), SKIP_WRITE_BARRIER);
  code->set_prologue_offset(prologue_offset);
  code->set_constant_pool_offset(desc.instr_size - desc.constant_pool_size);
  code->set_builtin_index(-1);
  code->set_protected_instructions(*empty_fixed_array());

  if (code->kind() == Code::OPTIMIZED_FUNCTION) {
    code->set_marked_for_deoptimization(false);
  }

  if (is_debug) {
    DCHECK(code->kind() == Code::FUNCTION);
    code->set_has_debug_break_slots(true);
  }

  // Allow self references to created code object by patching the handle to
  // point to the newly allocated Code object.
  if (!self_ref.is_null()) *(self_ref.location()) = *code;

  // Migrate generated code.
  // The generated code can contain Object** values (typically from handles)
  // that are dereferenced during the copy to point directly to the actual heap
  // objects. These pointers can include references to the code object itself,
  // through the self_reference parameter.
  code->CopyFrom(desc);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) code->ObjectVerify();
#endif
  return code;
}


Handle<Code> Factory::CopyCode(Handle<Code> code) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyCode(*code),
                     Code);
}


Handle<BytecodeArray> Factory::CopyBytecodeArray(
    Handle<BytecodeArray> bytecode_array) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyBytecodeArray(*bytecode_array),
                     BytecodeArray);
}

Handle<JSObject> Factory::NewJSObject(Handle<JSFunction> constructor,
                                      PretenureFlag pretenure) {
  JSFunction::EnsureHasInitialMap(constructor);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObject(*constructor, pretenure), JSObject);
}


Handle<JSObject> Factory::NewJSObjectWithNullProto() {
  Handle<JSObject> result = NewJSObject(isolate()->object_function());
  Handle<Map> new_map =
      Map::Copy(Handle<Map>(result->map()), "ObjectWithNullProto");
  Map::SetPrototype(new_map, null_value());
  JSObject::MigrateToMap(result, new_map);
  return result;
}

Handle<JSGlobalObject> Factory::NewJSGlobalObject(
    Handle<JSFunction> constructor) {
  DCHECK(constructor->has_initial_map());
  Handle<Map> map(constructor->initial_map());
  DCHECK(map->is_dictionary_map());

  // Make sure no field properties are described in the initial map.
  // This guarantees us that normalizing the properties does not
  // require us to change property values to PropertyCells.
  DCHECK(map->NextFreePropertyIndex() == 0);

  // Make sure we don't have a ton of pre-allocated slots in the
  // global objects. They will be unused once we normalize the object.
  DCHECK(map->unused_property_fields() == 0);
  DCHECK(map->GetInObjectProperties() == 0);

  // Initial size of the backing store to avoid resize of the storage during
  // bootstrapping. The size differs between the JS global object ad the
  // builtins object.
  int initial_size = 64;

  // Allocate a dictionary object for backing storage.
  int at_least_space_for = map->NumberOfOwnDescriptors() * 2 + initial_size;
  Handle<GlobalDictionary> dictionary =
      GlobalDictionary::New(isolate(), at_least_space_for);

  // The global object might be created from an object template with accessors.
  // Fill these accessors into the dictionary.
  Handle<DescriptorArray> descs(map->instance_descriptors());
  for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
    PropertyDetails details = descs->GetDetails(i);
    // Only accessors are expected.
    DCHECK_EQ(kAccessor, details.kind());
    PropertyDetails d(kAccessor, details.attributes(), i + 1,
                      PropertyCellType::kMutable);
    Handle<Name> name(descs->GetKey(i));
    Handle<PropertyCell> cell = NewPropertyCell();
    cell->set_value(descs->GetValue(i));
    // |dictionary| already contains enough space for all properties.
    USE(GlobalDictionary::Add(dictionary, name, cell, d));
  }

  // Allocate the global object and initialize it with the backing store.
  Handle<JSGlobalObject> global = New<JSGlobalObject>(map, OLD_SPACE);
  isolate()->heap()->InitializeJSObjectFromMap(*global, *dictionary, *map);

  // Create a new map for the global object.
  Handle<Map> new_map = Map::CopyDropDescriptors(map);
  new_map->set_dictionary_map(true);

  // Set up the global object as a normalized object.
  global->set_map(*new_map);
  global->set_properties(*dictionary);

  // Make sure result is a global object with properties in dictionary.
  DCHECK(global->IsJSGlobalObject() && !global->HasFastProperties());
  return global;
}


Handle<JSObject> Factory::NewJSObjectFromMap(
    Handle<Map> map,
    PretenureFlag pretenure,
    Handle<AllocationSite> allocation_site) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObjectFromMap(
          *map,
          pretenure,
          allocation_site.is_null() ? NULL : *allocation_site),
      JSObject);
}


Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind,
                                    PretenureFlag pretenure) {
  Map* map = isolate()->get_initial_js_array_map(elements_kind);
  if (map == nullptr) {
    Context* native_context = isolate()->context()->native_context();
    JSFunction* array_function = native_context->array_function();
    map = array_function->initial_map();
  }
  return Handle<JSArray>::cast(NewJSObjectFromMap(handle(map), pretenure));
}

Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind, int length,
                                    int capacity,
                                    ArrayStorageAllocationMode mode,
                                    PretenureFlag pretenure) {
  Handle<JSArray> array = NewJSArray(elements_kind, pretenure);
  NewJSArrayStorage(array, length, capacity, mode);
  return array;
}

Handle<JSArray> Factory::NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                                ElementsKind elements_kind,
                                                int length,
                                                PretenureFlag pretenure) {
  DCHECK(length <= elements->length());
  Handle<JSArray> array = NewJSArray(elements_kind, pretenure);

  array->set_elements(*elements);
  array->set_length(Smi::FromInt(length));
  JSObject::ValidateElements(array);
  return array;
}


void Factory::NewJSArrayStorage(Handle<JSArray> array,
                                int length,
                                int capacity,
                                ArrayStorageAllocationMode mode) {
  DCHECK(capacity >= length);

  if (capacity == 0) {
    array->set_length(Smi::kZero);
    array->set_elements(*empty_fixed_array());
    return;
  }

  HandleScope inner_scope(isolate());
  Handle<FixedArrayBase> elms;
  ElementsKind elements_kind = array->GetElementsKind();
  if (IsFastDoubleElementsKind(elements_kind)) {
    if (mode == DONT_INITIALIZE_ARRAY_ELEMENTS) {
      elms = NewFixedDoubleArray(capacity);
    } else {
      DCHECK(mode == INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
      elms = NewFixedDoubleArrayWithHoles(capacity);
    }
  } else {
    DCHECK(IsFastSmiOrObjectElementsKind(elements_kind));
    if (mode == DONT_INITIALIZE_ARRAY_ELEMENTS) {
      elms = NewUninitializedFixedArray(capacity);
    } else {
      DCHECK(mode == INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
      elms = NewFixedArrayWithHoles(capacity);
    }
  }

  array->set_elements(*elms);
  array->set_length(Smi::FromInt(length));
}

Handle<JSModuleNamespace> Factory::NewJSModuleNamespace() {
  Handle<Map> map = isolate()->js_module_namespace_map();
  Handle<JSModuleNamespace> module_namespace(
      Handle<JSModuleNamespace>::cast(NewJSObjectFromMap(map)));
  FieldIndex index = FieldIndex::ForDescriptor(
      *map, JSModuleNamespace::kToStringTagFieldIndex);
  module_namespace->FastPropertyAtPut(index,
                                      isolate()->heap()->Module_string());
  return module_namespace;
}

Handle<JSGeneratorObject> Factory::NewJSGeneratorObject(
    Handle<JSFunction> function) {
  DCHECK(IsResumableFunction(function->shared()->kind()));
  JSFunction::EnsureHasInitialMap(function);
  Handle<Map> map(function->initial_map());
  DCHECK_EQ(JS_GENERATOR_OBJECT_TYPE, map->instance_type());
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObjectFromMap(*map),
      JSGeneratorObject);
}

Handle<Module> Factory::NewModule(Handle<SharedFunctionInfo> code) {
  Handle<ModuleInfo> module_info(code->scope_info()->ModuleDescriptorInfo(),
                                 isolate());
  Handle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate(), module_info->RegularExportCount());
  Handle<FixedArray> regular_exports =
      NewFixedArray(module_info->RegularExportCount());
  Handle<FixedArray> regular_imports =
      NewFixedArray(module_info->regular_imports()->length());
  int requested_modules_length = module_info->module_requests()->length();
  Handle<FixedArray> requested_modules =
      requested_modules_length > 0 ? NewFixedArray(requested_modules_length)
                                   : empty_fixed_array();

  Handle<Module> module = Handle<Module>::cast(NewStruct(MODULE_TYPE));
  module->set_code(*code);
  module->set_exports(*exports);
  module->set_regular_exports(*regular_exports);
  module->set_regular_imports(*regular_imports);
  module->set_hash(isolate()->GenerateIdentityHash(Smi::kMaxValue));
  module->set_module_namespace(isolate()->heap()->undefined_value());
  module->set_requested_modules(*requested_modules);
  DCHECK(!module->instantiated());
  DCHECK(!module->evaluated());
  return module;
}

Handle<JSArrayBuffer> Factory::NewJSArrayBuffer(SharedFlag shared,
                                                PretenureFlag pretenure) {
  Handle<JSFunction> array_buffer_fun(
      shared == SharedFlag::kShared
          ? isolate()->native_context()->shared_array_buffer_fun()
          : isolate()->native_context()->array_buffer_fun());
  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->AllocateJSObject(
                                    *array_buffer_fun, pretenure),
                     JSArrayBuffer);
}


Handle<JSDataView> Factory::NewJSDataView() {
  Handle<JSFunction> data_view_fun(
      isolate()->native_context()->data_view_fun());
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObject(*data_view_fun),
      JSDataView);
}

Handle<JSIteratorResult> Factory::NewJSIteratorResult(Handle<Object> value,
                                                      bool done) {
  Handle<Map> map(isolate()->native_context()->iterator_result_map());
  Handle<JSIteratorResult> js_iter_result =
      Handle<JSIteratorResult>::cast(NewJSObjectFromMap(map));
  js_iter_result->set_value(*value);
  js_iter_result->set_done(*ToBoolean(done));
  return js_iter_result;
}

Handle<JSMap> Factory::NewJSMap() {
  Handle<Map> map(isolate()->native_context()->js_map_map());
  Handle<JSMap> js_map = Handle<JSMap>::cast(NewJSObjectFromMap(map));
  JSMap::Initialize(js_map, isolate());
  return js_map;
}


Handle<JSSet> Factory::NewJSSet() {
  Handle<Map> map(isolate()->native_context()->js_set_map());
  Handle<JSSet> js_set = Handle<JSSet>::cast(NewJSObjectFromMap(map));
  JSSet::Initialize(js_set, isolate());
  return js_set;
}


Handle<JSMapIterator> Factory::NewJSMapIterator() {
  Handle<Map> map(isolate()->native_context()->map_iterator_map());
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->AllocateJSObjectFromMap(*map),
                     JSMapIterator);
}


Handle<JSSetIterator> Factory::NewJSSetIterator() {
  Handle<Map> map(isolate()->native_context()->set_iterator_map());
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->AllocateJSObjectFromMap(*map),
                     JSSetIterator);
}


namespace {

ElementsKind GetExternalArrayElementsKind(ExternalArrayType type) {
  switch (type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return TYPE##_ELEMENTS;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
  }
  UNREACHABLE();
  return FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND;
#undef TYPED_ARRAY_CASE
}


size_t GetExternalArrayElementSize(ExternalArrayType type) {
  switch (type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return size;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
      return 0;
  }
#undef TYPED_ARRAY_CASE
}


size_t GetFixedTypedArraysElementSize(ElementsKind kind) {
  switch (kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                 \
    return size;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
      return 0;
  }
#undef TYPED_ARRAY_CASE
}


ExternalArrayType GetArrayTypeFromElementsKind(ElementsKind kind) {
  switch (kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                 \
    return kExternal##Type##Array;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
      return kExternalInt8Array;
  }
#undef TYPED_ARRAY_CASE
}


JSFunction* GetTypedArrayFun(ExternalArrayType type, Isolate* isolate) {
  Context* native_context = isolate->context()->native_context();
  switch (type) {
#define TYPED_ARRAY_FUN(Type, type, TYPE, ctype, size)                        \
    case kExternal##Type##Array:                                              \
      return native_context->type##_array_fun();

    TYPED_ARRAYS(TYPED_ARRAY_FUN)
#undef TYPED_ARRAY_FUN

    default:
      UNREACHABLE();
      return NULL;
  }
}


JSFunction* GetTypedArrayFun(ElementsKind elements_kind, Isolate* isolate) {
  Context* native_context = isolate->context()->native_context();
  switch (elements_kind) {
#define TYPED_ARRAY_FUN(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                \
    return native_context->type##_array_fun();

    TYPED_ARRAYS(TYPED_ARRAY_FUN)
#undef TYPED_ARRAY_FUN

    default:
      UNREACHABLE();
      return NULL;
  }
}


void SetupArrayBufferView(i::Isolate* isolate,
                          i::Handle<i::JSArrayBufferView> obj,
                          i::Handle<i::JSArrayBuffer> buffer,
                          size_t byte_offset, size_t byte_length,
                          PretenureFlag pretenure = NOT_TENURED) {
  DCHECK(byte_offset + byte_length <=
         static_cast<size_t>(buffer->byte_length()->Number()));

  DCHECK_EQ(obj->GetInternalFieldCount(),
            v8::ArrayBufferView::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    obj->SetInternalField(i, Smi::kZero);
  }

  obj->set_buffer(*buffer);

  i::Handle<i::Object> byte_offset_object =
      isolate->factory()->NewNumberFromSize(byte_offset, pretenure);
  obj->set_byte_offset(*byte_offset_object);

  i::Handle<i::Object> byte_length_object =
      isolate->factory()->NewNumberFromSize(byte_length, pretenure);
  obj->set_byte_length(*byte_length_object);
}


}  // namespace


Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type,
                                              PretenureFlag pretenure) {
  Handle<JSFunction> typed_array_fun_handle(GetTypedArrayFun(type, isolate()));

  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->AllocateJSObject(
                                    *typed_array_fun_handle, pretenure),
                     JSTypedArray);
}


Handle<JSTypedArray> Factory::NewJSTypedArray(ElementsKind elements_kind,
                                              PretenureFlag pretenure) {
  Handle<JSFunction> typed_array_fun_handle(
      GetTypedArrayFun(elements_kind, isolate()));

  CALL_HEAP_FUNCTION(isolate(), isolate()->heap()->AllocateJSObject(
                                    *typed_array_fun_handle, pretenure),
                     JSTypedArray);
}


Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type,
                                              Handle<JSArrayBuffer> buffer,
                                              size_t byte_offset, size_t length,
                                              PretenureFlag pretenure) {
  Handle<JSTypedArray> obj = NewJSTypedArray(type, pretenure);

  size_t element_size = GetExternalArrayElementSize(type);
  ElementsKind elements_kind = GetExternalArrayElementsKind(type);

  CHECK(byte_offset % element_size == 0);

  CHECK(length <= (std::numeric_limits<size_t>::max() / element_size));
  CHECK(length <= static_cast<size_t>(Smi::kMaxValue));
  size_t byte_length = length * element_size;
  SetupArrayBufferView(isolate(), obj, buffer, byte_offset, byte_length,
                       pretenure);

  Handle<Object> length_object = NewNumberFromSize(length, pretenure);
  obj->set_length(*length_object);

  Handle<FixedTypedArrayBase> elements = NewFixedTypedArrayWithExternalPointer(
      static_cast<int>(length), type,
      static_cast<uint8_t*>(buffer->backing_store()) + byte_offset, pretenure);
  Handle<Map> map = JSObject::GetElementsTransitionMap(obj, elements_kind);
  JSObject::SetMapAndElements(obj, map, elements);
  return obj;
}


Handle<JSTypedArray> Factory::NewJSTypedArray(ElementsKind elements_kind,
                                              size_t number_of_elements,
                                              PretenureFlag pretenure) {
  Handle<JSTypedArray> obj = NewJSTypedArray(elements_kind, pretenure);
  DCHECK_EQ(obj->GetInternalFieldCount(),
            v8::ArrayBufferView::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    obj->SetInternalField(i, Smi::kZero);
  }

  size_t element_size = GetFixedTypedArraysElementSize(elements_kind);
  ExternalArrayType array_type = GetArrayTypeFromElementsKind(elements_kind);

  CHECK(number_of_elements <=
        (std::numeric_limits<size_t>::max() / element_size));
  CHECK(number_of_elements <= static_cast<size_t>(Smi::kMaxValue));
  size_t byte_length = number_of_elements * element_size;

  obj->set_byte_offset(Smi::kZero);
  i::Handle<i::Object> byte_length_object =
      NewNumberFromSize(byte_length, pretenure);
  obj->set_byte_length(*byte_length_object);
  Handle<Object> length_object =
      NewNumberFromSize(number_of_elements, pretenure);
  obj->set_length(*length_object);

  Handle<JSArrayBuffer> buffer =
      NewJSArrayBuffer(SharedFlag::kNotShared, pretenure);
  JSArrayBuffer::Setup(buffer, isolate(), true, NULL, byte_length,
                       SharedFlag::kNotShared);
  obj->set_buffer(*buffer);
  Handle<FixedTypedArrayBase> elements = NewFixedTypedArray(
      static_cast<int>(number_of_elements), array_type, true, pretenure);
  obj->set_elements(*elements);
  return obj;
}


Handle<JSDataView> Factory::NewJSDataView(Handle<JSArrayBuffer> buffer,
                                          size_t byte_offset,
                                          size_t byte_length) {
  Handle<JSDataView> obj = NewJSDataView();
  SetupArrayBufferView(isolate(), obj, buffer, byte_offset, byte_length);
  return obj;
}


MaybeHandle<JSBoundFunction> Factory::NewJSBoundFunction(
    Handle<JSReceiver> target_function, Handle<Object> bound_this,
    Vector<Handle<Object>> bound_args) {
  DCHECK(target_function->IsCallable());
  STATIC_ASSERT(Code::kMaxArguments <= FixedArray::kMaxLength);
  if (bound_args.length() >= Code::kMaxArguments) {
    THROW_NEW_ERROR(isolate(),
                    NewRangeError(MessageTemplate::kTooManyArguments),
                    JSBoundFunction);
  }

  // Determine the prototype of the {target_function}.
  Handle<Object> prototype;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), prototype,
      JSReceiver::GetPrototype(isolate(), target_function), JSBoundFunction);

  // Create the [[BoundArguments]] for the result.
  Handle<FixedArray> bound_arguments;
  if (bound_args.length() == 0) {
    bound_arguments = empty_fixed_array();
  } else {
    bound_arguments = NewFixedArray(bound_args.length());
    for (int i = 0; i < bound_args.length(); ++i) {
      bound_arguments->set(i, *bound_args[i]);
    }
  }

  // Setup the map for the JSBoundFunction instance.
  Handle<Map> map = target_function->IsConstructor()
                        ? isolate()->bound_function_with_constructor_map()
                        : isolate()->bound_function_without_constructor_map();
  if (map->prototype() != *prototype) {
    map = Map::TransitionToPrototype(map, prototype, REGULAR_PROTOTYPE);
  }
  DCHECK_EQ(target_function->IsConstructor(), map->is_constructor());

  // Setup the JSBoundFunction instance.
  Handle<JSBoundFunction> result =
      Handle<JSBoundFunction>::cast(NewJSObjectFromMap(map));
  result->set_bound_target_function(*target_function);
  result->set_bound_this(*bound_this);
  result->set_bound_arguments(*bound_arguments);
  return result;
}


// ES6 section 9.5.15 ProxyCreate (target, handler)
Handle<JSProxy> Factory::NewJSProxy(Handle<JSReceiver> target,
                                    Handle<JSReceiver> handler) {
  // Allocate the proxy object.
  Handle<Map> map;
  if (target->IsCallable()) {
    if (target->IsConstructor()) {
      map = Handle<Map>(isolate()->proxy_constructor_map());
    } else {
      map = Handle<Map>(isolate()->proxy_callable_map());
    }
  } else {
    map = Handle<Map>(isolate()->proxy_map());
  }
  DCHECK(map->prototype()->IsNull(isolate()));
  Handle<JSProxy> result = New<JSProxy>(map, NEW_SPACE);
  result->initialize_properties();
  result->set_target(*target);
  result->set_handler(*handler);
  result->set_hash(*undefined_value(), SKIP_WRITE_BARRIER);
  return result;
}

Handle<JSGlobalProxy> Factory::NewUninitializedJSGlobalProxy(int size) {
  // Create an empty shell of a JSGlobalProxy that needs to be reinitialized
  // via ReinitializeJSGlobalProxy later.
  Handle<Map> map = NewMap(JS_GLOBAL_PROXY_TYPE, size);
  // Maintain invariant expected from any JSGlobalProxy.
  map->set_is_access_check_needed(true);
  CALL_HEAP_FUNCTION(
      isolate(), isolate()->heap()->AllocateJSObjectFromMap(*map, NOT_TENURED),
      JSGlobalProxy);
}


void Factory::ReinitializeJSGlobalProxy(Handle<JSGlobalProxy> object,
                                        Handle<JSFunction> constructor) {
  DCHECK(constructor->has_initial_map());
  Handle<Map> map(constructor->initial_map(), isolate());
  Handle<Map> old_map(object->map(), isolate());

  // The proxy's hash should be retained across reinitialization.
  Handle<Object> hash(object->hash(), isolate());

  if (old_map->is_prototype_map()) {
    map = Map::Copy(map, "CopyAsPrototypeForJSGlobalProxy");
    map->set_is_prototype_map(true);
  }
  JSObject::NotifyMapChange(old_map, map, isolate());
  old_map->NotifyLeafMapLayoutChange();

  // Check that the already allocated object has the same size and type as
  // objects allocated using the constructor.
  DCHECK(map->instance_size() == old_map->instance_size());
  DCHECK(map->instance_type() == old_map->instance_type());

  // Allocate the backing storage for the properties.
  Handle<FixedArray> properties = empty_fixed_array();

  // In order to keep heap in consistent state there must be no allocations
  // before object re-initialization is finished.
  DisallowHeapAllocation no_allocation;

  // Reset the map for the object.
  object->synchronized_set_map(*map);

  Heap* heap = isolate()->heap();
  // Reinitialize the object from the constructor map.
  heap->InitializeJSObjectFromMap(*object, *properties, *map);

  // Restore the saved hash.
  object->set_hash(*hash);
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfo(
    Handle<String> name, int number_of_literals, FunctionKind kind,
    Handle<Code> code, Handle<ScopeInfo> scope_info) {
  DCHECK(IsValidFunctionKind(kind));
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo(
      name, code, IsConstructable(kind, scope_info->language_mode()));
  shared->set_scope_info(*scope_info);
  shared->set_outer_scope_info(*the_hole_value());
  shared->set_kind(kind);
  shared->set_num_literals(number_of_literals);
  if (IsGeneratorFunction(kind)) {
    shared->set_instance_class_name(isolate()->heap()->Generator_string());
  }
  return shared;
}

Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfoForLiteral(
    FunctionLiteral* literal, Handle<Script> script) {
  Handle<Code> code = isolate()->builtins()->CompileLazy();
  Handle<ScopeInfo> scope_info(ScopeInfo::Empty(isolate()));
  Handle<SharedFunctionInfo> result = NewSharedFunctionInfo(
      literal->name(), literal->materialized_literal_count(), literal->kind(),
      code, scope_info);
  SharedFunctionInfo::InitFromFunctionLiteral(result, literal);
  SharedFunctionInfo::SetScript(result, script);
  return result;
}

Handle<JSMessageObject> Factory::NewJSMessageObject(
    MessageTemplate::Template message, Handle<Object> argument,
    int start_position, int end_position, Handle<Object> script,
    Handle<Object> stack_frames) {
  Handle<Map> map = message_object_map();
  Handle<JSMessageObject> message_obj = New<JSMessageObject>(map, NEW_SPACE);
  message_obj->set_properties(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message_obj->initialize_elements();
  message_obj->set_elements(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message_obj->set_type(message);
  message_obj->set_argument(*argument);
  message_obj->set_start_position(start_position);
  message_obj->set_end_position(end_position);
  message_obj->set_script(*script);
  message_obj->set_stack_frames(*stack_frames);
  message_obj->set_error_level(v8::Isolate::kMessageError);
  return message_obj;
}


Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfo(
    Handle<String> name, MaybeHandle<Code> maybe_code, bool is_constructor) {
  // Function names are assumed to be flat elsewhere. Must flatten before
  // allocating SharedFunctionInfo to avoid GC seeing the uninitialized SFI.
  name = String::Flatten(name, TENURED);

  Handle<Map> map = shared_function_info_map();
  Handle<SharedFunctionInfo> share = New<SharedFunctionInfo>(map, OLD_SPACE);

  // Set pointer fields.
  share->set_name(*name);
  share->set_function_data(*undefined_value(), SKIP_WRITE_BARRIER);
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    code = isolate()->builtins()->Illegal();
  }
  share->set_code(*code);
  share->set_optimized_code_map(*empty_fixed_array());
  share->set_scope_info(ScopeInfo::Empty(isolate()));
  share->set_outer_scope_info(*the_hole_value());
  Handle<Code> construct_stub =
      is_constructor ? isolate()->builtins()->JSConstructStubGeneric()
                     : isolate()->builtins()->ConstructedNonConstructable();
  share->SetConstructStub(*construct_stub);
  share->set_instance_class_name(*Object_string());
  share->set_script(*undefined_value(), SKIP_WRITE_BARRIER);
  share->set_debug_info(DebugInfo::uninitialized(), SKIP_WRITE_BARRIER);
  share->set_function_identifier(*undefined_value(), SKIP_WRITE_BARRIER);
  StaticFeedbackVectorSpec empty_spec;
  Handle<FeedbackMetadata> feedback_metadata =
      FeedbackMetadata::New(isolate(), &empty_spec);
  share->set_feedback_metadata(*feedback_metadata, SKIP_WRITE_BARRIER);
  share->set_function_literal_id(FunctionLiteral::kIdTypeInvalid);
#if TRACE_MAPS
  share->set_unique_id(isolate()->GetNextUniqueSharedFunctionInfoId());
#endif
  share->set_profiler_ticks(0);
  share->set_ast_node_count(0);
  share->set_counters(0);

  // Set integer fields (smi or int, depending on the architecture).
  share->set_length(0);
  share->set_internal_formal_parameter_count(0);
  share->set_expected_nof_properties(0);
  share->set_num_literals(0);
  share->set_start_position_and_type(0);
  share->set_end_position(0);
  share->set_function_token_position(0);
  // All compiler hints default to false or 0.
  share->set_compiler_hints(0);
  share->set_opt_count_and_bailout_reason(0);

  // Link into the list.
  Handle<Object> new_noscript_list =
      WeakFixedArray::Add(noscript_shared_function_infos(), share);
  isolate()->heap()->set_noscript_shared_function_infos(*new_noscript_list);

  return share;
}


static inline int NumberCacheHash(Handle<FixedArray> cache,
                                  Handle<Object> number) {
  int mask = (cache->length() >> 1) - 1;
  if (number->IsSmi()) {
    return Handle<Smi>::cast(number)->value() & mask;
  } else {
    int64_t bits = bit_cast<int64_t>(number->Number());
    return (static_cast<int>(bits) ^ static_cast<int>(bits >> 32)) & mask;
  }
}


Handle<Object> Factory::GetNumberStringCache(Handle<Object> number) {
  DisallowHeapAllocation no_gc;
  int hash = NumberCacheHash(number_string_cache(), number);
  Object* key = number_string_cache()->get(hash * 2);
  if (key == *number || (key->IsHeapNumber() && number->IsHeapNumber() &&
                         key->Number() == number->Number())) {
    return Handle<String>(
        String::cast(number_string_cache()->get(hash * 2 + 1)), isolate());
  }
  return undefined_value();
}


void Factory::SetNumberStringCache(Handle<Object> number,
                                   Handle<String> string) {
  int hash = NumberCacheHash(number_string_cache(), number);
  if (number_string_cache()->get(hash * 2) != *undefined_value()) {
    int full_size = isolate()->heap()->FullSizeNumberStringCacheLength();
    if (number_string_cache()->length() != full_size) {
      Handle<FixedArray> new_cache = NewFixedArray(full_size, TENURED);
      isolate()->heap()->set_number_string_cache(*new_cache);
      return;
    }
  }
  number_string_cache()->set(hash * 2, *number);
  number_string_cache()->set(hash * 2 + 1, *string);
}


Handle<String> Factory::NumberToString(Handle<Object> number,
                                       bool check_number_string_cache) {
  isolate()->counters()->number_to_string_runtime()->Increment();
  if (check_number_string_cache) {
    Handle<Object> cached = GetNumberStringCache(number);
    if (!cached->IsUndefined(isolate())) return Handle<String>::cast(cached);
  }

  char arr[100];
  Vector<char> buffer(arr, arraysize(arr));
  const char* str;
  if (number->IsSmi()) {
    int num = Handle<Smi>::cast(number)->value();
    str = IntToCString(num, buffer);
  } else {
    double num = Handle<HeapNumber>::cast(number)->value();
    str = DoubleToCString(num, buffer);
  }

  // We tenure the allocated string since it is referenced from the
  // number-string cache which lives in the old space.
  Handle<String> js_string = NewStringFromAsciiChecked(str, TENURED);
  SetNumberStringCache(number, js_string);
  return js_string;
}


Handle<DebugInfo> Factory::NewDebugInfo(Handle<SharedFunctionInfo> shared) {
  // Allocate initial fixed array for active break points before allocating the
  // debug info object to avoid allocation while setting up the debug info
  // object.
  Handle<FixedArray> break_points(
      NewFixedArray(DebugInfo::kEstimatedNofBreakPointsInFunction));

  // Make a copy of the bytecode array if available.
  Handle<Object> maybe_debug_bytecode_array = undefined_value();
  if (shared->HasBytecodeArray()) {
    Handle<BytecodeArray> original(shared->bytecode_array());
    maybe_debug_bytecode_array = CopyBytecodeArray(original);
  }

  // Create and set up the debug info object. Debug info contains function, a
  // copy of the original code, the executing code and initial fixed array for
  // active break points.
  Handle<DebugInfo> debug_info =
      Handle<DebugInfo>::cast(NewStruct(DEBUG_INFO_TYPE));
  debug_info->set_shared(*shared);
  debug_info->set_debug_bytecode_array(*maybe_debug_bytecode_array);
  debug_info->set_break_points(*break_points);

  // Link debug info to function.
  shared->set_debug_info(*debug_info);

  return debug_info;
}


Handle<JSObject> Factory::NewArgumentsObject(Handle<JSFunction> callee,
                                             int length) {
  bool strict_mode_callee = is_strict(callee->shared()->language_mode()) ||
                            !callee->shared()->has_simple_parameters();
  Handle<Map> map = strict_mode_callee ? isolate()->strict_arguments_map()
                                       : isolate()->sloppy_arguments_map();
  AllocationSiteUsageContext context(isolate(), Handle<AllocationSite>(),
                                     false);
  DCHECK(!isolate()->has_pending_exception());
  Handle<JSObject> result = NewJSObjectFromMap(map);
  Handle<Smi> value(Smi::FromInt(length), isolate());
  Object::SetProperty(result, length_string(), value, STRICT).Assert();
  if (!strict_mode_callee) {
    Object::SetProperty(result, callee_string(), callee, STRICT).Assert();
  }
  return result;
}


Handle<JSWeakMap> Factory::NewJSWeakMap() {
  // TODO(adamk): Currently the map is only created three times per
  // isolate. If it's created more often, the map should be moved into the
  // strong root list.
  Handle<Map> map = NewMap(JS_WEAK_MAP_TYPE, JSWeakMap::kSize);
  return Handle<JSWeakMap>::cast(NewJSObjectFromMap(map));
}


Handle<Map> Factory::ObjectLiteralMapFromCache(Handle<Context> context,
                                               int number_of_properties,
                                               bool* is_result_from_cache) {
  const int kMapCacheSize = 128;

  // We do not cache maps for too many properties or when running builtin code.
  if (number_of_properties > kMapCacheSize ||
      isolate()->bootstrapper()->IsActive()) {
    *is_result_from_cache = false;
    Handle<Map> map = Map::Create(isolate(), number_of_properties);
    return map;
  }
  *is_result_from_cache = true;
  if (number_of_properties == 0) {
    // Reuse the initial map of the Object function if the literal has no
    // predeclared properties.
    return handle(context->object_function()->initial_map(), isolate());
  }

  int cache_index = number_of_properties - 1;
  Handle<Object> maybe_cache(context->map_cache(), isolate());
  if (maybe_cache->IsUndefined(isolate())) {
    // Allocate the new map cache for the native context.
    maybe_cache = NewFixedArray(kMapCacheSize, TENURED);
    context->set_map_cache(*maybe_cache);
  } else {
    // Check to see whether there is a matching element in the cache.
    Handle<FixedArray> cache = Handle<FixedArray>::cast(maybe_cache);
    Object* result = cache->get(cache_index);
    if (result->IsWeakCell()) {
      WeakCell* cell = WeakCell::cast(result);
      if (!cell->cleared()) {
        return handle(Map::cast(cell->value()), isolate());
      }
    }
  }
  // Create a new map and add it to the cache.
  Handle<FixedArray> cache = Handle<FixedArray>::cast(maybe_cache);
  Handle<Map> map = Map::Create(isolate(), number_of_properties);
  Handle<WeakCell> cell = NewWeakCell(map);
  cache->set(cache_index, *cell);
  return map;
}


void Factory::SetRegExpAtomData(Handle<JSRegExp> regexp,
                                JSRegExp::Type type,
                                Handle<String> source,
                                JSRegExp::Flags flags,
                                Handle<Object> data) {
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kAtomDataSize);

  store->set(JSRegExp::kTagIndex, Smi::FromInt(type));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kAtomPatternIndex, *data);
  regexp->set_data(*store);
}


void Factory::SetRegExpIrregexpData(Handle<JSRegExp> regexp,
                                    JSRegExp::Type type,
                                    Handle<String> source,
                                    JSRegExp::Flags flags,
                                    int capture_count) {
  Handle<FixedArray> store = NewFixedArray(JSRegExp::kIrregexpDataSize);
  Smi* uninitialized = Smi::FromInt(JSRegExp::kUninitializedValue);
  store->set(JSRegExp::kTagIndex, Smi::FromInt(type));
  store->set(JSRegExp::kSourceIndex, *source);
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags));
  store->set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpLatin1CodeSavedIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeSavedIndex, uninitialized);
  store->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::kZero);
  store->set(JSRegExp::kIrregexpCaptureCountIndex,
             Smi::FromInt(capture_count));
  store->set(JSRegExp::kIrregexpCaptureNameMapIndex, uninitialized);
  regexp->set_data(*store);
}

Handle<RegExpMatchInfo> Factory::NewRegExpMatchInfo() {
  // Initially, the last match info consists of all fixed fields plus space for
  // the match itself (i.e., 2 capture indices).
  static const int kInitialSize = RegExpMatchInfo::kFirstCaptureIndex +
                                  RegExpMatchInfo::kInitialCaptureIndices;

  Handle<FixedArray> elems = NewFixedArray(kInitialSize);
  Handle<RegExpMatchInfo> result = Handle<RegExpMatchInfo>::cast(elems);

  result->SetNumberOfCaptureRegisters(RegExpMatchInfo::kInitialCaptureIndices);
  result->SetLastSubject(*empty_string());
  result->SetLastInput(*undefined_value());
  result->SetCapture(0, 0);
  result->SetCapture(1, 0);

  return result;
}

Handle<Object> Factory::GlobalConstantFor(Handle<Name> name) {
  if (Name::Equals(name, undefined_string())) return undefined_value();
  if (Name::Equals(name, nan_string())) return nan_value();
  if (Name::Equals(name, infinity_string())) return infinity_value();
  return Handle<Object>::null();
}


Handle<Object> Factory::ToBoolean(bool value) {
  return value ? true_value() : false_value();
}

Handle<String> Factory::ToPrimitiveHintString(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return default_string();
    case ToPrimitiveHint::kNumber:
      return number_string();
    case ToPrimitiveHint::kString:
      return string_string();
  }
  UNREACHABLE();
  return Handle<String>::null();
}

Handle<Map> Factory::CreateSloppyFunctionMap(FunctionMode function_mode) {
  Handle<Map> map = NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetFunctionInstanceDescriptor(map, function_mode);
  map->set_is_constructor(IsFunctionModeWithPrototype(function_mode));
  map->set_is_callable();
  return map;
}

void Factory::SetFunctionInstanceDescriptor(Handle<Map> map,
                                            FunctionMode function_mode) {
  int size = IsFunctionModeWithPrototype(function_mode) ? 5 : 4;
  Map::EnsureDescriptorSlack(map, size);

  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  Handle<AccessorInfo> length =
      Accessors::FunctionLengthInfo(isolate(), roc_attribs);
  {  // Add length.
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(length->name())), length, roc_attribs);
    map->AppendDescriptor(&d);
  }

  STATIC_ASSERT(JSFunction::kNameDescriptorIndex == 1);
  Handle<AccessorInfo> name =
      Accessors::FunctionNameInfo(isolate(), roc_attribs);
  {  // Add name.
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(name->name())), name, roc_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> args =
      Accessors::FunctionArgumentsInfo(isolate(), ro_attribs);
  {  // Add arguments.
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(args->name())), args, ro_attribs);
    map->AppendDescriptor(&d);
  }
  Handle<AccessorInfo> caller =
      Accessors::FunctionCallerInfo(isolate(), ro_attribs);
  {  // Add caller.
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(caller->name())), caller, ro_attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    if (function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE) {
      ro_attribs = static_cast<PropertyAttributes>(ro_attribs & ~READ_ONLY);
    }
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), ro_attribs);
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(prototype->name())), prototype, ro_attribs);
    map->AppendDescriptor(&d);
  }
}

Handle<Map> Factory::CreateStrictFunctionMap(
    FunctionMode function_mode, Handle<JSFunction> empty_function) {
  Handle<Map> map = NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetStrictFunctionInstanceDescriptor(map, function_mode);
  map->set_is_constructor(IsFunctionModeWithPrototype(function_mode));
  map->set_is_callable();
  Map::SetPrototype(map, empty_function);
  return map;
}

void Factory::SetStrictFunctionInstanceDescriptor(Handle<Map> map,
                                                  FunctionMode function_mode) {
  int size = IsFunctionModeWithPrototype(function_mode) ? 3 : 2;
  Map::EnsureDescriptorSlack(map, size);

  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes ro_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  DCHECK(function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ||
         function_mode == FUNCTION_WITH_READONLY_PROTOTYPE ||
         function_mode == FUNCTION_WITHOUT_PROTOTYPE);
  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  {  // Add length.
    Handle<AccessorInfo> length =
        Accessors::FunctionLengthInfo(isolate(), roc_attribs);
    Descriptor d = Descriptor::AccessorConstant(
        handle(Name::cast(length->name())), length, roc_attribs);
    map->AppendDescriptor(&d);
  }

  STATIC_ASSERT(JSFunction::kNameDescriptorIndex == 1);
  {  // Add name.
    Handle<AccessorInfo> name =
        Accessors::FunctionNameInfo(isolate(), roc_attribs);
    Descriptor d = Descriptor::AccessorConstant(
        handle(Name::cast(name->name())), name, roc_attribs);
    map->AppendDescriptor(&d);
  }
  if (IsFunctionModeWithPrototype(function_mode)) {
    // Add prototype.
    PropertyAttributes attribs =
        function_mode == FUNCTION_WITH_WRITEABLE_PROTOTYPE ? rw_attribs
                                                           : ro_attribs;
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), attribs);
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(prototype->name())), prototype, attribs);
    map->AppendDescriptor(&d);
  }
}

Handle<Map> Factory::CreateClassFunctionMap(Handle<JSFunction> empty_function) {
  Handle<Map> map = NewMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  SetClassFunctionInstanceDescriptor(map);
  map->set_is_constructor(true);
  map->set_is_callable();
  Map::SetPrototype(map, empty_function);
  return map;
}

void Factory::SetClassFunctionInstanceDescriptor(Handle<Map> map) {
  Map::EnsureDescriptorSlack(map, 2);

  PropertyAttributes rw_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  PropertyAttributes roc_attribs =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);

  STATIC_ASSERT(JSFunction::kLengthDescriptorIndex == 0);
  {  // Add length.
    Handle<AccessorInfo> length =
        Accessors::FunctionLengthInfo(isolate(), roc_attribs);
    Descriptor d = Descriptor::AccessorConstant(
        handle(Name::cast(length->name())), length, roc_attribs);
    map->AppendDescriptor(&d);
  }

  {
    // Add prototype.
    Handle<AccessorInfo> prototype =
        Accessors::FunctionPrototypeInfo(isolate(), rw_attribs);
    Descriptor d = Descriptor::AccessorConstant(
        Handle<Name>(Name::cast(prototype->name())), prototype, rw_attribs);
    map->AppendDescriptor(&d);
  }
}

}  // namespace internal
}  // namespace v8
