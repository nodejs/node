// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/string.h"

#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/isolate-utils.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/numbers/conversions.h"
#include "src/objects/map.h"
#include "src/objects/oddball.h"
#include "src/objects/string-comparator.h"
#include "src/objects/string-inl.h"
#include "src/strings/char-predicates.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-hasher.h"
#include "src/strings/string-search.h"
#include "src/strings/string-stream.h"
#include "src/strings/unicode-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

Handle<String> String::SlowFlatten(Isolate* isolate, Handle<ConsString> cons,
                                   AllocationType allocation) {
  DCHECK_NE(cons->second().length(), 0);

  // TurboFan can create cons strings with empty first parts.
  while (cons->first().length() == 0) {
    // We do not want to call this function recursively. Therefore we call
    // String::Flatten only in those cases where String::SlowFlatten is not
    // called again.
    if (cons->second().IsConsString() && !cons->second().IsFlat()) {
      cons = handle(ConsString::cast(cons->second()), isolate);
    } else {
      return String::Flatten(isolate, handle(cons->second(), isolate));
    }
  }

  DCHECK(AllowGarbageCollection::IsAllowed());
  int length = cons->length();
  allocation =
      ObjectInYoungGeneration(*cons) ? allocation : AllocationType::kOld;
  Handle<SeqString> result;
  if (cons->IsOneByteRepresentation()) {
    Handle<SeqOneByteString> flat =
        isolate->factory()
            ->NewRawOneByteString(length, allocation)
            .ToHandleChecked();
    DisallowGarbageCollection no_gc;
    WriteToFlat(*cons, flat->GetChars(no_gc), 0, length);
    result = flat;
  } else {
    Handle<SeqTwoByteString> flat =
        isolate->factory()
            ->NewRawTwoByteString(length, allocation)
            .ToHandleChecked();
    DisallowGarbageCollection no_gc;
    WriteToFlat(*cons, flat->GetChars(no_gc), 0, length);
    result = flat;
  }
  cons->set_first(*result);
  cons->set_second(ReadOnlyRoots(isolate).empty_string());
  DCHECK(result->IsFlat());
  return result;
}

namespace {

template <class StringClass>
void MigrateExternalStringResource(Isolate* isolate, ExternalString from,
                                   StringClass to) {
  Address to_resource_address = to.resource_as_address();
  if (to_resource_address == kNullAddress) {
    StringClass cast_from = StringClass::cast(from);
    // |to| is a just-created internalized copy of |from|. Migrate the resource.
    to.SetResource(isolate, cast_from.resource());
    // Zap |from|'s resource pointer to reflect the fact that |from| has
    // relinquished ownership of its resource.
    isolate->heap()->UpdateExternalString(
        from, ExternalString::cast(from).ExternalPayloadSize(), 0);
    cast_from.SetResource(isolate, nullptr);
  } else if (to_resource_address != from.resource_as_address()) {
    // |to| already existed and has its own resource. Finalize |from|.
    isolate->heap()->FinalizeExternalString(from);
  }
}

void MigrateExternalString(Isolate* isolate, String string,
                           String internalized) {
  if (internalized.IsExternalOneByteString()) {
    MigrateExternalStringResource(isolate, ExternalString::cast(string),
                                  ExternalOneByteString::cast(internalized));
  } else if (internalized.IsExternalTwoByteString()) {
    MigrateExternalStringResource(isolate, ExternalString::cast(string),
                                  ExternalTwoByteString::cast(internalized));
  } else {
    // If the external string is duped into an existing non-external
    // internalized string, free its resource (it's about to be rewritten
    // into a ThinString below).
    isolate->heap()->FinalizeExternalString(string);
  }
}

}  // namespace

template <typename IsolateT>
void String::MakeThin(IsolateT* isolate, String internalized) {
  DisallowGarbageCollection no_gc;
  DCHECK_NE(*this, internalized);
  DCHECK(internalized.IsInternalizedString());

  if (this->IsExternalString()) {
    MigrateExternalString(isolate->AsIsolate(), *this, internalized);
  }

  bool has_pointers = StringShape(*this).IsIndirect();

  int old_size = this->Size();
  bool one_byte = internalized.IsOneByteRepresentation();
  Handle<Map> map = one_byte ? isolate->factory()->thin_one_byte_string_map()
                             : isolate->factory()->thin_string_map();
  // Update actual first and then do release store on the map word. This ensures
  // that the concurrent marker will read the pointer when visiting a
  // ThinString.
  ThinString thin = ThinString::unchecked_cast(*this);
  thin.set_actual(internalized);
  DCHECK_GE(old_size, ThinString::kSize);
  this->set_map(*map, kReleaseStore);
  Address thin_end = thin.address() + ThinString::kSize;
  int size_delta = old_size - ThinString::kSize;
  if (size_delta != 0) {
    if (!Heap::IsLargeObject(thin)) {
      isolate->heap()->CreateFillerObjectAt(
          thin_end, size_delta,
          has_pointers ? ClearRecordedSlots::kYes : ClearRecordedSlots::kNo);
    } else {
      // We don't need special handling for the combination IsLargeObject &&
      // has_pointers, because indirect strings never get that large.
      DCHECK(!has_pointers);
    }
  }
}

template void String::MakeThin(Isolate* isolate, String internalized);
template void String::MakeThin(LocalIsolate* isolate, String internalized);

bool String::MakeExternal(v8::String::ExternalStringResource* resource) {
  // Disallow garbage collection to avoid possible GC vs string access deadlock.
  DisallowGarbageCollection no_gc;

  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(this->SupportsExternalization());
  DCHECK(resource->IsCacheable());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    base::ScopedVector<base::uc16> smart_chars(this->length());
    String::WriteToFlat(*this, smart_chars.begin(), 0, this->length());
    DCHECK_EQ(0, memcmp(smart_chars.begin(), resource->data(),
                        resource->length() * sizeof(smart_chars[0])));
  }
#endif                      // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kUncachedSize) return false;
  // Read-only strings cannot be made external, since that would mutate the
  // string.
  if (IsReadOnlyHeapObject(*this)) return false;
  Isolate* isolate = GetIsolateFromWritableObject(*this);
  bool is_internalized = this->IsInternalizedString();
  bool has_pointers = StringShape(*this).IsIndirect();

  if (has_pointers) {
    isolate->heap()->NotifyObjectLayoutChange(*this, no_gc,
                                              InvalidateRecordedSlots::kYes);
  }

  base::SharedMutexGuard<base::kExclusive> shared_mutex_guard(
      isolate->internalized_string_access());
  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if the space the existing
  // string occupies is too small for a regular external string.  Instead, we
  // resort to an uncached external string instead, omitting the field caching
  // the address of the backing store.  When we encounter uncached external
  // strings in generated code, we need to bailout to runtime.
  Map new_map;
  ReadOnlyRoots roots(isolate);
  if (size < ExternalString::kSizeOfAllExternalStrings) {
    if (is_internalized) {
      new_map = roots.uncached_external_internalized_string_map();
    } else {
      new_map = roots.uncached_external_string_map();
    }
  } else {
    new_map = is_internalized ? roots.external_internalized_string_map()
                              : roots.external_string_map();
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  if (!isolate->heap()->IsLargeObject(*this)) {
    isolate->heap()->CreateFillerObjectAt(
        this->address() + new_size, size - new_size,
        has_pointers ? ClearRecordedSlots::kYes : ClearRecordedSlots::kNo);
  } else {
    // We don't need special handling for the combination IsLargeObject &&
    // has_pointers, because indirect strings never get that large.
    DCHECK(!has_pointers);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->set_map(new_map, kReleaseStore);

  ExternalTwoByteString self = ExternalTwoByteString::cast(*this);
  self.AllocateExternalPointerEntries(isolate);
  self.SetResource(isolate, resource);
  isolate->heap()->RegisterExternalString(*this);
  // Force regeneration of the hash value.
  if (is_internalized) self.EnsureHash();
  return true;
}

bool String::MakeExternal(v8::String::ExternalOneByteStringResource* resource) {
  // Disallow garbage collection to avoid possible GC vs string access deadlock.
  DisallowGarbageCollection no_gc;

  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(this->SupportsExternalization());
  DCHECK(resource->IsCacheable());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    if (this->IsTwoByteRepresentation()) {
      base::ScopedVector<uint16_t> smart_chars(this->length());
      String::WriteToFlat(*this, smart_chars.begin(), 0, this->length());
      DCHECK(String::IsOneByte(smart_chars.begin(), this->length()));
    }
    base::ScopedVector<char> smart_chars(this->length());
    String::WriteToFlat(*this, smart_chars.begin(), 0, this->length());
    DCHECK_EQ(0, memcmp(smart_chars.begin(), resource->data(),
                        resource->length() * sizeof(smart_chars[0])));
  }
#endif                      // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kUncachedSize) return false;
  // Read-only strings cannot be made external, since that would mutate the
  // string.
  if (IsReadOnlyHeapObject(*this)) return false;
  Isolate* isolate = GetIsolateFromWritableObject(*this);
  bool is_internalized = this->IsInternalizedString();
  bool has_pointers = StringShape(*this).IsIndirect();

  if (has_pointers) {
    isolate->heap()->NotifyObjectLayoutChange(*this, no_gc,
                                              InvalidateRecordedSlots::kYes);
  }

  base::SharedMutexGuard<base::kExclusive> shared_mutex_guard(
      isolate->internalized_string_access());
  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if the space the existing
  // string occupies is too small for a regular external string.  Instead, we
  // resort to an uncached external string instead, omitting the field caching
  // the address of the backing store.  When we encounter uncached external
  // strings in generated code, we need to bailout to runtime.
  Map new_map;
  ReadOnlyRoots roots(isolate);
  if (size < ExternalString::kSizeOfAllExternalStrings) {
    new_map = is_internalized
                  ? roots.uncached_external_one_byte_internalized_string_map()
                  : roots.uncached_external_one_byte_string_map();
  } else {
    new_map = is_internalized
                  ? roots.external_one_byte_internalized_string_map()
                  : roots.external_one_byte_string_map();
  }

  if (!isolate->heap()->IsLargeObject(*this)) {
    // Byte size of the external String object.
    int new_size = this->SizeFromMap(new_map);

    isolate->heap()->CreateFillerObjectAt(
        this->address() + new_size, size - new_size,
        has_pointers ? ClearRecordedSlots::kYes : ClearRecordedSlots::kNo);
  } else {
    // We don't need special handling for the combination IsLargeObject &&
    // has_pointers, because indirect strings never get that large.
    DCHECK(!has_pointers);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->set_map(new_map, kReleaseStore);

  ExternalOneByteString self = ExternalOneByteString::cast(*this);
  self.AllocateExternalPointerEntries(isolate);
  self.SetResource(isolate, resource);
  isolate->heap()->RegisterExternalString(*this);
  // Force regeneration of the hash value.
  if (is_internalized) self.EnsureHash();
  return true;
}

bool String::SupportsExternalization() {
  if (this->IsThinString()) {
    return i::ThinString::cast(*this).actual().SupportsExternalization();
  }

  // RO_SPACE strings cannot be externalized.
  if (IsReadOnlyHeapObject(*this)) {
    return false;
  }

  // Already an external string.
  if (StringShape(*this).IsExternal()) {
    return false;
  }

#ifdef V8_COMPRESS_POINTERS
  // Small strings may not be in-place externalizable.
  if (this->Size() < ExternalString::kUncachedSize) return false;
#else
  DCHECK_LE(ExternalString::kUncachedSize, this->Size());
#endif

  Isolate* isolate = GetIsolateFromWritableObject(*this);
  return !isolate->heap()->IsInGCPostProcessing();
}

const char* String::PrefixForDebugPrint() const {
  StringShape shape(*this);
  if (IsTwoByteRepresentation()) {
    StringShape shape(*this);
    if (shape.IsInternalized()) {
      return "u#";
    } else if (shape.IsCons()) {
      return "uc\"";
    } else if (shape.IsThin()) {
      return "u>\"";
    } else if (shape.IsExternal()) {
      return "ue\"";
    } else {
      return "u\"";
    }
  } else {
    StringShape shape(*this);
    if (shape.IsInternalized()) {
      return "#";
    } else if (shape.IsCons()) {
      return "c\"";
    } else if (shape.IsThin()) {
      return ">\"";
    } else if (shape.IsExternal()) {
      return "e\"";
    } else {
      return "\"";
    }
  }
  UNREACHABLE();
}

const char* String::SuffixForDebugPrint() const {
  StringShape shape(*this);
  if (shape.IsInternalized()) return "";
  return "\"";
}

void String::StringShortPrint(StringStream* accumulator) {
  if (!LooksValid()) {
    accumulator->Add("<Invalid String>");
    return;
  }

  const int len = length();
  accumulator->Add("<String[%u]: ", len);
  accumulator->Add(PrefixForDebugPrint());

  if (len > kMaxShortPrintLength) {
    accumulator->Add("...<truncated>>");
    accumulator->Add(SuffixForDebugPrint());
    accumulator->Put('>');
    return;
  }

  PrintUC16(accumulator, 0, len);
  accumulator->Add(SuffixForDebugPrint());
  accumulator->Put('>');
}

void String::PrintUC16(std::ostream& os, int start, int end) {
  if (end < 0) end = length();
  StringCharacterStream stream(*this, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    os << AsUC16(stream.GetNext());
  }
}

void String::PrintUC16(StringStream* accumulator, int start, int end) {
  if (end < 0) end = length();
  StringCharacterStream stream(*this, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    uint16_t c = stream.GetNext();
    if (c == '\n') {
      accumulator->Add("\\n");
    } else if (c == '\r') {
      accumulator->Add("\\r");
    } else if (c == '\\') {
      accumulator->Add("\\\\");
    } else if (!std::isprint(c)) {
      accumulator->Add("\\x%02x", c);
    } else {
      accumulator->Put(static_cast<char>(c));
    }
  }
}

int32_t String::ToArrayIndex(Address addr) {
  DisallowGarbageCollection no_gc;
  String key(addr);

  uint32_t index;
  if (!key.AsArrayIndex(&index)) return -1;
  if (index <= INT_MAX) return index;
  return -1;
}

bool String::LooksValid() {
  // TODO(leszeks): Maybe remove this check entirely, Heap::Contains uses
  // basically the same logic as the way we access the heap in the first place.
  // RO_SPACE objects should always be valid.
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return true;
  if (ReadOnlyHeap::Contains(*this)) return true;
  BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(*this);
  if (chunk->heap() == nullptr) return false;
  return chunk->heap()->Contains(*this);
}

namespace {

bool AreDigits(const uint8_t* s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}

int ParseDecimalInteger(const uint8_t* s, int from, int to) {
  DCHECK_LT(to - from, 10);  // Overflow is not possible.
  DCHECK(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}

}  // namespace

// static
Handle<Object> String::ToNumber(Isolate* isolate, Handle<String> subject) {
  // Flatten {subject} string first.
  subject = String::Flatten(isolate, subject);

  // Fast array index case.
  uint32_t index;
  if (subject->AsArrayIndex(&index)) {
    return isolate->factory()->NewNumberFromUint(index);
  }

  // Fast case: short integer or some sorts of junk values.
  if (subject->IsSeqOneByteString()) {
    int len = subject->length();
    if (len == 0) return handle(Smi::zero(), isolate);

    DisallowGarbageCollection no_gc;
    uint8_t const* data =
        Handle<SeqOneByteString>::cast(subject)->GetChars(no_gc);
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->factory()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit
      // or the 'I' character ('Infinity'). All of that have codes not greater
      // than '9' except 'I' and &nbsp;.
      if (data[start_pos] != 'I' && data[start_pos] != 0xA0) {
        return isolate->factory()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits
      // we know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return isolate->factory()->minus_zero_value();
        d = -d;
      } else if (!subject->HasHashCode() && len <= String::kMaxArrayIndexSize &&
                 (len == 1 || data[0] != '0')) {
        // String hash is not calculated yet but all the data are present.
        // Update the hash field to speed up sequential convertions.
        uint32_t raw_hash_field = StringHasher::MakeArrayIndexHash(d, len);
#ifdef DEBUG
        subject->EnsureHash();  // Force hash calculation.
        DCHECK_EQ(subject->raw_hash_field(), raw_hash_field);
#endif
        subject->set_raw_hash_field(raw_hash_field);
      }
      return handle(Smi::FromInt(d), isolate);
    }
  }

  // Slower case.
  int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
  return isolate->factory()->NewNumber(StringToDouble(isolate, subject, flags));
}

String::FlatContent String::GetFlatContent(
    const DisallowGarbageCollection& no_gc) {
#if DEBUG
  // Check that this method is called only from the main thread.
  {
    Isolate* isolate;
    // We don't have to check read only strings as those won't move.
    DCHECK_IMPLIES(GetIsolateFromHeapObject(*this, &isolate),
                   ThreadId::Current() == isolate->thread_id());
  }
#endif
  USE(no_gc);
  int length = this->length();
  StringShape shape(*this);
  String string = *this;
  int offset = 0;
  if (shape.representation_tag() == kConsStringTag) {
    ConsString cons = ConsString::cast(string);
    if (cons.second().length() != 0) {
      return FlatContent(no_gc);
    }
    string = cons.first();
    shape = StringShape(string);
  } else if (shape.representation_tag() == kSlicedStringTag) {
    SlicedString slice = SlicedString::cast(string);
    offset = slice.offset();
    string = slice.parent();
    shape = StringShape(string);
    DCHECK(shape.representation_tag() != kConsStringTag &&
           shape.representation_tag() != kSlicedStringTag);
  }
  if (shape.representation_tag() == kThinStringTag) {
    ThinString thin = ThinString::cast(string);
    string = thin.actual();
    shape = StringShape(string);
    DCHECK(!shape.IsCons());
    DCHECK(!shape.IsSliced());
  }
  if (shape.encoding_tag() == kOneByteStringTag) {
    const uint8_t* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqOneByteString::cast(string).GetChars(no_gc);
    } else {
      start = ExternalOneByteString::cast(string).GetChars();
    }
    return FlatContent(start + offset, length, no_gc);
  } else {
    DCHECK_EQ(shape.encoding_tag(), kTwoByteStringTag);
    const base::uc16* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqTwoByteString::cast(string).GetChars(no_gc);
    } else {
      start = ExternalTwoByteString::cast(string).GetChars();
    }
    return FlatContent(start + offset, length, no_gc);
  }
}

std::unique_ptr<char[]> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int offset, int length,
                                          int* length_return) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return std::unique_ptr<char[]>();
  }
  // Negative length means the to the end of the string.
  if (length < 0) length = kMaxInt - offset;

  // Compute the size of the UTF-8 string. Start at the specified offset.
  StringCharacterStream stream(*this, offset);
  int character_position = offset;
  int utf8_bytes = 0;
  int last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    utf8_bytes += unibrow::Utf8::Length(character, last);
    last = character;
  }

  if (length_return) {
    *length_return = utf8_bytes;
  }

  char* result = NewArray<char>(utf8_bytes + 1);

  // Convert the UTF-16 string to a UTF-8 buffer. Start at the specified offset.
  stream.Reset(*this, offset);
  character_position = offset;
  int utf8_byte_position = 0;
  last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    if (allow_nulls == DISALLOW_NULLS && character == 0) {
      character = ' ';
    }
    utf8_byte_position +=
        unibrow::Utf8::Encode(result + utf8_byte_position, character, last);
    last = character;
  }
  result[utf8_byte_position] = 0;
  return std::unique_ptr<char[]>(result);
}

std::unique_ptr<char[]> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int* length_return) {
  return ToCString(allow_nulls, robust_flag, 0, -1, length_return);
}

// static
template <typename sinkchar>
void String::WriteToFlat(String source, sinkchar* sink, int from, int to) {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(source));
  return WriteToFlat(source, sink, from, to,
                     SharedStringAccessGuardIfNeeded::NotNeeded());
}

// static
template <typename sinkchar>
void String::WriteToFlat(String source, sinkchar* sink, int from, int to,
                         const SharedStringAccessGuardIfNeeded& access_guard) {
  DisallowGarbageCollection no_gc;
  while (from < to) {
    DCHECK_LE(0, from);
    DCHECK_LE(to, source.length());
    switch (StringShape(source).full_representation_tag()) {
      case kOneByteStringTag | kExternalStringTag: {
        CopyChars(sink, ExternalOneByteString::cast(source).GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kExternalStringTag: {
        const base::uc16* data = ExternalTwoByteString::cast(source).GetChars();
        CopyChars(sink, data + from, to - from);
        return;
      }
      case kOneByteStringTag | kSeqStringTag: {
        CopyChars(
            sink,
            SeqOneByteString::cast(source).GetChars(no_gc, access_guard) + from,
            to - from);
        return;
      }
      case kTwoByteStringTag | kSeqStringTag: {
        CopyChars(
            sink,
            SeqTwoByteString::cast(source).GetChars(no_gc, access_guard) + from,
            to - from);
        return;
      }
      case kOneByteStringTag | kConsStringTag:
      case kTwoByteStringTag | kConsStringTag: {
        ConsString cons_string = ConsString::cast(source);
        String first = cons_string.first();
        int boundary = first.length();
        if (to - boundary >= boundary - from) {
          // Right hand side is longer.  Recurse over left.
          if (from < boundary) {
            WriteToFlat(first, sink, from, boundary, access_guard);
            if (from == 0 && cons_string.second() == first) {
              CopyChars(sink + boundary, sink, boundary);
              return;
            }
            sink += boundary - from;
            from = 0;
          } else {
            from -= boundary;
          }
          to -= boundary;
          source = cons_string.second();
        } else {
          // Left hand side is longer.  Recurse over right.
          if (to > boundary) {
            String second = cons_string.second();
            // When repeatedly appending to a string, we get a cons string that
            // is unbalanced to the left, a list, essentially.  We inline the
            // common case of sequential one-byte right child.
            if (to - boundary == 1) {
              sink[boundary - from] = static_cast<sinkchar>(second.Get(0));
            } else if (second.IsSeqOneByteString()) {
              CopyChars(
                  sink + boundary - from,
                  SeqOneByteString::cast(second).GetChars(no_gc, access_guard),
                  to - boundary);
            } else {
              WriteToFlat(second, sink + boundary - from, 0, to - boundary,
                          access_guard);
            }
            to = boundary;
          }
          source = first;
        }
        break;
      }
      case kOneByteStringTag | kSlicedStringTag:
      case kTwoByteStringTag | kSlicedStringTag: {
        SlicedString slice = SlicedString::cast(source);
        unsigned offset = slice.offset();
        WriteToFlat(slice.parent(), sink, from + offset, to + offset,
                    access_guard);
        return;
      }
      case kOneByteStringTag | kThinStringTag:
      case kTwoByteStringTag | kThinStringTag:
        source = ThinString::cast(source).actual();
        break;
    }
  }
  DCHECK_EQ(from, to);
}

template <typename SourceChar>
static void CalculateLineEndsImpl(std::vector<int>* line_ends,
                                  base::Vector<const SourceChar> src,
                                  bool include_ending_line) {
  const int src_len = src.length();
  for (int i = 0; i < src_len - 1; i++) {
    SourceChar current = src[i];
    SourceChar next = src[i + 1];
    if (IsLineTerminatorSequence(current, next)) line_ends->push_back(i);
  }

  if (src_len > 0 && IsLineTerminatorSequence(src[src_len - 1], 0)) {
    line_ends->push_back(src_len - 1);
  }
  if (include_ending_line) {
    // Include one character beyond the end of script. The rewriter uses that
    // position for the implicit return statement.
    line_ends->push_back(src_len);
  }
}

template <typename IsolateT>
Handle<FixedArray> String::CalculateLineEnds(IsolateT* isolate,
                                             Handle<String> src,
                                             bool include_ending_line) {
  src = Flatten(isolate, src);
  // Rough estimate of line count based on a roughly estimated average
  // length of (unpacked) code.
  int line_count_estimate = src->length() >> 4;
  std::vector<int> line_ends;
  line_ends.reserve(line_count_estimate);
  {
    DisallowGarbageCollection no_gc;  // ensure vectors stay valid.
    // Dispatch on type of strings.
    String::FlatContent content = src->GetFlatContent(no_gc);
    DCHECK(content.IsFlat());
    if (content.IsOneByte()) {
      CalculateLineEndsImpl(&line_ends, content.ToOneByteVector(),
                            include_ending_line);
    } else {
      CalculateLineEndsImpl(&line_ends, content.ToUC16Vector(),
                            include_ending_line);
    }
  }
  int line_count = static_cast<int>(line_ends.size());
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(line_count, AllocationType::kOld);
  for (int i = 0; i < line_count; i++) {
    array->set(i, Smi::FromInt(line_ends[i]));
  }
  return array;
}

template Handle<FixedArray> String::CalculateLineEnds(Isolate* isolate,
                                                      Handle<String> src,
                                                      bool include_ending_line);
template Handle<FixedArray> String::CalculateLineEnds(LocalIsolate* isolate,
                                                      Handle<String> src,
                                                      bool include_ending_line);

bool String::SlowEquals(String other) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(other));
  return SlowEquals(other, SharedStringAccessGuardIfNeeded::NotNeeded());
}

bool String::SlowEquals(
    String other, const SharedStringAccessGuardIfNeeded& access_guard) const {
  DisallowGarbageCollection no_gc;
  // Fast check: negative check with lengths.
  int len = length();
  if (len != other.length()) return false;
  if (len == 0) return true;

  // Fast check: if at least one ThinString is involved, dereference it/them
  // and restart.
  if (this->IsThinString() || other.IsThinString()) {
    if (other.IsThinString()) other = ThinString::cast(other).actual();
    if (this->IsThinString()) {
      return ThinString::cast(*this).actual().Equals(other);
    } else {
      return this->Equals(other);
    }
  }

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (HasHashCode() && other.HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (hash() != other.hash()) {
        bool found_difference = false;
        for (int i = 0; i < len; i++) {
          if (Get(i) != other.Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (hash() != other.hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (this->Get(0, access_guard) != other.Get(0, access_guard)) return false;

  if (IsSeqOneByteString() && other.IsSeqOneByteString()) {
    const uint8_t* str1 =
        SeqOneByteString::cast(*this).GetChars(no_gc, access_guard);
    const uint8_t* str2 =
        SeqOneByteString::cast(other).GetChars(no_gc, access_guard);
    return CompareCharsEqual(str1, str2, len);
  }

  StringComparator comparator;
  return comparator.Equals(*this, other, access_guard);
}

// static
bool String::SlowEquals(Isolate* isolate, Handle<String> one,
                        Handle<String> two) {
  // Fast check: negative check with lengths.
  int one_length = one->length();
  if (one_length != two->length()) return false;
  if (one_length == 0) return true;

  // Fast check: if at least one ThinString is involved, dereference it/them
  // and restart.
  if (one->IsThinString() || two->IsThinString()) {
    if (one->IsThinString())
      one = handle(ThinString::cast(*one).actual(), isolate);
    if (two->IsThinString())
      two = handle(ThinString::cast(*two).actual(), isolate);
    return String::Equals(isolate, one, two);
  }

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (one->HasHashCode() && two->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (one->hash() != two->hash()) {
        bool found_difference = false;
        for (int i = 0; i < one_length; i++) {
          if (one->Get(i) != two->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (one->hash() != two->hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (one->Get(0) != two->Get(0)) return false;

  one = String::Flatten(isolate, one);
  two = String::Flatten(isolate, two);

  DisallowGarbageCollection no_gc;
  String::FlatContent flat1 = one->GetFlatContent(no_gc);
  String::FlatContent flat2 = two->GetFlatContent(no_gc);

  if (flat1.IsOneByte() && flat2.IsOneByte()) {
    return CompareCharsEqual(flat1.ToOneByteVector().begin(),
                             flat2.ToOneByteVector().begin(), one_length);
  } else {
    for (int i = 0; i < one_length; i++) {
      if (flat1.Get(i) != flat2.Get(i)) return false;
    }
    return true;
  }
}

// static
ComparisonResult String::Compare(Isolate* isolate, Handle<String> x,
                                 Handle<String> y) {
  // A few fast case tests before we flatten.
  if (x.is_identical_to(y)) {
    return ComparisonResult::kEqual;
  } else if (y->length() == 0) {
    return x->length() == 0 ? ComparisonResult::kEqual
                            : ComparisonResult::kGreaterThan;
  } else if (x->length() == 0) {
    return ComparisonResult::kLessThan;
  }

  int const d = x->Get(0) - y->Get(0);
  if (d < 0) {
    return ComparisonResult::kLessThan;
  } else if (d > 0) {
    return ComparisonResult::kGreaterThan;
  }

  // Slow case.
  x = String::Flatten(isolate, x);
  y = String::Flatten(isolate, y);

  DisallowGarbageCollection no_gc;
  ComparisonResult result = ComparisonResult::kEqual;
  int prefix_length = x->length();
  if (y->length() < prefix_length) {
    prefix_length = y->length();
    result = ComparisonResult::kGreaterThan;
  } else if (y->length() > prefix_length) {
    result = ComparisonResult::kLessThan;
  }
  int r;
  String::FlatContent x_content = x->GetFlatContent(no_gc);
  String::FlatContent y_content = y->GetFlatContent(no_gc);
  if (x_content.IsOneByte()) {
    base::Vector<const uint8_t> x_chars = x_content.ToOneByteVector();
    if (y_content.IsOneByte()) {
      base::Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.begin(), y_chars.begin(), prefix_length);
    } else {
      base::Vector<const base::uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.begin(), y_chars.begin(), prefix_length);
    }
  } else {
    base::Vector<const base::uc16> x_chars = x_content.ToUC16Vector();
    if (y_content.IsOneByte()) {
      base::Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.begin(), y_chars.begin(), prefix_length);
    } else {
      base::Vector<const base::uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.begin(), y_chars.begin(), prefix_length);
    }
  }
  if (r < 0) {
    result = ComparisonResult::kLessThan;
  } else if (r > 0) {
    result = ComparisonResult::kGreaterThan;
  }
  return result;
}

namespace {

uint32_t ToValidIndex(String str, Object number) {
  uint32_t index = PositiveNumberToUint32(number);
  uint32_t length_value = static_cast<uint32_t>(str.length());
  if (index > length_value) return length_value;
  return index;
}

}  // namespace

Object String::IndexOf(Isolate* isolate, Handle<Object> receiver,
                       Handle<Object> search, Handle<Object> position) {
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.indexOf")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToInteger(isolate, position));

  uint32_t index = ToValidIndex(*receiver_string, *position);
  return Smi::FromInt(
      String::IndexOf(isolate, receiver_string, search_string, index));
}

namespace {

template <typename T>
int SearchString(Isolate* isolate, String::FlatContent receiver_content,
                 base::Vector<T> pat_vector, int start_index) {
  if (receiver_content.IsOneByte()) {
    return SearchString(isolate, receiver_content.ToOneByteVector(), pat_vector,
                        start_index);
  }
  return SearchString(isolate, receiver_content.ToUC16Vector(), pat_vector,
                      start_index);
}

}  // namespace

int String::IndexOf(Isolate* isolate, Handle<String> receiver,
                    Handle<String> search, int start_index) {
  DCHECK_LE(0, start_index);
  DCHECK(start_index <= receiver->length());

  uint32_t search_length = search->length();
  if (search_length == 0) return start_index;

  uint32_t receiver_length = receiver->length();
  if (start_index + search_length > receiver_length) return -1;

  receiver = String::Flatten(isolate, receiver);
  search = String::Flatten(isolate, search);

  DisallowGarbageCollection no_gc;  // ensure vectors stay valid
  // Extract flattened substrings of cons strings before getting encoding.
  String::FlatContent receiver_content = receiver->GetFlatContent(no_gc);
  String::FlatContent search_content = search->GetFlatContent(no_gc);

  // dispatch on type of strings
  if (search_content.IsOneByte()) {
    base::Vector<const uint8_t> pat_vector = search_content.ToOneByteVector();
    return SearchString<const uint8_t>(isolate, receiver_content, pat_vector,
                                       start_index);
  }
  base::Vector<const base::uc16> pat_vector = search_content.ToUC16Vector();
  return SearchString<const base::uc16>(isolate, receiver_content, pat_vector,
                                        start_index);
}

MaybeHandle<String> String::GetSubstitution(Isolate* isolate, Match* match,
                                            Handle<String> replacement,
                                            int start_index) {
  DCHECK_GE(start_index, 0);

  Factory* factory = isolate->factory();

  const int replacement_length = replacement->length();
  const int captures_length = match->CaptureCount();

  replacement = String::Flatten(isolate, replacement);

  Handle<String> dollar_string =
      factory->LookupSingleCharacterStringFromCode('$');
  int next_dollar_ix =
      String::IndexOf(isolate, replacement, dollar_string, start_index);
  if (next_dollar_ix < 0) {
    return replacement;
  }

  IncrementalStringBuilder builder(isolate);

  if (next_dollar_ix > 0) {
    builder.AppendString(factory->NewSubString(replacement, 0, next_dollar_ix));
  }

  while (true) {
    const int peek_ix = next_dollar_ix + 1;
    if (peek_ix >= replacement_length) {
      builder.AppendCharacter('$');
      return builder.Finish();
    }

    int continue_from_ix = -1;
    const uint16_t peek = replacement->Get(peek_ix);
    switch (peek) {
      case '$':  // $$
        builder.AppendCharacter('$');
        continue_from_ix = peek_ix + 1;
        break;
      case '&':  // $& - match
        builder.AppendString(match->GetMatch());
        continue_from_ix = peek_ix + 1;
        break;
      case '`':  // $` - prefix
        builder.AppendString(match->GetPrefix());
        continue_from_ix = peek_ix + 1;
        break;
      case '\'':  // $' - suffix
        builder.AppendString(match->GetSuffix());
        continue_from_ix = peek_ix + 1;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        // Valid indices are $1 .. $9, $01 .. $09 and $10 .. $99
        int scaled_index = (peek - '0');
        int advance = 1;

        if (peek_ix + 1 < replacement_length) {
          const uint16_t next_peek = replacement->Get(peek_ix + 1);
          if (next_peek >= '0' && next_peek <= '9') {
            const int new_scaled_index = scaled_index * 10 + (next_peek - '0');
            if (new_scaled_index < captures_length) {
              scaled_index = new_scaled_index;
              advance = 2;
            }
          }
        }

        if (scaled_index == 0 || scaled_index >= captures_length) {
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        bool capture_exists;
        Handle<String> capture;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, capture, match->GetCapture(scaled_index, &capture_exists),
            String);
        if (capture_exists) builder.AppendString(capture);
        continue_from_ix = peek_ix + advance;
        break;
      }
      case '<': {  // $<name> - named capture
        using CaptureState = String::Match::CaptureState;

        if (!match->HasNamedCaptures()) {
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        Handle<String> bracket_string =
            factory->LookupSingleCharacterStringFromCode('>');
        const int closing_bracket_ix =
            String::IndexOf(isolate, replacement, bracket_string, peek_ix + 1);

        if (closing_bracket_ix == -1) {
          // No closing bracket was found, treat '$<' as a string literal.
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        Handle<String> capture_name =
            factory->NewSubString(replacement, peek_ix + 1, closing_bracket_ix);
        Handle<String> capture;
        CaptureState capture_state;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, capture,
            match->GetNamedCapture(capture_name, &capture_state), String);

        if (capture_state == CaptureState::MATCHED) {
          builder.AppendString(capture);
        }

        continue_from_ix = closing_bracket_ix + 1;
        break;
      }
      default:
        builder.AppendCharacter('$');
        continue_from_ix = peek_ix;
        break;
    }

    // Go the the next $ in the replacement.
    // TODO(jgruber): Single-char lookups could be much more efficient.
    DCHECK_NE(continue_from_ix, -1);
    next_dollar_ix =
        String::IndexOf(isolate, replacement, dollar_string, continue_from_ix);

    // Return if there are no more $ characters in the replacement. If we
    // haven't reached the end, we need to append the suffix.
    if (next_dollar_ix < 0) {
      if (continue_from_ix < replacement_length) {
        builder.AppendString(factory->NewSubString(
            replacement, continue_from_ix, replacement_length));
      }
      return builder.Finish();
    }

    // Append substring between the previous and the next $ character.
    if (next_dollar_ix > continue_from_ix) {
      builder.AppendString(
          factory->NewSubString(replacement, continue_from_ix, next_dollar_ix));
    }
  }

  UNREACHABLE();
}

namespace {  // for String.Prototype.lastIndexOf

template <typename schar, typename pchar>
int StringMatchBackwards(base::Vector<const schar> subject,
                         base::Vector<const pchar> pattern, int idx) {
  int pattern_length = pattern.length();
  DCHECK_GE(pattern_length, 1);
  DCHECK(idx + pattern_length <= subject.length());

  if (sizeof(schar) == 1 && sizeof(pchar) > 1) {
    for (int i = 0; i < pattern_length; i++) {
      base::uc16 c = pattern[i];
      if (c > String::kMaxOneByteCharCode) {
        return -1;
      }
    }
  }

  pchar pattern_first_char = pattern[0];
  for (int i = idx; i >= 0; i--) {
    if (subject[i] != pattern_first_char) continue;
    int j = 1;
    while (j < pattern_length) {
      if (pattern[j] != subject[i + j]) {
        break;
      }
      j++;
    }
    if (j == pattern_length) {
      return i;
    }
  }
  return -1;
}

}  // namespace

Object String::LastIndexOf(Isolate* isolate, Handle<Object> receiver,
                           Handle<Object> search, Handle<Object> position) {
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.lastIndexOf")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToNumber(isolate, position));

  uint32_t start_index;

  if (position->IsNaN()) {
    start_index = receiver_string->length();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                       Object::ToInteger(isolate, position));
    start_index = ToValidIndex(*receiver_string, *position);
  }

  uint32_t pattern_length = search_string->length();
  uint32_t receiver_length = receiver_string->length();

  if (start_index + pattern_length > receiver_length) {
    start_index = receiver_length - pattern_length;
  }

  if (pattern_length == 0) {
    return Smi::FromInt(start_index);
  }

  receiver_string = String::Flatten(isolate, receiver_string);
  search_string = String::Flatten(isolate, search_string);

  int last_index = -1;
  DisallowGarbageCollection no_gc;  // ensure vectors stay valid

  String::FlatContent receiver_content = receiver_string->GetFlatContent(no_gc);
  String::FlatContent search_content = search_string->GetFlatContent(no_gc);

  if (search_content.IsOneByte()) {
    base::Vector<const uint8_t> pat_vector = search_content.ToOneByteVector();
    if (receiver_content.IsOneByte()) {
      last_index = StringMatchBackwards(receiver_content.ToOneByteVector(),
                                        pat_vector, start_index);
    } else {
      last_index = StringMatchBackwards(receiver_content.ToUC16Vector(),
                                        pat_vector, start_index);
    }
  } else {
    base::Vector<const base::uc16> pat_vector = search_content.ToUC16Vector();
    if (receiver_content.IsOneByte()) {
      last_index = StringMatchBackwards(receiver_content.ToOneByteVector(),
                                        pat_vector, start_index);
    } else {
      last_index = StringMatchBackwards(receiver_content.ToUC16Vector(),
                                        pat_vector, start_index);
    }
  }
  return Smi::FromInt(last_index);
}

bool String::HasOneBytePrefix(base::Vector<const char> str) {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return IsEqualToImpl<EqualityType::kPrefix>(
      str, GetPtrComprCageBase(*this),
      SharedStringAccessGuardIfNeeded::NotNeeded());
}

namespace {

template <typename Char>
uint32_t HashString(String string, size_t start, int length, uint64_t seed,
                    const SharedStringAccessGuardIfNeeded& access_guard) {
  DisallowGarbageCollection no_gc;

  if (length > String::kMaxHashCalcLength) {
    return StringHasher::GetTrivialHash(length);
  }

  std::unique_ptr<Char[]> buffer;
  const Char* chars;

  if (string.IsConsString()) {
    DCHECK_EQ(0, start);
    DCHECK(!string.IsFlat());
    buffer.reset(new Char[length]);
    String::WriteToFlat(string, buffer.get(), 0, length, access_guard);
    chars = buffer.get();
  } else {
    chars = string.GetChars<Char>(no_gc, access_guard) + start;
  }

  return StringHasher::HashSequentialString<Char>(chars, length, seed);
}

}  // namespace

uint32_t String::ComputeAndSetHash() {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return ComputeAndSetHash(SharedStringAccessGuardIfNeeded::NotNeeded());
}
uint32_t String::ComputeAndSetHash(
    const SharedStringAccessGuardIfNeeded& access_guard) {
  DisallowGarbageCollection no_gc;
  // Should only be called if hash code has not yet been computed.
  DCHECK(!HasHashCode());

  // Store the hash code in the object.
  uint64_t seed = HashSeed(GetReadOnlyRoots());
  size_t start = 0;
  String string = *this;
  if (string.IsSlicedString()) {
    SlicedString sliced = SlicedString::cast(string);
    start = sliced.offset();
    string = sliced.parent();
  }
  if (string.IsConsString() && string.IsFlat()) {
    string = ConsString::cast(string).first();
  }
  if (string.IsThinString()) {
    string = ThinString::cast(string).actual();
    if (length() == string.length()) {
      set_raw_hash_field(string.raw_hash_field());
      return hash();
    }
  }
  uint32_t raw_hash_field =
      string.IsOneByteRepresentation()
          ? HashString<uint8_t>(string, start, length(), seed, access_guard)
          : HashString<uint16_t>(string, start, length(), seed, access_guard);
  set_raw_hash_field(raw_hash_field);

  // Check the hash code is there.
  DCHECK(HasHashCode());
  uint32_t result = raw_hash_field >> kHashShift;
  DCHECK_NE(result, 0);  // Ensure that the hash value of 0 is never computed.
  return result;
}

bool String::SlowAsArrayIndex(uint32_t* index) {
  DisallowGarbageCollection no_gc;
  int length = this->length();
  if (length <= kMaxCachedArrayIndexLength) {
    EnsureHash();  // Force computation of hash code.
    uint32_t field = raw_hash_field();
    if ((field & kIsNotIntegerIndexMask) != 0) return false;
    *index = ArrayIndexValueBits::decode(field);
    return true;
  }
  if (length == 0 || length > kMaxArrayIndexSize) return false;
  StringCharacterStream stream(*this);
  return StringToIndex(&stream, index);
}

bool String::SlowAsIntegerIndex(size_t* index) {
  DisallowGarbageCollection no_gc;
  int length = this->length();
  if (length <= kMaxCachedArrayIndexLength) {
    EnsureHash();  // Force computation of hash code.
    uint32_t field = raw_hash_field();
    if ((field & kIsNotIntegerIndexMask) != 0) return false;
    *index = ArrayIndexValueBits::decode(field);
    return true;
  }
  if (length == 0 || length > kMaxIntegerIndexSize) return false;
  StringCharacterStream stream(*this);
  return StringToIndex<StringCharacterStream, size_t, kToIntegerIndex>(&stream,
                                                                       index);
}

void String::PrintOn(FILE* file) {
  int length = this->length();
  for (int i = 0; i < length; i++) {
    PrintF(file, "%c", Get(i));
  }
}

Handle<String> SeqString::Truncate(Handle<SeqString> string, int new_length) {
  if (new_length == 0) return string->GetReadOnlyRoots().empty_string_handle();

  int new_size, old_size;
  int old_length = string->length();
  if (old_length <= new_length) return string;

  if (string->IsSeqOneByteString()) {
    old_size = SeqOneByteString::SizeFor(old_length);
    new_size = SeqOneByteString::SizeFor(new_length);
  } else {
    DCHECK(string->IsSeqTwoByteString());
    old_size = SeqTwoByteString::SizeFor(old_length);
    new_size = SeqTwoByteString::SizeFor(new_length);
  }

  int delta = old_size - new_size;

  Address start_of_string = string->address();
  DCHECK(IsAligned(start_of_string, kObjectAlignment));
  DCHECK(IsAligned(start_of_string + new_size, kObjectAlignment));

  Heap* heap = Heap::FromWritableHeapObject(*string);
  if (!heap->IsLargeObject(*string)) {
    // Sizes are pointer size aligned, so that we can use filler objects
    // that are a multiple of pointer size.
    heap->CreateFillerObjectAt(start_of_string + new_size, delta,
                               ClearRecordedSlots::kNo);
  }
  // We are storing the new length using release store after creating a filler
  // for the left-over space to avoid races with the sweeper thread.
  string->set_length(new_length, kReleaseStore);

  return string;
}

void SeqOneByteString::clear_padding() {
  int data_size = SeqString::kHeaderSize + length() * kOneByteSize;
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

void SeqTwoByteString::clear_padding() {
  int data_size = SeqString::kHeaderSize + length() * base::kUC16Size;
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

uint16_t ConsString::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  DCHECK(index >= 0 && index < this->length());

  // Check for a flattened cons string
  if (second().length() == 0) {
    String left = first();
    return left.Get(index);
  }

  String string = String::cast(*this);

  while (true) {
    if (StringShape(string).IsCons()) {
      ConsString cons_string = ConsString::cast(string);
      String left = cons_string.first();
      if (left.length() > index) {
        string = left;
      } else {
        index -= left.length();
        string = cons_string.second();
      }
    } else {
      return string.Get(index, access_guard);
    }
  }

  UNREACHABLE();
}

uint16_t ThinString::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  return actual().Get(index, access_guard);
}

uint16_t SlicedString::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  return parent().Get(offset() + index, access_guard);
}

int ExternalString::ExternalPayloadSize() const {
  int length_multiplier = IsTwoByteRepresentation() ? i::kShortSize : kCharSize;
  return length() * length_multiplier;
}

FlatStringReader::FlatStringReader(Isolate* isolate, Handle<String> str)
    : Relocatable(isolate), str_(str), length_(str->length()) {
#if DEBUG
  // Check that this constructor is called only from the main thread.
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
#endif
  PostGarbageCollection();
}

void FlatStringReader::PostGarbageCollection() {
  DCHECK(str_->IsFlat());
  DisallowGarbageCollection no_gc;
  // This does not actually prevent the vector from being relocated later.
  String::FlatContent content = str_->GetFlatContent(no_gc);
  DCHECK(content.IsFlat());
  is_one_byte_ = content.IsOneByte();
  if (is_one_byte_) {
    start_ = content.ToOneByteVector().begin();
  } else {
    start_ = content.ToUC16Vector().begin();
  }
}

void ConsStringIterator::Initialize(ConsString cons_string, int offset) {
  DCHECK(!cons_string.is_null());
  root_ = cons_string;
  consumed_ = offset;
  // Force stack blown condition to trigger restart.
  depth_ = 1;
  maximum_depth_ = kStackSize + depth_;
  DCHECK(StackBlown());
}

String ConsStringIterator::Continue(int* offset_out) {
  DCHECK_NE(depth_, 0);
  DCHECK_EQ(0, *offset_out);
  bool blew_stack = StackBlown();
  String string;
  // Get the next leaf if there is one.
  if (!blew_stack) string = NextLeaf(&blew_stack);
  // Restart search from root.
  if (blew_stack) {
    DCHECK(string.is_null());
    string = Search(offset_out);
  }
  // Ensure future calls return null immediately.
  if (string.is_null()) Reset(ConsString());
  return string;
}

String ConsStringIterator::Search(int* offset_out) {
  ConsString cons_string = root_;
  // Reset the stack, pushing the root string.
  depth_ = 1;
  maximum_depth_ = 1;
  frames_[0] = cons_string;
  const int consumed = consumed_;
  int offset = 0;
  while (true) {
    // Loop until the string is found which contains the target offset.
    String string = cons_string.first();
    int length = string.length();
    int32_t type;
    if (consumed < offset + length) {
      // Target offset is in the left branch.
      // Keep going if we're still in a ConString.
      type = string.map().instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushLeft(cons_string);
        continue;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
    } else {
      // Descend right.
      // Update progress through the string.
      offset += length;
      // Keep going if we're still in a ConString.
      string = cons_string.second();
      type = string.map().instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushRight(cons_string);
        continue;
      }
      // Need this to be updated for the current string.
      length = string.length();
      // Account for the possibility of an empty right leaf.
      // This happens only if we have asked for an offset outside the string.
      if (length == 0) {
        // Reset so future operations will return null immediately.
        Reset(ConsString());
        return String();
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
      // Pop stack so next iteration is in correct place.
      Pop();
    }
    DCHECK_NE(length, 0);
    // Adjust return values and exit.
    consumed_ = offset + length;
    *offset_out = consumed - offset;
    return string;
  }
  UNREACHABLE();
}

String ConsStringIterator::NextLeaf(bool* blew_stack) {
  while (true) {
    // Tree traversal complete.
    if (depth_ == 0) {
      *blew_stack = false;
      return String();
    }
    // We've lost track of higher nodes.
    if (StackBlown()) {
      *blew_stack = true;
      return String();
    }
    // Go right.
    ConsString cons_string = frames_[OffsetForDepth(depth_ - 1)];
    String string = cons_string.second();
    int32_t type = string.map().instance_type();
    if ((type & kStringRepresentationMask) != kConsStringTag) {
      // Pop stack so next iteration is in correct place.
      Pop();
      int length = string.length();
      // Could be a flattened ConsString.
      if (length == 0) continue;
      consumed_ += length;
      return string;
    }
    cons_string = ConsString::cast(string);
    PushRight(cons_string);
    // Need to traverse all the way left.
    while (true) {
      // Continue left.
      string = cons_string.first();
      type = string.map().instance_type();
      if ((type & kStringRepresentationMask) != kConsStringTag) {
        AdjustMaximumDepth();
        int length = string.length();
        if (length == 0) break;  // Skip empty left-hand sides of ConsStrings.
        consumed_ += length;
        return string;
      }
      cons_string = ConsString::cast(string);
      PushLeft(cons_string);
    }
  }
  UNREACHABLE();
}

const byte* String::AddressOfCharacterAt(
    int start_index, const DisallowGarbageCollection& no_gc) {
  DCHECK(IsFlat());
  String subject = *this;
  if (subject.IsConsString()) {
    subject = ConsString::cast(subject).first();
  } else if (subject.IsSlicedString()) {
    start_index += SlicedString::cast(subject).offset();
    subject = SlicedString::cast(subject).parent();
  }
  if (subject.IsThinString()) {
    subject = ThinString::cast(subject).actual();
  }
  CHECK_LE(0, start_index);
  CHECK_LE(start_index, subject.length());
  if (subject.IsSeqOneByteString()) {
    return reinterpret_cast<const byte*>(
        SeqOneByteString::cast(subject).GetChars(no_gc) + start_index);
  } else if (subject.IsSeqTwoByteString()) {
    return reinterpret_cast<const byte*>(
        SeqTwoByteString::cast(subject).GetChars(no_gc) + start_index);
  } else if (subject.IsExternalOneByteString()) {
    return reinterpret_cast<const byte*>(
        ExternalOneByteString::cast(subject).GetChars() + start_index);
  } else {
    DCHECK(subject.IsExternalTwoByteString());
    return reinterpret_cast<const byte*>(
        ExternalTwoByteString::cast(subject).GetChars() + start_index);
  }
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void String::WriteToFlat(
    String source, uint16_t* sink, int from, int to);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void String::WriteToFlat(
    String source, uint8_t* sink, int from, int to);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void String::WriteToFlat(
    String source, uint16_t* sink, int from, int to,
    const SharedStringAccessGuardIfNeeded&);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void String::WriteToFlat(
    String source, uint8_t* sink, int from, int to,
    const SharedStringAccessGuardIfNeeded&);

namespace {
// Check that the constants defined in src/objects/instance-type.h coincides
// with the Torque-definition of string instance types in src/objects/string.tq.

DEFINE_TORQUE_GENERATED_STRING_INSTANCE_TYPE()

STATIC_ASSERT(kStringRepresentationMask == RepresentationBits::kMask);

STATIC_ASSERT(kStringEncodingMask == IsOneByteBit::kMask);
STATIC_ASSERT(kTwoByteStringTag == IsOneByteBit::encode(false));
STATIC_ASSERT(kOneByteStringTag == IsOneByteBit::encode(true));

STATIC_ASSERT(kUncachedExternalStringMask == IsUncachedBit::kMask);
STATIC_ASSERT(kUncachedExternalStringTag == IsUncachedBit::encode(true));

STATIC_ASSERT(kIsNotInternalizedMask == IsNotInternalizedBit::kMask);
STATIC_ASSERT(kNotInternalizedTag == IsNotInternalizedBit::encode(true));
STATIC_ASSERT(kInternalizedTag == IsNotInternalizedBit::encode(false));
}  // namespace

}  // namespace internal
}  // namespace v8
