// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"

#include "src/allocation-site-scopes.h"
#include "src/base/bits.h"
#include "src/conversions.h"
#include "src/isolate-inl.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


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


Handle<Oddball> Factory::NewOddball(Handle<Map> map,
                                    const char* to_string,
                                    Handle<Object> to_number,
                                    byte kind) {
  Handle<Oddball> oddball = New<Oddball>(map, OLD_POINTER_SPACE);
  Oddball::Initialize(isolate(), oddball, to_string, to_number, kind);
  return oddball;
}


Handle<FixedArray> Factory::NewFixedArray(int size, PretenureFlag pretenure) {
  DCHECK(0 <= size);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateFixedArray(size, pretenure),
      FixedArray);
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
    Handle<FixedDoubleArray> double_array =
        Handle<FixedDoubleArray>::cast(array);
    for (int i = 0; i < size; ++i) {
      double_array->set_the_hole(i);
    }
  }
  return array;
}


Handle<ConstantPoolArray> Factory::NewConstantPoolArray(
    const ConstantPoolArray::NumberOfEntries& small) {
  DCHECK(small.total_count() > 0);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateConstantPoolArray(small),
      ConstantPoolArray);
}


Handle<ConstantPoolArray> Factory::NewExtendedConstantPoolArray(
    const ConstantPoolArray::NumberOfEntries& small,
    const ConstantPoolArray::NumberOfEntries& extended) {
  DCHECK(small.total_count() > 0);
  DCHECK(extended.total_count() > 0);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateExtendedConstantPoolArray(small, extended),
      ConstantPoolArray);
}


Handle<OrderedHashSet> Factory::NewOrderedHashSet() {
  return OrderedHashSet::Allocate(isolate(), 4);
}


Handle<OrderedHashMap> Factory::NewOrderedHashMap() {
  return OrderedHashMap::Allocate(isolate(), 4);
}


Handle<AccessorPair> Factory::NewAccessorPair() {
  Handle<AccessorPair> accessors =
      Handle<AccessorPair>::cast(NewStruct(ACCESSOR_PAIR_TYPE));
  accessors->set_getter(*the_hole_value(), SKIP_WRITE_BARRIER);
  accessors->set_setter(*the_hole_value(), SKIP_WRITE_BARRIER);
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


// Internalized strings are created in the old generation (data space).
Handle<String> Factory::InternalizeString(Handle<String> string) {
  if (string->IsInternalizedString()) return string;
  return StringTable::LookupString(isolate(), string);
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
  int utf16_length = decoder->Utf16Length();
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


MaybeHandle<String> Factory::NewStringFromTwoByte(Vector<const uc16> string,
                                                  PretenureFlag pretenure) {
  int length = string.length();
  const uc16* start = string.start();
  if (String::IsOneByte(start, length)) {
    if (length == 1) return LookupSingleCharacterStringFromCode(string[0]);
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(),
        result,
        NewRawOneByteString(length, pretenure),
        String);
    CopyChars(result->GetChars(), start, length);
    return result;
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(),
        result,
        NewRawTwoByteString(length, pretenure),
        String);
    CopyChars(result->GetChars(), start, length);
    return result;
  }
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
    case STRING_TYPE: return internalized_string_map();
    case ONE_BYTE_STRING_TYPE:
      return one_byte_internalized_string_map();
    case EXTERNAL_STRING_TYPE: return external_internalized_string_map();
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

  Handle<Map> map = (is_one_byte || is_one_byte_data_in_two_byte_string)
                        ? cons_one_byte_string_map()
                        : cons_string_map();
  Handle<ConsString> result =  New<ConsString>(map, NEW_SPACE);

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  result->set_hash_field(String::kEmptyHashField);
  result->set_length(length);
  result->set_first(*left, mode);
  result->set_second(*right, mode);
  return result;
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

  Handle<Map> map = external_one_byte_string_map();
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
  Handle<Map> map = is_one_byte ?
      external_string_with_one_byte_data_map() : external_string_map();
  Handle<ExternalTwoByteString> external_string =
      New<ExternalTwoByteString>(map, NEW_SPACE);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return external_string;
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


Handle<Symbol> Factory::NewPrivateOwnSymbol() {
  Handle<Symbol> symbol = NewSymbol();
  symbol->set_is_private(true);
  symbol->set_is_own(true);
  return symbol;
}


Handle<Context> Factory::NewNativeContext() {
  Handle<FixedArray> array = NewFixedArray(Context::NATIVE_CONTEXT_SLOTS);
  array->set_map_no_write_barrier(*native_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_js_array_maps(*undefined_value());
  DCHECK(context->IsNativeContext());
  return context;
}


Handle<Context> Factory::NewGlobalContext(Handle<JSFunction> function,
                                          Handle<ScopeInfo> scope_info) {
  Handle<FixedArray> array =
      NewFixedArray(scope_info->ContextLength(), TENURED);
  array->set_map_no_write_barrier(*global_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(function->context());
  context->set_extension(*scope_info);
  context->set_global_object(function->context()->global_object());
  DCHECK(context->IsGlobalContext());
  return context;
}


Handle<Context> Factory::NewModuleContext(Handle<ScopeInfo> scope_info) {
  Handle<FixedArray> array =
      NewFixedArray(scope_info->ContextLength(), TENURED);
  array->set_map_no_write_barrier(*module_context_map());
  // Instance link will be set later.
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_extension(Smi::FromInt(0));
  return context;
}


Handle<Context> Factory::NewFunctionContext(int length,
                                            Handle<JSFunction> function) {
  DCHECK(length >= Context::MIN_CONTEXT_SLOTS);
  Handle<FixedArray> array = NewFixedArray(length);
  array->set_map_no_write_barrier(*function_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(function->context());
  context->set_extension(Smi::FromInt(0));
  context->set_global_object(function->context()->global_object());
  return context;
}


Handle<Context> Factory::NewCatchContext(Handle<JSFunction> function,
                                         Handle<Context> previous,
                                         Handle<String> name,
                                         Handle<Object> thrown_object) {
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == Context::THROWN_OBJECT_INDEX);
  Handle<FixedArray> array = NewFixedArray(Context::MIN_CONTEXT_SLOTS + 1);
  array->set_map_no_write_barrier(*catch_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(*previous);
  context->set_extension(*name);
  context->set_global_object(previous->global_object());
  context->set(Context::THROWN_OBJECT_INDEX, *thrown_object);
  return context;
}


Handle<Context> Factory::NewWithContext(Handle<JSFunction> function,
                                        Handle<Context> previous,
                                        Handle<JSReceiver> extension) {
  Handle<FixedArray> array = NewFixedArray(Context::MIN_CONTEXT_SLOTS);
  array->set_map_no_write_barrier(*with_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(*previous);
  context->set_extension(*extension);
  context->set_global_object(previous->global_object());
  return context;
}


Handle<Context> Factory::NewBlockContext(Handle<JSFunction> function,
                                         Handle<Context> previous,
                                         Handle<ScopeInfo> scope_info) {
  Handle<FixedArray> array =
      NewFixedArrayWithHoles(scope_info->ContextLength());
  array->set_map_no_write_barrier(*block_context_map());
  Handle<Context> context = Handle<Context>::cast(array);
  context->set_closure(*function);
  context->set_previous(*previous);
  context->set_extension(*scope_info);
  context->set_global_object(previous->global_object());
  return context;
}


Handle<Struct> Factory::NewStruct(InstanceType type) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateStruct(type),
      Struct);
}


Handle<CodeCache> Factory::NewCodeCache() {
  Handle<CodeCache> code_cache =
      Handle<CodeCache>::cast(NewStruct(CODE_CACHE_TYPE));
  code_cache->set_default_cache(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  code_cache->set_normal_type_cache(*undefined_value(), SKIP_WRITE_BARRIER);
  return code_cache;
}


Handle<AliasedArgumentsEntry> Factory::NewAliasedArgumentsEntry(
    int aliased_context_slot) {
  Handle<AliasedArgumentsEntry> entry = Handle<AliasedArgumentsEntry>::cast(
      NewStruct(ALIASED_ARGUMENTS_ENTRY_TYPE));
  entry->set_aliased_context_slot(aliased_context_slot);
  return entry;
}


Handle<DeclaredAccessorDescriptor> Factory::NewDeclaredAccessorDescriptor() {
  return Handle<DeclaredAccessorDescriptor>::cast(
      NewStruct(DECLARED_ACCESSOR_DESCRIPTOR_TYPE));
}


Handle<DeclaredAccessorInfo> Factory::NewDeclaredAccessorInfo() {
  Handle<DeclaredAccessorInfo> info =
      Handle<DeclaredAccessorInfo>::cast(
          NewStruct(DECLARED_ACCESSOR_INFO_TYPE));
  info->set_flag(0);  // Must clear the flag, it was initialized as undefined.
  return info;
}


Handle<ExecutableAccessorInfo> Factory::NewExecutableAccessorInfo() {
  Handle<ExecutableAccessorInfo> info =
      Handle<ExecutableAccessorInfo>::cast(
          NewStruct(EXECUTABLE_ACCESSOR_INFO_TYPE));
  info->set_flag(0);  // Must clear the flag, it was initialized as undefined.
  return info;
}


Handle<Script> Factory::NewScript(Handle<String> source) {
  // Generate id for this script.
  Heap* heap = isolate()->heap();
  int id = heap->last_script_id()->value() + 1;
  if (!Smi::IsValid(id) || id < 0) id = 1;
  heap->set_last_script_id(Smi::FromInt(id));

  // Create and initialize script object.
  Handle<Foreign> wrapper = NewForeign(0, TENURED);
  Handle<Script> script = Handle<Script>::cast(NewStruct(SCRIPT_TYPE));
  script->set_source(*source);
  script->set_name(heap->undefined_value());
  script->set_id(Smi::FromInt(id));
  script->set_line_offset(Smi::FromInt(0));
  script->set_column_offset(Smi::FromInt(0));
  script->set_context_data(heap->undefined_value());
  script->set_type(Smi::FromInt(Script::TYPE_NORMAL));
  script->set_wrapper(*wrapper);
  script->set_line_ends(heap->undefined_value());
  script->set_eval_from_shared(heap->undefined_value());
  script->set_eval_from_instructions_offset(Smi::FromInt(0));
  script->set_flags(Smi::FromInt(0));

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


Handle<ExternalArray> Factory::NewExternalArray(int length,
                                                ExternalArrayType array_type,
                                                void* external_pointer,
                                                PretenureFlag pretenure) {
  DCHECK(0 <= length && length <= Smi::kMaxValue);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateExternalArray(length,
                                               array_type,
                                               external_pointer,
                                               pretenure),
      ExternalArray);
}


Handle<FixedTypedArrayBase> Factory::NewFixedTypedArray(
    int length,
    ExternalArrayType array_type,
    PretenureFlag pretenure) {
  DCHECK(0 <= length && length <= Smi::kMaxValue);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateFixedTypedArray(length,
                                                 array_type,
                                                 pretenure),
      FixedTypedArrayBase);
}


Handle<Cell> Factory::NewCell(Handle<Object> value) {
  AllowDeferredHandleDereference convert_to_cell;
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateCell(*value),
      Cell);
}


Handle<PropertyCell> Factory::NewPropertyCellWithHole() {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocatePropertyCell(),
      PropertyCell);
}


Handle<PropertyCell> Factory::NewPropertyCell(Handle<Object> value) {
  AllowDeferredHandleDereference convert_to_cell;
  Handle<PropertyCell> cell = NewPropertyCellWithHole();
  PropertyCell::SetValueInferType(cell, value);
  return cell;
}


Handle<AllocationSite> Factory::NewAllocationSite() {
  Handle<Map> map = allocation_site_map();
  Handle<AllocationSite> site = New<AllocationSite>(map, OLD_POINTER_SPACE);
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


Handle<ConstantPoolArray> Factory::CopyConstantPoolArray(
    Handle<ConstantPoolArray> array) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyConstantPoolArray(*array),
                     ConstantPoolArray);
}


Handle<Object> Factory::NewNumber(double value,
                                  PretenureFlag pretenure) {
  // We need to distinguish the minus zero value and this cannot be
  // done after conversion to int. Doing this by comparing bit
  // patterns is faster than using fpclassify() et al.
  if (IsMinusZero(value)) return NewHeapNumber(-0.0, IMMUTABLE, pretenure);

  int int_value = FastD2I(value);
  if (value == int_value && Smi::IsValid(int_value)) {
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


MaybeHandle<Object> Factory::NewTypeError(const char* message,
                                          Vector<Handle<Object> > args) {
  return NewError("MakeTypeError", message, args);
}


MaybeHandle<Object> Factory::NewTypeError(Handle<String> message) {
  return NewError("$TypeError", message);
}


MaybeHandle<Object> Factory::NewRangeError(const char* message,
                                           Vector<Handle<Object> > args) {
  return NewError("MakeRangeError", message, args);
}


MaybeHandle<Object> Factory::NewRangeError(Handle<String> message) {
  return NewError("$RangeError", message);
}


MaybeHandle<Object> Factory::NewSyntaxError(const char* message,
                                            Handle<JSArray> args) {
  return NewError("MakeSyntaxError", message, args);
}


MaybeHandle<Object> Factory::NewSyntaxError(Handle<String> message) {
  return NewError("$SyntaxError", message);
}


MaybeHandle<Object> Factory::NewReferenceError(const char* message,
                                               Vector<Handle<Object> > args) {
  return NewError("MakeReferenceError", message, args);
}


MaybeHandle<Object> Factory::NewReferenceError(const char* message,
                                               Handle<JSArray> args) {
  return NewError("MakeReferenceError", message, args);
}


MaybeHandle<Object> Factory::NewReferenceError(Handle<String> message) {
  return NewError("$ReferenceError", message);
}


MaybeHandle<Object> Factory::NewError(const char* maker, const char* message,
                                      Vector<Handle<Object> > args) {
  // Instantiate a closeable HandleScope for EscapeFrom.
  v8::EscapableHandleScope scope(reinterpret_cast<v8::Isolate*>(isolate()));
  Handle<FixedArray> array = NewFixedArray(args.length());
  for (int i = 0; i < args.length(); i++) {
    array->set(i, *args[i]);
  }
  Handle<JSArray> object = NewJSArrayWithElements(array);
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                             NewError(maker, message, object), Object);
  return result.EscapeFrom(&scope);
}


MaybeHandle<Object> Factory::NewEvalError(const char* message,
                                          Vector<Handle<Object> > args) {
  return NewError("MakeEvalError", message, args);
}


MaybeHandle<Object> Factory::NewError(const char* message,
                                      Vector<Handle<Object> > args) {
  return NewError("MakeError", message, args);
}


Handle<String> Factory::EmergencyNewError(const char* message,
                                          Handle<JSArray> args) {
  const int kBufferSize = 1000;
  char buffer[kBufferSize];
  size_t space = kBufferSize;
  char* p = &buffer[0];

  Vector<char> v(buffer, kBufferSize);
  StrNCpy(v, message, space);
  space -= Min(space, strlen(message));
  p = &buffer[kBufferSize] - space;

  for (int i = 0; i < Smi::cast(args->length())->value(); i++) {
    if (space > 0) {
      *p++ = ' ';
      space--;
      if (space > 0) {
        Handle<String> arg_str = Handle<String>::cast(
            Object::GetElement(isolate(), args, i).ToHandleChecked());
        SmartArrayPointer<char> arg = arg_str->ToCString();
        Vector<char> v2(p, static_cast<int>(space));
        StrNCpy(v2, arg.get(), space);
        space -= Min(space, strlen(arg.get()));
        p = &buffer[kBufferSize] - space;
      }
    }
  }
  if (space > 0) {
    *p = '\0';
  } else {
    buffer[kBufferSize - 1] = '\0';
  }
  return NewStringFromUtf8(CStrVector(buffer), TENURED).ToHandleChecked();
}


MaybeHandle<Object> Factory::NewError(const char* maker, const char* message,
                                      Handle<JSArray> args) {
  Handle<String> make_str = InternalizeUtf8String(maker);
  Handle<Object> fun_obj = Object::GetProperty(
      isolate()->js_builtins_object(), make_str).ToHandleChecked();
  // If the builtins haven't been properly configured yet this error
  // constructor may not have been defined.  Bail out.
  if (!fun_obj->IsJSFunction()) {
    return EmergencyNewError(message, args);
  }
  Handle<JSFunction> fun = Handle<JSFunction>::cast(fun_obj);
  Handle<Object> message_obj = InternalizeUtf8String(message);
  Handle<Object> argv[] = { message_obj, args };

  // Invoke the JavaScript factory method. If an exception is thrown while
  // running the factory method, use the exception as the result.
  Handle<Object> result;
  MaybeHandle<Object> exception;
  if (!Execution::TryCall(fun,
                          isolate()->js_builtins_object(),
                          arraysize(argv),
                          argv,
                          &exception).ToHandle(&result)) {
    return exception;
  }
  return result;
}


MaybeHandle<Object> Factory::NewError(Handle<String> message) {
  return NewError("$Error", message);
}


MaybeHandle<Object> Factory::NewError(const char* constructor,
                                      Handle<String> message) {
  Handle<String> constr = InternalizeUtf8String(constructor);
  Handle<JSFunction> fun = Handle<JSFunction>::cast(Object::GetProperty(
      isolate()->js_builtins_object(), constr).ToHandleChecked());
  Handle<Object> argv[] = { message };

  // Invoke the JavaScript factory method. If an exception is thrown while
  // running the factory method, use the exception as the result.
  Handle<Object> result;
  MaybeHandle<Object> exception;
  if (!Execution::TryCall(fun,
                          isolate()->js_builtins_object(),
                          arraysize(argv),
                          argv,
                          &exception).ToHandle(&result)) {
    return exception;
  }
  return result;
}


void Factory::InitializeFunction(Handle<JSFunction> function,
                                 Handle<SharedFunctionInfo> info,
                                 Handle<Context> context) {
  function->initialize_properties();
  function->initialize_elements();
  function->set_shared(*info);
  function->set_code(info->code());
  function->set_context(*context);
  function->set_prototype_or_initial_map(*the_hole_value());
  function->set_literals_or_bindings(*empty_fixed_array());
  function->set_next_function_link(*undefined_value());
}


Handle<JSFunction> Factory::NewFunction(Handle<Map> map,
                                        Handle<SharedFunctionInfo> info,
                                        Handle<Context> context,
                                        PretenureFlag pretenure) {
  AllocationSpace space = pretenure == TENURED ? OLD_POINTER_SPACE : NEW_SPACE;
  Handle<JSFunction> result = New<JSFunction>(map, space);
  InitializeFunction(result, info, context);
  return result;
}


Handle<JSFunction> Factory::NewFunction(Handle<Map> map,
                                        Handle<String> name,
                                        MaybeHandle<Code> code) {
  Handle<Context> context(isolate()->native_context());
  Handle<SharedFunctionInfo> info = NewSharedFunctionInfo(name, code);
  DCHECK((info->strict_mode() == SLOPPY) &&
         (map.is_identical_to(isolate()->sloppy_function_map()) ||
          map.is_identical_to(
              isolate()->sloppy_function_without_prototype_map()) ||
          map.is_identical_to(
              isolate()->sloppy_function_with_readonly_prototype_map())));
  return NewFunction(map, info, context);
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name) {
  return NewFunction(
      isolate()->sloppy_function_map(), name, MaybeHandle<Code>());
}


Handle<JSFunction> Factory::NewFunctionWithoutPrototype(Handle<String> name,
                                                        Handle<Code> code) {
  return NewFunction(
      isolate()->sloppy_function_without_prototype_map(), name, code);
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name,
                                        Handle<Code> code,
                                        Handle<Object> prototype,
                                        bool read_only_prototype) {
  Handle<Map> map = read_only_prototype
      ? isolate()->sloppy_function_with_readonly_prototype_map()
      : isolate()->sloppy_function_map();
  Handle<JSFunction> result = NewFunction(map, name, code);
  result->set_prototype_or_initial_map(*prototype);
  return result;
}


Handle<JSFunction> Factory::NewFunction(Handle<String> name,
                                        Handle<Code> code,
                                        Handle<Object> prototype,
                                        InstanceType type,
                                        int instance_size,
                                        bool read_only_prototype) {
  // Allocate the function
  Handle<JSFunction> function = NewFunction(
      name, code, prototype, read_only_prototype);

  ElementsKind elements_kind =
      type == JS_ARRAY_TYPE ? FAST_SMI_ELEMENTS : FAST_HOLEY_SMI_ELEMENTS;
  Handle<Map> initial_map = NewMap(type, instance_size, elements_kind);
  if (prototype->IsTheHole() && !function->shared()->is_generator()) {
    prototype = NewFunctionPrototype(function);
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
  if (function->shared()->is_generator()) {
    // Generator prototypes can share maps since they don't have "constructor"
    // properties.
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

  if (!function->shared()->is_generator()) {
    JSObject::AddProperty(prototype, constructor_string(), function, DONT_ENUM);
  }

  return prototype;
}


Handle<JSFunction> Factory::NewFunctionFromSharedFunctionInfo(
    Handle<SharedFunctionInfo> info,
    Handle<Context> context,
    PretenureFlag pretenure) {
  int map_index = Context::FunctionMapIndex(info->strict_mode(), info->kind());
  Handle<Map> map(Map::cast(context->native_context()->get(map_index)));
  Handle<JSFunction> result = NewFunction(map, info, context, pretenure);

  if (info->ic_age() != isolate()->heap()->global_ic_age()) {
    info->ResetForNewContext(isolate()->heap()->global_ic_age());
  }

  int index = info->SearchOptimizedCodeMap(context->native_context(),
                                           BailoutId::None());
  if (!info->bound() && index < 0) {
    int number_of_literals = info->num_literals();
    Handle<FixedArray> literals = NewFixedArray(number_of_literals, pretenure);
    if (number_of_literals > 0) {
      // Store the native context in the literals array prefix. This
      // context will be used when creating object, regexp and array
      // literals in this function.
      literals->set(JSFunction::kLiteralNativeContextIndex,
                    context->native_context());
    }
    result->set_literals(*literals);
  }

  if (index > 0) {
    // Caching of optimized code enabled and optimized code found.
    FixedArray* literals = info->GetLiteralsFromOptimizedCodeMap(index);
    if (literals != NULL) result->set_literals(literals);
    Code* code = info->GetCodeFromOptimizedCodeMap(index);
    DCHECK(!code->marked_for_deoptimization());
    result->ReplaceCode(code);
    return result;
  }

  if (isolate()->use_crankshaft() &&
      FLAG_always_opt &&
      result->is_compiled() &&
      !info->is_toplevel() &&
      info->allows_lazy_compilation() &&
      !info->optimization_disabled() &&
      !isolate()->DebuggerHasBreakPoints()) {
    result->MarkForOptimization();
  }
  return result;
}


Handle<ScopeInfo> Factory::NewScopeInfo(int length) {
  Handle<FixedArray> array = NewFixedArray(length, TENURED);
  array->set_map_no_write_barrier(*scope_info_map());
  Handle<ScopeInfo> scope_info = Handle<ScopeInfo>::cast(array);
  return scope_info;
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
  Handle<ConstantPoolArray> constant_pool =
      desc.origin->NewConstantPool(isolate());

  // Compute size.
  int body_size = RoundUp(desc.instr_size, kObjectAlignment);
  int obj_size = Code::SizeFor(body_size);

  Handle<Code> code = NewCodeRaw(obj_size, immovable);
  DCHECK(isolate()->code_range() == NULL ||
         !isolate()->code_range()->valid() ||
         isolate()->code_range()->contains(code->address()));

  // The code object has not been fully initialized yet.  We rely on the
  // fact that no allocation will happen from this point on.
  DisallowHeapAllocation no_gc;
  code->set_gc_metadata(Smi::FromInt(0));
  code->set_ic_age(isolate()->heap()->global_ic_age());
  code->set_instruction_size(desc.instr_size);
  code->set_relocation_info(*reloc_info);
  code->set_flags(flags);
  code->set_raw_kind_specific_flags1(0);
  code->set_raw_kind_specific_flags2(0);
  code->set_is_crankshafted(crankshafted);
  code->set_deoptimization_data(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  code->set_raw_type_feedback_info(Smi::FromInt(0));
  code->set_next_code_link(*undefined_value());
  code->set_handler_table(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  code->set_prologue_offset(prologue_offset);
  if (code->kind() == Code::OPTIMIZED_FUNCTION) {
    code->set_marked_for_deoptimization(false);
  }

  if (is_debug) {
    DCHECK(code->kind() == Code::FUNCTION);
    code->set_has_debug_break_slots(true);
  }

  desc.origin->PopulateConstantPool(*constant_pool);
  code->set_constant_pool(*constant_pool);

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


Handle<Code> Factory::CopyCode(Handle<Code> code, Vector<byte> reloc_info) {
  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->CopyCode(*code, reloc_info),
                     Code);
}


Handle<JSObject> Factory::NewJSObject(Handle<JSFunction> constructor,
                                      PretenureFlag pretenure) {
  JSFunction::EnsureHasInitialMap(constructor);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObject(*constructor, pretenure), JSObject);
}


Handle<JSObject> Factory::NewJSObjectWithMemento(
    Handle<JSFunction> constructor,
    Handle<AllocationSite> site) {
  JSFunction::EnsureHasInitialMap(constructor);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObject(*constructor, NOT_TENURED, *site),
      JSObject);
}


Handle<JSModule> Factory::NewJSModule(Handle<Context> context,
                                      Handle<ScopeInfo> scope_info) {
  // Allocate a fresh map. Modules do not have a prototype.
  Handle<Map> map = NewMap(JS_MODULE_TYPE, JSModule::kSize);
  // Allocate the object based on the map.
  Handle<JSModule> module =
      Handle<JSModule>::cast(NewJSObjectFromMap(map, TENURED));
  module->set_context(*context);
  module->set_scope_info(*scope_info);
  return module;
}


Handle<GlobalObject> Factory::NewGlobalObject(Handle<JSFunction> constructor) {
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
  DCHECK(map->inobject_properties() == 0);

  // Initial size of the backing store to avoid resize of the storage during
  // bootstrapping. The size differs between the JS global object ad the
  // builtins object.
  int initial_size = map->instance_type() == JS_GLOBAL_OBJECT_TYPE ? 64 : 512;

  // Allocate a dictionary object for backing storage.
  int at_least_space_for = map->NumberOfOwnDescriptors() * 2 + initial_size;
  Handle<NameDictionary> dictionary =
      NameDictionary::New(isolate(), at_least_space_for);

  // The global object might be created from an object template with accessors.
  // Fill these accessors into the dictionary.
  Handle<DescriptorArray> descs(map->instance_descriptors());
  for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
    PropertyDetails details = descs->GetDetails(i);
    DCHECK(details.type() == CALLBACKS);  // Only accessors are expected.
    PropertyDetails d = PropertyDetails(details.attributes(), CALLBACKS, i + 1);
    Handle<Name> name(descs->GetKey(i));
    Handle<Object> value(descs->GetCallbacksObject(i), isolate());
    Handle<PropertyCell> cell = NewPropertyCell(value);
    // |dictionary| already contains enough space for all properties.
    USE(NameDictionary::Add(dictionary, name, cell, d));
  }

  // Allocate the global object and initialize it with the backing store.
  Handle<GlobalObject> global = New<GlobalObject>(map, OLD_POINTER_SPACE);
  isolate()->heap()->InitializeJSObjectFromMap(*global, *dictionary, *map);

  // Create a new map for the global object.
  Handle<Map> new_map = Map::CopyDropDescriptors(map);
  new_map->set_dictionary_map(true);

  // Set up the global object as a normalized object.
  global->set_map(*new_map);
  global->set_properties(*dictionary);

  // Make sure result is a global object with properties in dictionary.
  DCHECK(global->IsGlobalObject() && !global->HasFastProperties());
  return global;
}


Handle<JSObject> Factory::NewJSObjectFromMap(
    Handle<Map> map,
    PretenureFlag pretenure,
    bool alloc_props,
    Handle<AllocationSite> allocation_site) {
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObjectFromMap(
          *map,
          pretenure,
          alloc_props,
          allocation_site.is_null() ? NULL : *allocation_site),
      JSObject);
}


Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind,
                                    PretenureFlag pretenure) {
  Context* native_context = isolate()->context()->native_context();
  JSFunction* array_function = native_context->array_function();
  Map* map = array_function->initial_map();
  Map* transition_map = isolate()->get_initial_js_array_map(elements_kind);
  if (transition_map != NULL) map = transition_map;
  return Handle<JSArray>::cast(NewJSObjectFromMap(handle(map), pretenure));
}


Handle<JSArray> Factory::NewJSArray(ElementsKind elements_kind,
                                    int length,
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
    array->set_length(Smi::FromInt(0));
    array->set_elements(*empty_fixed_array());
    return;
  }

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


Handle<JSGeneratorObject> Factory::NewJSGeneratorObject(
    Handle<JSFunction> function) {
  DCHECK(function->shared()->is_generator());
  JSFunction::EnsureHasInitialMap(function);
  Handle<Map> map(function->initial_map());
  DCHECK(map->instance_type() == JS_GENERATOR_OBJECT_TYPE);
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObjectFromMap(*map),
      JSGeneratorObject);
}


Handle<JSArrayBuffer> Factory::NewJSArrayBuffer() {
  Handle<JSFunction> array_buffer_fun(
      isolate()->native_context()->array_buffer_fun());
  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObject(*array_buffer_fun),
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


static JSFunction* GetTypedArrayFun(ExternalArrayType type,
                                    Isolate* isolate) {
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


Handle<JSTypedArray> Factory::NewJSTypedArray(ExternalArrayType type) {
  Handle<JSFunction> typed_array_fun_handle(GetTypedArrayFun(type, isolate()));

  CALL_HEAP_FUNCTION(
      isolate(),
      isolate()->heap()->AllocateJSObject(*typed_array_fun_handle),
      JSTypedArray);
}


Handle<JSProxy> Factory::NewJSProxy(Handle<Object> handler,
                                    Handle<Object> prototype) {
  // Allocate map.
  // TODO(rossberg): Once we optimize proxies, think about a scheme to share
  // maps. Will probably depend on the identity of the handler object, too.
  Handle<Map> map = NewMap(JS_PROXY_TYPE, JSProxy::kSize);
  map->set_prototype(*prototype);

  // Allocate the proxy object.
  Handle<JSProxy> result = New<JSProxy>(map, NEW_SPACE);
  result->InitializeBody(map->instance_size(), Smi::FromInt(0));
  result->set_handler(*handler);
  result->set_hash(*undefined_value(), SKIP_WRITE_BARRIER);
  return result;
}


Handle<JSProxy> Factory::NewJSFunctionProxy(Handle<Object> handler,
                                            Handle<Object> call_trap,
                                            Handle<Object> construct_trap,
                                            Handle<Object> prototype) {
  // Allocate map.
  // TODO(rossberg): Once we optimize proxies, think about a scheme to share
  // maps. Will probably depend on the identity of the handler object, too.
  Handle<Map> map = NewMap(JS_FUNCTION_PROXY_TYPE, JSFunctionProxy::kSize);
  map->set_prototype(*prototype);

  // Allocate the proxy object.
  Handle<JSFunctionProxy> result = New<JSFunctionProxy>(map, NEW_SPACE);
  result->InitializeBody(map->instance_size(), Smi::FromInt(0));
  result->set_handler(*handler);
  result->set_hash(*undefined_value(), SKIP_WRITE_BARRIER);
  result->set_call_trap(*call_trap);
  result->set_construct_trap(*construct_trap);
  return result;
}


void Factory::ReinitializeJSProxy(Handle<JSProxy> proxy, InstanceType type,
                                  int size) {
  DCHECK(type == JS_OBJECT_TYPE || type == JS_FUNCTION_TYPE);

  // Allocate fresh map.
  // TODO(rossberg): Once we optimize proxies, cache these maps.
  Handle<Map> map = NewMap(type, size);

  // Check that the receiver has at least the size of the fresh object.
  int size_difference = proxy->map()->instance_size() - map->instance_size();
  DCHECK(size_difference >= 0);

  map->set_prototype(proxy->map()->prototype());

  // Allocate the backing storage for the properties.
  int prop_size = map->InitialPropertiesLength();
  Handle<FixedArray> properties = NewFixedArray(prop_size, TENURED);

  Heap* heap = isolate()->heap();
  MaybeHandle<SharedFunctionInfo> shared;
  if (type == JS_FUNCTION_TYPE) {
    OneByteStringKey key(STATIC_CHAR_VECTOR("<freezing call trap>"),
                         heap->HashSeed());
    Handle<String> name = InternalizeStringWithKey(&key);
    shared = NewSharedFunctionInfo(name, MaybeHandle<Code>());
  }

  // In order to keep heap in consistent state there must be no allocations
  // before object re-initialization is finished and filler object is installed.
  DisallowHeapAllocation no_allocation;

  // Put in filler if the new object is smaller than the old.
  if (size_difference > 0) {
    Address address = proxy->address();
    heap->CreateFillerObjectAt(address + map->instance_size(), size_difference);
    heap->AdjustLiveBytes(address, -size_difference, Heap::FROM_MUTATOR);
  }

  // Reset the map for the object.
  proxy->synchronized_set_map(*map);
  Handle<JSObject> jsobj = Handle<JSObject>::cast(proxy);

  // Reinitialize the object from the constructor map.
  heap->InitializeJSObjectFromMap(*jsobj, *properties, *map);

  // The current native context is used to set up certain bits.
  // TODO(adamk): Using the current context seems wrong, it should be whatever
  // context the JSProxy originated in. But that context isn't stored anywhere.
  Handle<Context> context(isolate()->native_context());

  // Functions require some minimal initialization.
  if (type == JS_FUNCTION_TYPE) {
    map->set_function_with_prototype(true);
    Handle<JSFunction> js_function = Handle<JSFunction>::cast(proxy);
    InitializeFunction(js_function, shared.ToHandleChecked(), context);
  } else {
    // Provide JSObjects with a constructor.
    map->set_constructor(context->object_function());
  }
}


void Factory::ReinitializeJSGlobalProxy(Handle<JSGlobalProxy> object,
                                        Handle<JSFunction> constructor) {
  DCHECK(constructor->has_initial_map());
  Handle<Map> map(constructor->initial_map(), isolate());

  // The proxy's hash should be retained across reinitialization.
  Handle<Object> hash(object->hash(), isolate());

  // Check that the already allocated object has the same size and type as
  // objects allocated using the constructor.
  DCHECK(map->instance_size() == object->map()->instance_size());
  DCHECK(map->instance_type() == object->map()->instance_type());

  // Allocate the backing storage for the properties.
  int prop_size = map->InitialPropertiesLength();
  Handle<FixedArray> properties = NewFixedArray(prop_size, TENURED);

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


void Factory::BecomeJSObject(Handle<JSProxy> proxy) {
  ReinitializeJSProxy(proxy, JS_OBJECT_TYPE, JSObject::kHeaderSize);
}


void Factory::BecomeJSFunction(Handle<JSProxy> proxy) {
  ReinitializeJSProxy(proxy, JS_FUNCTION_TYPE, JSFunction::kSize);
}


Handle<TypeFeedbackVector> Factory::NewTypeFeedbackVector(int slot_count) {
  // Ensure we can skip the write barrier
  DCHECK_EQ(isolate()->heap()->uninitialized_symbol(),
            *TypeFeedbackVector::UninitializedSentinel(isolate()));

  if (slot_count == 0) {
    return Handle<TypeFeedbackVector>::cast(empty_fixed_array());
  }

  CALL_HEAP_FUNCTION(isolate(),
                     isolate()->heap()->AllocateFixedArrayWithFiller(
                         slot_count, TENURED,
                         *TypeFeedbackVector::UninitializedSentinel(isolate())),
                     TypeFeedbackVector);
}


Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfo(
    Handle<String> name, int number_of_literals, FunctionKind kind,
    Handle<Code> code, Handle<ScopeInfo> scope_info,
    Handle<TypeFeedbackVector> feedback_vector) {
  DCHECK(IsValidFunctionKind(kind));
  Handle<SharedFunctionInfo> shared = NewSharedFunctionInfo(name, code);
  shared->set_scope_info(*scope_info);
  shared->set_feedback_vector(*feedback_vector);
  shared->set_kind(kind);
  int literals_array_size = number_of_literals;
  // If the function contains object, regexp or array literals,
  // allocate extra space for a literals array prefix containing the
  // context.
  if (number_of_literals > 0) {
    literals_array_size += JSFunction::kLiteralsPrefixSize;
  }
  shared->set_num_literals(literals_array_size);
  if (IsGeneratorFunction(kind)) {
    shared->set_instance_class_name(isolate()->heap()->Generator_string());
    shared->DisableOptimization(kGenerator);
  }
  return shared;
}


Handle<JSMessageObject> Factory::NewJSMessageObject(
    Handle<String> type,
    Handle<JSArray> arguments,
    int start_position,
    int end_position,
    Handle<Object> script,
    Handle<Object> stack_frames) {
  Handle<Map> map = message_object_map();
  Handle<JSMessageObject> message = New<JSMessageObject>(map, NEW_SPACE);
  message->set_properties(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message->initialize_elements();
  message->set_elements(*empty_fixed_array(), SKIP_WRITE_BARRIER);
  message->set_type(*type);
  message->set_arguments(*arguments);
  message->set_start_position(start_position);
  message->set_end_position(end_position);
  message->set_script(*script);
  message->set_stack_frames(*stack_frames);
  return message;
}


Handle<SharedFunctionInfo> Factory::NewSharedFunctionInfo(
    Handle<String> name,
    MaybeHandle<Code> maybe_code) {
  Handle<Map> map = shared_function_info_map();
  Handle<SharedFunctionInfo> share = New<SharedFunctionInfo>(map,
                                                             OLD_POINTER_SPACE);

  // Set pointer fields.
  share->set_name(*name);
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    code = handle(isolate()->builtins()->builtin(Builtins::kIllegal));
  }
  share->set_code(*code);
  share->set_optimized_code_map(Smi::FromInt(0));
  share->set_scope_info(ScopeInfo::Empty(isolate()));
  Code* construct_stub =
      isolate()->builtins()->builtin(Builtins::kJSConstructStubGeneric);
  share->set_construct_stub(construct_stub);
  share->set_instance_class_name(*Object_string());
  share->set_function_data(*undefined_value(), SKIP_WRITE_BARRIER);
  share->set_script(*undefined_value(), SKIP_WRITE_BARRIER);
  share->set_debug_info(*undefined_value(), SKIP_WRITE_BARRIER);
  share->set_inferred_name(*empty_string(), SKIP_WRITE_BARRIER);
  Handle<TypeFeedbackVector> feedback_vector = NewTypeFeedbackVector(0);
  share->set_feedback_vector(*feedback_vector, SKIP_WRITE_BARRIER);
  share->set_profiler_ticks(0);
  share->set_ast_node_count(0);
  share->set_counters(0);

  // Set integer fields (smi or int, depending on the architecture).
  share->set_length(0);
  share->set_formal_parameter_count(0);
  share->set_expected_nof_properties(0);
  share->set_num_literals(0);
  share->set_start_position_and_type(0);
  share->set_end_position(0);
  share->set_function_token_position(0);
  // All compiler hints default to false or 0.
  share->set_compiler_hints(0);
  share->set_opt_count_and_bailout_reason(0);

  return share;
}


static inline int NumberCacheHash(Handle<FixedArray> cache,
                                  Handle<Object> number) {
  int mask = (cache->length() >> 1) - 1;
  if (number->IsSmi()) {
    return Handle<Smi>::cast(number)->value() & mask;
  } else {
    DoubleRepresentation rep(number->Number());
    return
        (static_cast<int>(rep.bits) ^ static_cast<int>(rep.bits >> 32)) & mask;
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
      // The first time we have a hash collision, we move to the full sized
      // number string cache.  The idea is to have a small number string
      // cache in the snapshot to keep  boot-time memory usage down.
      // If we expand the number string cache already while creating
      // the snapshot then that didn't work out.
      DCHECK(!isolate()->serializer_enabled() || FLAG_extra_code != NULL);
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
    if (!cached->IsUndefined()) return Handle<String>::cast(cached);
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
  // Get the original code of the function.
  Handle<Code> code(shared->code());

  // Create a copy of the code before allocating the debug info object to avoid
  // allocation while setting up the debug info object.
  Handle<Code> original_code(*Factory::CopyCode(code));

  // Allocate initial fixed array for active break points before allocating the
  // debug info object to avoid allocation while setting up the debug info
  // object.
  Handle<FixedArray> break_points(
      NewFixedArray(DebugInfo::kEstimatedNofBreakPointsInFunction));

  // Create and set up the debug info object. Debug info contains function, a
  // copy of the original code, the executing code and initial fixed array for
  // active break points.
  Handle<DebugInfo> debug_info =
      Handle<DebugInfo>::cast(NewStruct(DEBUG_INFO_TYPE));
  debug_info->set_shared(*shared);
  debug_info->set_original_code(*original_code);
  debug_info->set_code(*code);
  debug_info->set_break_points(*break_points);

  // Link debug info to function.
  shared->set_debug_info(*debug_info);

  return debug_info;
}


Handle<JSObject> Factory::NewArgumentsObject(Handle<JSFunction> callee,
                                             int length) {
  bool strict_mode_callee = callee->shared()->strict_mode() == STRICT;
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


Handle<JSFunction> Factory::CreateApiFunction(
    Handle<FunctionTemplateInfo> obj,
    Handle<Object> prototype,
    ApiInstanceType instance_type) {
  Handle<Code> code = isolate()->builtins()->HandleApiCall();
  Handle<Code> construct_stub = isolate()->builtins()->JSConstructStubApi();

  Handle<JSFunction> result;
  if (obj->remove_prototype()) {
    result = NewFunctionWithoutPrototype(empty_string(), code);
  } else {
    int internal_field_count = 0;
    if (!obj->instance_template()->IsUndefined()) {
      Handle<ObjectTemplateInfo> instance_template =
          Handle<ObjectTemplateInfo>(
              ObjectTemplateInfo::cast(obj->instance_template()));
      internal_field_count =
          Smi::cast(instance_template->internal_field_count())->value();
    }

    // TODO(svenpanne) Kill ApiInstanceType and refactor things by generalizing
    // JSObject::GetHeaderSize.
    int instance_size = kPointerSize * internal_field_count;
    InstanceType type;
    switch (instance_type) {
      case JavaScriptObjectType:
        type = JS_OBJECT_TYPE;
        instance_size += JSObject::kHeaderSize;
        break;
      case GlobalObjectType:
        type = JS_GLOBAL_OBJECT_TYPE;
        instance_size += JSGlobalObject::kSize;
        break;
      case GlobalProxyType:
        type = JS_GLOBAL_PROXY_TYPE;
        instance_size += JSGlobalProxy::kSize;
        break;
      default:
        UNREACHABLE();
        type = JS_OBJECT_TYPE;  // Keep the compiler happy.
        break;
    }

    result = NewFunction(empty_string(), code, prototype, type,
                         instance_size, obj->read_only_prototype());
  }

  result->shared()->set_length(obj->length());
  Handle<Object> class_name(obj->class_name(), isolate());
  if (class_name->IsString()) {
    result->shared()->set_instance_class_name(*class_name);
    result->shared()->set_name(*class_name);
  }
  result->shared()->set_function_data(*obj);
  result->shared()->set_construct_stub(*construct_stub);
  result->shared()->DontAdaptArguments();

  if (obj->remove_prototype()) {
    DCHECK(result->shared()->IsApiFunction());
    DCHECK(!result->has_initial_map());
    DCHECK(!result->has_prototype());
    return result;
  }

  if (prototype->IsTheHole()) {
#ifdef DEBUG
    LookupIterator it(handle(JSObject::cast(result->prototype())),
                      constructor_string(),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    MaybeHandle<Object> maybe_prop = Object::GetProperty(&it);
    DCHECK(it.IsFound());
    DCHECK(maybe_prop.ToHandleChecked().is_identical_to(result));
#endif
  } else {
    JSObject::AddProperty(handle(JSObject::cast(result->prototype())),
                          constructor_string(), result, DONT_ENUM);
  }

  // Down from here is only valid for API functions that can be used as a
  // constructor (don't set the "remove prototype" flag).

  Handle<Map> map(result->initial_map());

  // Mark as undetectable if needed.
  if (obj->undetectable()) {
    map->set_is_undetectable();
  }

  // Mark as hidden for the __proto__ accessor if needed.
  if (obj->hidden_prototype()) {
    map->set_is_hidden_prototype();
  }

  // Mark as needs_access_check if needed.
  if (obj->needs_access_check()) {
    map->set_is_access_check_needed(true);
  }

  // Set interceptor information in the map.
  if (!obj->named_property_handler()->IsUndefined()) {
    map->set_has_named_interceptor();
  }
  if (!obj->indexed_property_handler()->IsUndefined()) {
    map->set_has_indexed_interceptor();
  }

  // Set instance call-as-function information in the map.
  if (!obj->instance_call_handler()->IsUndefined()) {
    map->set_has_instance_call_handler();
  }

  // Recursively copy parent instance templates' accessors,
  // 'data' may be modified.
  int max_number_of_additional_properties = 0;
  int max_number_of_static_properties = 0;
  FunctionTemplateInfo* info = *obj;
  while (true) {
    if (!info->instance_template()->IsUndefined()) {
      Object* props =
          ObjectTemplateInfo::cast(
              info->instance_template())->property_accessors();
      if (!props->IsUndefined()) {
        Handle<Object> props_handle(props, isolate());
        NeanderArray props_array(props_handle);
        max_number_of_additional_properties += props_array.length();
      }
    }
    if (!info->property_accessors()->IsUndefined()) {
      Object* props = info->property_accessors();
      if (!props->IsUndefined()) {
        Handle<Object> props_handle(props, isolate());
        NeanderArray props_array(props_handle);
        max_number_of_static_properties += props_array.length();
      }
    }
    Object* parent = info->parent_template();
    if (parent->IsUndefined()) break;
    info = FunctionTemplateInfo::cast(parent);
  }

  Map::EnsureDescriptorSlack(map, max_number_of_additional_properties);

  // Use a temporary FixedArray to acculumate static accessors
  int valid_descriptors = 0;
  Handle<FixedArray> array;
  if (max_number_of_static_properties > 0) {
    array = NewFixedArray(max_number_of_static_properties);
  }

  while (true) {
    // Install instance descriptors
    if (!obj->instance_template()->IsUndefined()) {
      Handle<ObjectTemplateInfo> instance =
          Handle<ObjectTemplateInfo>(
              ObjectTemplateInfo::cast(obj->instance_template()), isolate());
      Handle<Object> props = Handle<Object>(instance->property_accessors(),
                                            isolate());
      if (!props->IsUndefined()) {
        Map::AppendCallbackDescriptors(map, props);
      }
    }
    // Accumulate static accessors
    if (!obj->property_accessors()->IsUndefined()) {
      Handle<Object> props = Handle<Object>(obj->property_accessors(),
                                            isolate());
      valid_descriptors =
          AccessorInfo::AppendUnique(props, array, valid_descriptors);
    }
    // Climb parent chain
    Handle<Object> parent = Handle<Object>(obj->parent_template(), isolate());
    if (parent->IsUndefined()) break;
    obj = Handle<FunctionTemplateInfo>::cast(parent);
  }

  // Install accumulated static accessors
  for (int i = 0; i < valid_descriptors; i++) {
    Handle<AccessorInfo> accessor(AccessorInfo::cast(array->get(i)));
    JSObject::SetAccessor(result, accessor).Assert();
  }

  DCHECK(result->shared()->IsApiFunction());
  return result;
}


Handle<MapCache> Factory::AddToMapCache(Handle<Context> context,
                                        Handle<FixedArray> keys,
                                        Handle<Map> map) {
  Handle<MapCache> map_cache = handle(MapCache::cast(context->map_cache()));
  Handle<MapCache> result = MapCache::Put(map_cache, keys, map);
  context->set_map_cache(*result);
  return result;
}


Handle<Map> Factory::ObjectLiteralMapFromCache(Handle<Context> context,
                                               Handle<FixedArray> keys) {
  if (context->map_cache()->IsUndefined()) {
    // Allocate the new map cache for the native context.
    Handle<MapCache> new_cache = MapCache::New(isolate(), 24);
    context->set_map_cache(*new_cache);
  }
  // Check to see whether there is a matching element in the cache.
  Handle<MapCache> cache =
      Handle<MapCache>(MapCache::cast(context->map_cache()));
  Handle<Object> result = Handle<Object>(cache->Lookup(*keys), isolate());
  if (result->IsMap()) return Handle<Map>::cast(result);
  int length = keys->length();
  // Create a new map and add it to the cache. Reuse the initial map of the
  // Object function if the literal has no predeclared properties.
  Handle<Map> map = length == 0
                        ? handle(context->object_function()->initial_map())
                        : Map::Create(isolate(), length);
  AddToMapCache(context, keys, map);
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
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags.value()));
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
  store->set(JSRegExp::kFlagsIndex, Smi::FromInt(flags.value()));
  store->set(JSRegExp::kIrregexpLatin1CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeIndex, uninitialized);
  store->set(JSRegExp::kIrregexpLatin1CodeSavedIndex, uninitialized);
  store->set(JSRegExp::kIrregexpUC16CodeSavedIndex, uninitialized);
  store->set(JSRegExp::kIrregexpMaxRegisterCountIndex, Smi::FromInt(0));
  store->set(JSRegExp::kIrregexpCaptureCountIndex,
             Smi::FromInt(capture_count));
  regexp->set_data(*store);
}



MaybeHandle<FunctionTemplateInfo> Factory::ConfigureInstance(
    Handle<FunctionTemplateInfo> desc, Handle<JSObject> instance) {
  // Configure the instance by adding the properties specified by the
  // instance template.
  Handle<Object> instance_template(desc->instance_template(), isolate());
  if (!instance_template->IsUndefined()) {
      RETURN_ON_EXCEPTION(
          isolate(),
          Execution::ConfigureInstance(isolate(), instance, instance_template),
          FunctionTemplateInfo);
  }
  return desc;
}


Handle<Object> Factory::GlobalConstantFor(Handle<String> name) {
  if (String::Equals(name, undefined_string())) return undefined_value();
  if (String::Equals(name, nan_string())) return nan_value();
  if (String::Equals(name, infinity_string())) return infinity_value();
  return Handle<Object>::null();
}


Handle<Object> Factory::ToBoolean(bool value) {
  return value ? true_value() : false_value();
}


} }  // namespace v8::internal
