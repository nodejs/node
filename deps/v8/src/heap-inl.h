// Copyright 2006-2010 the V8 project authors. All rights reserved.
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

#ifndef V8_HEAP_INL_H_
#define V8_HEAP_INL_H_

#include "heap.h"
#include "objects.h"
#include "v8-counters.h"

namespace v8 {
namespace internal {

int Heap::MaxObjectSizeInPagedSpace() {
  return Page::kMaxHeapObjectSize;
}


MaybeObject* Heap::AllocateStringFromUtf8(Vector<const char> str,
                                          PretenureFlag pretenure) {
  // Check for ASCII first since this is the common case.
  if (String::IsAscii(str.start(), str.length())) {
    // If the string is ASCII, we do not need to convert the characters
    // since UTF8 is backwards compatible with ASCII.
    return AllocateStringFromAscii(str, pretenure);
  }
  // Non-ASCII and we need to decode.
  return AllocateStringFromUtf8Slow(str, pretenure);
}


MaybeObject* Heap::AllocateSymbol(Vector<const char> str,
                                  int chars,
                                  uint32_t hash_field) {
  unibrow::Utf8InputBuffer<> buffer(str.start(),
                                    static_cast<unsigned>(str.length()));
  return AllocateInternalSymbol(&buffer, chars, hash_field);
}


MaybeObject* Heap::AllocateAsciiSymbol(Vector<const char> str,
                                       uint32_t hash_field) {
  if (str.length() > SeqAsciiString::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  // Compute map and object size.
  Map* map = ascii_symbol_map();
  int size = SeqAsciiString::SizeFor(str.length());

  // Allocate string.
  Object* result;
  { MaybeObject* maybe_result = (size > MaxObjectSizeInPagedSpace())
                   ? lo_space_->AllocateRaw(size)
                   : old_data_space_->AllocateRaw(size);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<HeapObject*>(result)->set_map(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  ASSERT_EQ(size, answer->Size());

  // Fill in the characters.
  memcpy(answer->address() + SeqAsciiString::kHeaderSize,
         str.start(), str.length());

  return answer;
}


MaybeObject* Heap::AllocateTwoByteSymbol(Vector<const uc16> str,
                                         uint32_t hash_field) {
  if (str.length() > SeqTwoByteString::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  // Compute map and object size.
  Map* map = symbol_map();
  int size = SeqTwoByteString::SizeFor(str.length());

  // Allocate string.
  Object* result;
  { MaybeObject* maybe_result = (size > MaxObjectSizeInPagedSpace())
                   ? lo_space_->AllocateRaw(size)
                   : old_data_space_->AllocateRaw(size);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<HeapObject*>(result)->set_map(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  ASSERT_EQ(size, answer->Size());

  // Fill in the characters.
  memcpy(answer->address() + SeqTwoByteString::kHeaderSize,
         str.start(), str.length() * kUC16Size);

  return answer;
}

MaybeObject* Heap::CopyFixedArray(FixedArray* src) {
  return CopyFixedArrayWithMap(src, src->map());
}


MaybeObject* Heap::AllocateRaw(int size_in_bytes,
                               AllocationSpace space,
                               AllocationSpace retry_space) {
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
  ASSERT(space != NEW_SPACE ||
         retry_space == OLD_POINTER_SPACE ||
         retry_space == OLD_DATA_SPACE ||
         retry_space == LO_SPACE);
#ifdef DEBUG
  if (FLAG_gc_interval >= 0 &&
      !disallow_allocation_failure_ &&
      Heap::allocation_timeout_-- <= 0) {
    return Failure::RetryAfterGC(space);
  }
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  MaybeObject* result;
  if (NEW_SPACE == space) {
    result = new_space_.AllocateRaw(size_in_bytes);
    if (always_allocate() && result->IsFailure()) {
      space = retry_space;
    } else {
      return result;
    }
  }

  if (OLD_POINTER_SPACE == space) {
    result = old_pointer_space_->AllocateRaw(size_in_bytes);
  } else if (OLD_DATA_SPACE == space) {
    result = old_data_space_->AllocateRaw(size_in_bytes);
  } else if (CODE_SPACE == space) {
    result = code_space_->AllocateRaw(size_in_bytes);
  } else if (LO_SPACE == space) {
    result = lo_space_->AllocateRaw(size_in_bytes);
  } else if (CELL_SPACE == space) {
    result = cell_space_->AllocateRaw(size_in_bytes);
  } else {
    ASSERT(MAP_SPACE == space);
    result = map_space_->AllocateRaw(size_in_bytes);
  }
  if (result->IsFailure()) old_gen_exhausted_ = true;
  return result;
}


MaybeObject* Heap::NumberFromInt32(int32_t value) {
  if (Smi::IsValid(value)) return Smi::FromInt(value);
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastI2D(value));
}


MaybeObject* Heap::NumberFromUint32(uint32_t value) {
  if ((int32_t)value >= 0 && Smi::IsValid((int32_t)value)) {
    return Smi::FromInt((int32_t)value);
  }
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastUI2D(value));
}


void Heap::FinalizeExternalString(String* string) {
  ASSERT(string->IsExternalString());
  v8::String::ExternalStringResourceBase** resource_addr =
      reinterpret_cast<v8::String::ExternalStringResourceBase**>(
          reinterpret_cast<byte*>(string) +
          ExternalString::kResourceOffset -
          kHeapObjectTag);

  // Dispose of the C++ object if it has not already been disposed.
  if (*resource_addr != NULL) {
    (*resource_addr)->Dispose();
  }

  // Clear the resource pointer in the string.
  *resource_addr = NULL;
}


MaybeObject* Heap::AllocateRawMap() {
#ifdef DEBUG
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  MaybeObject* result = map_space_->AllocateRaw(Map::kSize);
  if (result->IsFailure()) old_gen_exhausted_ = true;
#ifdef DEBUG
  if (!result->IsFailure()) {
    // Maps have their own alignment.
    CHECK((reinterpret_cast<intptr_t>(result) & kMapAlignmentMask) ==
          static_cast<intptr_t>(kHeapObjectTag));
  }
#endif
  return result;
}


MaybeObject* Heap::AllocateRawCell() {
#ifdef DEBUG
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  MaybeObject* result = cell_space_->AllocateRaw(JSGlobalPropertyCell::kSize);
  if (result->IsFailure()) old_gen_exhausted_ = true;
  return result;
}


bool Heap::InNewSpace(Object* object) {
  bool result = new_space_.Contains(object);
  ASSERT(!result ||                  // Either not in new space
         gc_state_ != NOT_IN_GC ||   // ... or in the middle of GC
         InToSpace(object));         // ... or in to-space (where we allocate).
  return result;
}


bool Heap::InFromSpace(Object* object) {
  return new_space_.FromSpaceContains(object);
}


bool Heap::InToSpace(Object* object) {
  return new_space_.ToSpaceContains(object);
}


bool Heap::ShouldBePromoted(Address old_address, int object_size) {
  // An object should be promoted if:
  // - the object has survived a scavenge operation or
  // - to space is already 25% full.
  return old_address < new_space_.age_mark()
      || (new_space_.Size() + object_size) >= (new_space_.Capacity() >> 2);
}


void Heap::RecordWrite(Address address, int offset) {
  if (new_space_.Contains(address)) return;
  ASSERT(!new_space_.FromSpaceContains(address));
  SLOW_ASSERT(Contains(address + offset));
  Page::FromAddress(address)->MarkRegionDirty(address + offset);
}


void Heap::RecordWrites(Address address, int start, int len) {
  if (new_space_.Contains(address)) return;
  ASSERT(!new_space_.FromSpaceContains(address));
  Page* page = Page::FromAddress(address);
  page->SetRegionMarks(page->GetRegionMarks() |
      page->GetRegionMaskForSpan(address + start, len * kPointerSize));
}


OldSpace* Heap::TargetSpace(HeapObject* object) {
  InstanceType type = object->map()->instance_type();
  AllocationSpace space = TargetSpaceId(type);
  return (space == OLD_POINTER_SPACE)
      ? old_pointer_space_
      : old_data_space_;
}


AllocationSpace Heap::TargetSpaceId(InstanceType type) {
  // Heap numbers and sequential strings are promoted to old data space, all
  // other object types are promoted to old pointer space.  We do not use
  // object->IsHeapNumber() and object->IsSeqString() because we already
  // know that object has the heap object tag.

  // These objects are never allocated in new space.
  ASSERT(type != MAP_TYPE);
  ASSERT(type != CODE_TYPE);
  ASSERT(type != ODDBALL_TYPE);
  ASSERT(type != JS_GLOBAL_PROPERTY_CELL_TYPE);

  if (type < FIRST_NONSTRING_TYPE) {
    // There are three string representations: sequential strings, cons
    // strings, and external strings.  Only cons strings contain
    // non-map-word pointers to heap objects.
    return ((type & kStringRepresentationMask) == kConsStringTag)
        ? OLD_POINTER_SPACE
        : OLD_DATA_SPACE;
  } else {
    return (type <= LAST_DATA_TYPE) ? OLD_DATA_SPACE : OLD_POINTER_SPACE;
  }
}


void Heap::CopyBlock(Address dst, Address src, int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));
  CopyWords(reinterpret_cast<Object**>(dst),
            reinterpret_cast<Object**>(src),
            byte_size / kPointerSize);
}


void Heap::CopyBlockToOldSpaceAndUpdateRegionMarks(Address dst,
                                                   Address src,
                                                   int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));

  Page* page = Page::FromAddress(dst);
  uint32_t marks = page->GetRegionMarks();

  for (int remaining = byte_size / kPointerSize;
       remaining > 0;
       remaining--) {
    Memory::Object_at(dst) = Memory::Object_at(src);

    if (Heap::InNewSpace(Memory::Object_at(dst))) {
      marks |= page->GetRegionMaskForAddress(dst);
    }

    dst += kPointerSize;
    src += kPointerSize;
  }

  page->SetRegionMarks(marks);
}


void Heap::MoveBlock(Address dst, Address src, int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));

  int size_in_words = byte_size / kPointerSize;

  if ((dst < src) || (dst >= (src + size_in_words))) {
    ASSERT((dst >= (src + size_in_words)) ||
           ((OffsetFrom(reinterpret_cast<Address>(src)) -
             OffsetFrom(reinterpret_cast<Address>(dst))) >= kPointerSize));

    Object** src_slot = reinterpret_cast<Object**>(src);
    Object** dst_slot = reinterpret_cast<Object**>(dst);
    Object** end_slot = src_slot + size_in_words;

    while (src_slot != end_slot) {
      *dst_slot++ = *src_slot++;
    }
  } else {
    memmove(dst, src, byte_size);
  }
}


void Heap::MoveBlockToOldSpaceAndUpdateRegionMarks(Address dst,
                                                   Address src,
                                                   int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));
  ASSERT((dst >= (src + byte_size)) ||
         ((OffsetFrom(src) - OffsetFrom(dst)) >= kPointerSize));

  CopyBlockToOldSpaceAndUpdateRegionMarks(dst, src, byte_size);
}


void Heap::ScavengeObject(HeapObject** p, HeapObject* object) {
  ASSERT(InFromSpace(object));

  // We use the first word (where the map pointer usually is) of a heap
  // object to record the forwarding pointer.  A forwarding pointer can
  // point to an old space, the code space, or the to space of the new
  // generation.
  MapWord first_word = object->map_word();

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    *p = first_word.ToForwardingAddress();
    return;
  }

  // Call the slow part of scavenge object.
  return ScavengeObjectSlow(p, object);
}


bool Heap::CollectGarbage(AllocationSpace space) {
  return CollectGarbage(space, SelectGarbageCollector(space));
}


MaybeObject* Heap::PrepareForCompare(String* str) {
  // Always flatten small strings and force flattening of long strings
  // after we have accumulated a certain amount we failed to flatten.
  static const int kMaxAlwaysFlattenLength = 32;
  static const int kFlattenLongThreshold = 16*KB;

  const int length = str->length();
  MaybeObject* obj = str->TryFlatten();
  if (length <= kMaxAlwaysFlattenLength ||
      unflattened_strings_length_ >= kFlattenLongThreshold) {
    return obj;
  }
  if (obj->IsFailure()) {
    unflattened_strings_length_ += length;
  }
  return str;
}


int Heap::AdjustAmountOfExternalAllocatedMemory(int change_in_bytes) {
  ASSERT(HasBeenSetup());
  int amount = amount_of_external_allocated_memory_ + change_in_bytes;
  if (change_in_bytes >= 0) {
    // Avoid overflow.
    if (amount > amount_of_external_allocated_memory_) {
      amount_of_external_allocated_memory_ = amount;
    }
    int amount_since_last_global_gc =
        amount_of_external_allocated_memory_ -
        amount_of_external_allocated_memory_at_last_global_gc_;
    if (amount_since_last_global_gc > external_allocation_limit_) {
      CollectAllGarbage(false);
    }
  } else {
    // Avoid underflow.
    if (amount >= 0) {
      amount_of_external_allocated_memory_ = amount;
    }
  }
  ASSERT(amount_of_external_allocated_memory_ >= 0);
  return amount_of_external_allocated_memory_;
}


void Heap::SetLastScriptId(Object* last_script_id) {
  roots_[kLastScriptIdRootIndex] = last_script_id;
}


#ifdef DEBUG
#define GC_GREEDY_CHECK() \
  if (FLAG_gc_greedy) v8::internal::Heap::GarbageCollectionGreedyCheck()
#else
#define GC_GREEDY_CHECK() { }
#endif


// Calls the FUNCTION_CALL function and retries it up to three times
// to guarantee that any allocations performed during the call will
// succeed if there's enough memory.

// Warning: Do not use the identifiers __object__, __maybe_object__ or
// __scope__ in a call to this macro.

#define CALL_AND_RETRY(FUNCTION_CALL, RETURN_VALUE, RETURN_EMPTY)         \
  do {                                                                    \
    GC_GREEDY_CHECK();                                                    \
    MaybeObject* __maybe_object__ = FUNCTION_CALL;                        \
    Object* __object__ = NULL;                                            \
    if (__maybe_object__->ToObject(&__object__)) RETURN_VALUE;            \
    if (__maybe_object__->IsOutOfMemory()) {                              \
      v8::internal::V8::FatalProcessOutOfMemory("CALL_AND_RETRY_0", true);\
    }                                                                     \
    if (!__maybe_object__->IsRetryAfterGC()) RETURN_EMPTY;                \
    Heap::CollectGarbage(                                                 \
        Failure::cast(__maybe_object__)->allocation_space());             \
    __maybe_object__ = FUNCTION_CALL;                                     \
    if (__maybe_object__->ToObject(&__object__)) RETURN_VALUE;            \
    if (__maybe_object__->IsOutOfMemory()) {                              \
      v8::internal::V8::FatalProcessOutOfMemory("CALL_AND_RETRY_1", true);\
    }                                                                     \
    if (!__maybe_object__->IsRetryAfterGC()) RETURN_EMPTY;                \
    Counters::gc_last_resort_from_handles.Increment();                    \
    Heap::CollectAllAvailableGarbage();                                   \
    {                                                                     \
      AlwaysAllocateScope __scope__;                                      \
      __maybe_object__ = FUNCTION_CALL;                                   \
    }                                                                     \
    if (__maybe_object__->ToObject(&__object__)) RETURN_VALUE;            \
    if (__maybe_object__->IsOutOfMemory() ||                              \
        __maybe_object__->IsRetryAfterGC()) {                             \
      /* TODO(1181417): Fix this. */                                      \
      v8::internal::V8::FatalProcessOutOfMemory("CALL_AND_RETRY_2", true);\
    }                                                                     \
    RETURN_EMPTY;                                                         \
  } while (false)


#define CALL_HEAP_FUNCTION(FUNCTION_CALL, TYPE)                \
  CALL_AND_RETRY(FUNCTION_CALL,                                \
                 return Handle<TYPE>(TYPE::cast(__object__)),  \
                 return Handle<TYPE>())


#define CALL_HEAP_FUNCTION_VOID(FUNCTION_CALL) \
  CALL_AND_RETRY(FUNCTION_CALL, return, return)


#ifdef DEBUG

inline bool Heap::allow_allocation(bool new_state) {
  bool old = allocation_allowed_;
  allocation_allowed_ = new_state;
  return old;
}

#endif


void ExternalStringTable::AddString(String* string) {
  ASSERT(string->IsExternalString());
  if (Heap::InNewSpace(string)) {
    new_space_strings_.Add(string);
  } else {
    old_space_strings_.Add(string);
  }
}


void ExternalStringTable::Iterate(ObjectVisitor* v) {
  if (!new_space_strings_.is_empty()) {
    Object** start = &new_space_strings_[0];
    v->VisitPointers(start, start + new_space_strings_.length());
  }
  if (!old_space_strings_.is_empty()) {
    Object** start = &old_space_strings_[0];
    v->VisitPointers(start, start + old_space_strings_.length());
  }
}


// Verify() is inline to avoid ifdef-s around its calls in release
// mode.
void ExternalStringTable::Verify() {
#ifdef DEBUG
  for (int i = 0; i < new_space_strings_.length(); ++i) {
    ASSERT(Heap::InNewSpace(new_space_strings_[i]));
    ASSERT(new_space_strings_[i] != Heap::raw_unchecked_null_value());
  }
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    ASSERT(!Heap::InNewSpace(old_space_strings_[i]));
    ASSERT(old_space_strings_[i] != Heap::raw_unchecked_null_value());
  }
#endif
}


void ExternalStringTable::AddOldString(String* string) {
  ASSERT(string->IsExternalString());
  ASSERT(!Heap::InNewSpace(string));
  old_space_strings_.Add(string);
}


void ExternalStringTable::ShrinkNewStrings(int position) {
  new_space_strings_.Rewind(position);
  Verify();
}

} }  // namespace v8::internal

#endif  // V8_HEAP_INL_H_
