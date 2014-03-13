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

#ifndef V8_HEAP_INL_H_
#define V8_HEAP_INL_H_

#include <cmath>

#include "heap.h"
#include "heap-profiler.h"
#include "isolate.h"
#include "list-inl.h"
#include "objects.h"
#include "platform.h"
#include "v8-counters.h"
#include "store-buffer.h"
#include "store-buffer-inl.h"

namespace v8 {
namespace internal {

void PromotionQueue::insert(HeapObject* target, int size) {
  if (emergency_stack_ != NULL) {
    emergency_stack_->Add(Entry(target, size));
    return;
  }

  if (NewSpacePage::IsAtStart(reinterpret_cast<Address>(rear_))) {
    NewSpacePage* rear_page =
        NewSpacePage::FromAddress(reinterpret_cast<Address>(rear_));
    ASSERT(!rear_page->prev_page()->is_anchor());
    rear_ = reinterpret_cast<intptr_t*>(rear_page->prev_page()->area_end());
    ActivateGuardIfOnTheSamePage();
  }

  if (guard_) {
    ASSERT(GetHeadPage() ==
           Page::FromAllocationTop(reinterpret_cast<Address>(limit_)));

    if ((rear_ - 2) < limit_) {
      RelocateQueueHead();
      emergency_stack_->Add(Entry(target, size));
      return;
    }
  }

  *(--rear_) = reinterpret_cast<intptr_t>(target);
  *(--rear_) = size;
  // Assert no overflow into live objects.
#ifdef DEBUG
  SemiSpace::AssertValidRange(target->GetIsolate()->heap()->new_space()->top(),
                              reinterpret_cast<Address>(rear_));
#endif
}


void PromotionQueue::ActivateGuardIfOnTheSamePage() {
  guard_ = guard_ ||
      heap_->new_space()->active_space()->current_page()->address() ==
      GetHeadPage()->address();
}


MaybeObject* Heap::AllocateStringFromUtf8(Vector<const char> str,
                                          PretenureFlag pretenure) {
  // Check for ASCII first since this is the common case.
  const char* start = str.start();
  int length = str.length();
  int non_ascii_start = String::NonAsciiStart(start, length);
  if (non_ascii_start >= length) {
    // If the string is ASCII, we do not need to convert the characters
    // since UTF8 is backwards compatible with ASCII.
    return AllocateStringFromOneByte(str, pretenure);
  }
  // Non-ASCII and we need to decode.
  return AllocateStringFromUtf8Slow(str, non_ascii_start, pretenure);
}


template<>
bool inline Heap::IsOneByte(Vector<const char> str, int chars) {
  // TODO(dcarney): incorporate Latin-1 check when Latin-1 is supported?
  // ASCII only check.
  return chars == str.length();
}


template<>
bool inline Heap::IsOneByte(String* str, int chars) {
  return str->IsOneByteRepresentation();
}


MaybeObject* Heap::AllocateInternalizedStringFromUtf8(
    Vector<const char> str, int chars, uint32_t hash_field) {
  if (IsOneByte(str, chars)) {
    return AllocateOneByteInternalizedString(
        Vector<const uint8_t>::cast(str), hash_field);
  }
  return AllocateInternalizedStringImpl<false>(str, chars, hash_field);
}


template<typename T>
MaybeObject* Heap::AllocateInternalizedStringImpl(
    T t, int chars, uint32_t hash_field) {
  if (IsOneByte(t, chars)) {
    return AllocateInternalizedStringImpl<true>(t, chars, hash_field);
  }
  return AllocateInternalizedStringImpl<false>(t, chars, hash_field);
}


MaybeObject* Heap::AllocateOneByteInternalizedString(Vector<const uint8_t> str,
                                                     uint32_t hash_field) {
  if (str.length() > SeqOneByteString::kMaxLength) {
    return Failure::OutOfMemoryException(0x2);
  }
  // Compute map and object size.
  Map* map = ascii_internalized_string_map();
  int size = SeqOneByteString::SizeFor(str.length());
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, TENURED);

  // Allocate string.
  Object* result;
  { MaybeObject* maybe_result = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // String maps are all immortal immovable objects.
  reinterpret_cast<HeapObject*>(result)->set_map_no_write_barrier(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  ASSERT_EQ(size, answer->Size());

  // Fill in the characters.
  OS::MemCopy(answer->address() + SeqOneByteString::kHeaderSize,
              str.start(), str.length());

  return answer;
}


MaybeObject* Heap::AllocateTwoByteInternalizedString(Vector<const uc16> str,
                                                     uint32_t hash_field) {
  if (str.length() > SeqTwoByteString::kMaxLength) {
    return Failure::OutOfMemoryException(0x3);
  }
  // Compute map and object size.
  Map* map = internalized_string_map();
  int size = SeqTwoByteString::SizeFor(str.length());
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, TENURED);

  // Allocate string.
  Object* result;
  { MaybeObject* maybe_result = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<HeapObject*>(result)->set_map(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  ASSERT_EQ(size, answer->Size());

  // Fill in the characters.
  OS::MemCopy(answer->address() + SeqTwoByteString::kHeaderSize,
              str.start(), str.length() * kUC16Size);

  return answer;
}

MaybeObject* Heap::CopyFixedArray(FixedArray* src) {
  return CopyFixedArrayWithMap(src, src->map());
}


MaybeObject* Heap::CopyFixedDoubleArray(FixedDoubleArray* src) {
  return CopyFixedDoubleArrayWithMap(src, src->map());
}


MaybeObject* Heap::CopyConstantPoolArray(ConstantPoolArray* src) {
  return CopyConstantPoolArrayWithMap(src, src->map());
}


MaybeObject* Heap::AllocateRaw(int size_in_bytes,
                               AllocationSpace space,
                               AllocationSpace retry_space) {
  ASSERT(AllowHandleAllocation::IsAllowed());
  ASSERT(AllowHeapAllocation::IsAllowed());
  ASSERT(gc_state_ == NOT_IN_GC);
  HeapProfiler* profiler = isolate_->heap_profiler();
#ifdef DEBUG
  if (FLAG_gc_interval >= 0 &&
      !disallow_allocation_failure_ &&
      Heap::allocation_timeout_-- <= 0) {
    return Failure::RetryAfterGC(space);
  }
  isolate_->counters()->objs_since_last_full()->Increment();
  isolate_->counters()->objs_since_last_young()->Increment();
#endif

  HeapObject* object;
  MaybeObject* result;
  if (NEW_SPACE == space) {
    result = new_space_.AllocateRaw(size_in_bytes);
    if (always_allocate() && result->IsFailure() && retry_space != NEW_SPACE) {
      space = retry_space;
    } else {
      if (profiler->is_tracking_allocations() && result->To(&object)) {
        profiler->AllocationEvent(object->address(), size_in_bytes);
      }
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
    result = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
  } else if (CELL_SPACE == space) {
    result = cell_space_->AllocateRaw(size_in_bytes);
  } else if (PROPERTY_CELL_SPACE == space) {
    result = property_cell_space_->AllocateRaw(size_in_bytes);
  } else {
    ASSERT(MAP_SPACE == space);
    result = map_space_->AllocateRaw(size_in_bytes);
  }
  if (result->IsFailure()) old_gen_exhausted_ = true;
  if (profiler->is_tracking_allocations() && result->To(&object)) {
    profiler->AllocationEvent(object->address(), size_in_bytes);
  }
  return result;
}


MaybeObject* Heap::NumberFromInt32(
    int32_t value, PretenureFlag pretenure) {
  if (Smi::IsValid(value)) return Smi::FromInt(value);
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastI2D(value), pretenure);
}


MaybeObject* Heap::NumberFromUint32(
    uint32_t value, PretenureFlag pretenure) {
  if (static_cast<int32_t>(value) >= 0 &&
      Smi::IsValid(static_cast<int32_t>(value))) {
    return Smi::FromInt(static_cast<int32_t>(value));
  }
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastUI2D(value), pretenure);
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
    *resource_addr = NULL;
  }
}


bool Heap::InNewSpace(Object* object) {
  bool result = new_space_.Contains(object);
  ASSERT(!result ||                  // Either not in new space
         gc_state_ != NOT_IN_GC ||   // ... or in the middle of GC
         InToSpace(object));         // ... or in to-space (where we allocate).
  return result;
}


bool Heap::InNewSpace(Address address) {
  return new_space_.Contains(address);
}


bool Heap::InFromSpace(Object* object) {
  return new_space_.FromSpaceContains(object);
}


bool Heap::InToSpace(Object* object) {
  return new_space_.ToSpaceContains(object);
}


bool Heap::InOldPointerSpace(Address address) {
  return old_pointer_space_->Contains(address);
}


bool Heap::InOldPointerSpace(Object* object) {
  return InOldPointerSpace(reinterpret_cast<Address>(object));
}


bool Heap::InOldDataSpace(Address address) {
  return old_data_space_->Contains(address);
}


bool Heap::InOldDataSpace(Object* object) {
  return InOldDataSpace(reinterpret_cast<Address>(object));
}


bool Heap::OldGenerationAllocationLimitReached() {
  if (!incremental_marking()->IsStopped()) return false;
  return OldGenerationSpaceAvailable() < 0;
}


bool Heap::ShouldBePromoted(Address old_address, int object_size) {
  // An object should be promoted if:
  // - the object has survived a scavenge operation or
  // - to space is already 25% full.
  NewSpacePage* page = NewSpacePage::FromAddress(old_address);
  Address age_mark = new_space_.age_mark();
  bool below_mark = page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK) &&
      (!page->ContainsLimit(age_mark) || old_address < age_mark);
  return below_mark || (new_space_.Size() + object_size) >=
                        (new_space_.EffectiveCapacity() >> 2);
}


void Heap::RecordWrite(Address address, int offset) {
  if (!InNewSpace(address)) store_buffer_.Mark(address + offset);
}


void Heap::RecordWrites(Address address, int start, int len) {
  if (!InNewSpace(address)) {
    for (int i = 0; i < len; i++) {
      store_buffer_.Mark(address + start + i * kPointerSize);
    }
  }
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
  ASSERT(type != CELL_TYPE);
  ASSERT(type != PROPERTY_CELL_TYPE);

  if (type <= LAST_NAME_TYPE) {
    if (type == SYMBOL_TYPE) return OLD_POINTER_SPACE;
    ASSERT(type < FIRST_NONSTRING_TYPE);
    // There are four string representations: sequential strings, external
    // strings, cons strings, and sliced strings.
    // Only the latter two contain non-map-word pointers to heap objects.
    return ((type & kIsIndirectStringMask) == kIsIndirectStringTag)
        ? OLD_POINTER_SPACE
        : OLD_DATA_SPACE;
  } else {
    return (type <= LAST_DATA_TYPE) ? OLD_DATA_SPACE : OLD_POINTER_SPACE;
  }
}


bool Heap::AllowedToBeMigrated(HeapObject* obj, AllocationSpace dst) {
  // Object migration is governed by the following rules:
  //
  // 1) Objects in new-space can be migrated to one of the old spaces
  //    that matches their target space or they stay in new-space.
  // 2) Objects in old-space stay in the same space when migrating.
  // 3) Fillers (two or more words) can migrate due to left-trimming of
  //    fixed arrays in new-space, old-data-space and old-pointer-space.
  // 4) Fillers (one word) can never migrate, they are skipped by
  //    incremental marking explicitly to prevent invalid pattern.
  // 5) Short external strings can end up in old pointer space when a cons
  //    string in old pointer space is made external (String::MakeExternal).
  //
  // Since this function is used for debugging only, we do not place
  // asserts here, but check everything explicitly.
  if (obj->map() == one_pointer_filler_map()) return false;
  InstanceType type = obj->map()->instance_type();
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  AllocationSpace src = chunk->owner()->identity();
  switch (src) {
    case NEW_SPACE:
      return dst == src || dst == TargetSpaceId(type);
    case OLD_POINTER_SPACE:
      return dst == src &&
          (dst == TargetSpaceId(type) || obj->IsFiller() ||
          (obj->IsExternalString() && ExternalString::cast(obj)->is_short()));
    case OLD_DATA_SPACE:
      return dst == src && dst == TargetSpaceId(type);
    case CODE_SPACE:
      return dst == src && type == CODE_TYPE;
    case MAP_SPACE:
    case CELL_SPACE:
    case PROPERTY_CELL_SPACE:
    case LO_SPACE:
      return false;
  }
  UNREACHABLE();
  return false;
}


void Heap::CopyBlock(Address dst, Address src, int byte_size) {
  CopyWords(reinterpret_cast<Object**>(dst),
            reinterpret_cast<Object**>(src),
            static_cast<size_t>(byte_size / kPointerSize));
}


void Heap::MoveBlock(Address dst, Address src, int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));

  int size_in_words = byte_size / kPointerSize;

  if ((dst < src) || (dst >= (src + byte_size))) {
    Object** src_slot = reinterpret_cast<Object**>(src);
    Object** dst_slot = reinterpret_cast<Object**>(dst);
    Object** end_slot = src_slot + size_in_words;

    while (src_slot != end_slot) {
      *dst_slot++ = *src_slot++;
    }
  } else {
    OS::MemMove(dst, src, static_cast<size_t>(byte_size));
  }
}


void Heap::ScavengePointer(HeapObject** p) {
  ScavengeObject(p, *p);
}


void Heap::UpdateAllocationSiteFeedback(HeapObject* object) {
  Heap* heap = object->GetHeap();
  ASSERT(heap->InFromSpace(object));

  if (!FLAG_allocation_site_pretenuring ||
      !AllocationSite::CanTrack(object->map()->instance_type())) return;

  // Check if there is potentially a memento behind the object. If
  // the last word of the momento is on another page we return
  // immediatelly. Note that we do not have to compare with the current
  // top pointer of the from space page, since we always install filler
  // objects above the top pointer of a from space page when performing
  // a garbage collection.
  Address object_address = object->address();
  Address memento_address = object_address + object->Size();
  Address last_memento_word_address = memento_address + kPointerSize;
  if (!NewSpacePage::OnSamePage(object_address,
                                last_memento_word_address)) {
    return;
  }

  HeapObject* candidate = HeapObject::FromAddress(memento_address);
  if (candidate->map() != heap->allocation_memento_map()) return;

  AllocationMemento* memento = AllocationMemento::cast(candidate);
  if (!memento->IsValid()) return;

  if (memento->GetAllocationSite()->IncrementMementoFoundCount()) {
    heap->AddAllocationSiteToScratchpad(memento->GetAllocationSite());
  }
}


void Heap::ScavengeObject(HeapObject** p, HeapObject* object) {
  ASSERT(object->GetIsolate()->heap()->InFromSpace(object));

  // We use the first word (where the map pointer usually is) of a heap
  // object to record the forwarding pointer.  A forwarding pointer can
  // point to an old space, the code space, or the to space of the new
  // generation.
  MapWord first_word = object->map_word();

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    HeapObject* dest = first_word.ToForwardingAddress();
    ASSERT(object->GetIsolate()->heap()->InFromSpace(*p));
    *p = dest;
    return;
  }

  UpdateAllocationSiteFeedback(object);

  // AllocationMementos are unrooted and shouldn't survive a scavenge
  ASSERT(object->map() != object->GetHeap()->allocation_memento_map());
  // Call the slow part of scavenge object.
  return ScavengeObjectSlow(p, object);
}


bool Heap::CollectGarbage(AllocationSpace space,
                          const char* gc_reason,
                          const v8::GCCallbackFlags callbackFlags) {
  const char* collector_reason = NULL;
  GarbageCollector collector = SelectGarbageCollector(space, &collector_reason);
  return CollectGarbage(collector, gc_reason, collector_reason, callbackFlags);
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


int64_t Heap::AdjustAmountOfExternalAllocatedMemory(
    int64_t change_in_bytes) {
  ASSERT(HasBeenSetUp());
  int64_t amount = amount_of_external_allocated_memory_ + change_in_bytes;
  if (change_in_bytes > 0) {
    // Avoid overflow.
    if (amount > amount_of_external_allocated_memory_) {
      amount_of_external_allocated_memory_ = amount;
    } else {
      // Give up and reset the counters in case of an overflow.
      amount_of_external_allocated_memory_ = 0;
      amount_of_external_allocated_memory_at_last_global_gc_ = 0;
    }
    int64_t amount_since_last_global_gc = PromotedExternalMemorySize();
    if (amount_since_last_global_gc > external_allocation_limit_) {
      CollectAllGarbage(kNoGCFlags, "external memory allocation limit reached");
    }
  } else {
    // Avoid underflow.
    if (amount >= 0) {
      amount_of_external_allocated_memory_ = amount;
    } else {
      // Give up and reset the counters in case of an underflow.
      amount_of_external_allocated_memory_ = 0;
      amount_of_external_allocated_memory_at_last_global_gc_ = 0;
    }
  }
  if (FLAG_trace_external_memory) {
    PrintPID("%8.0f ms: ", isolate()->time_millis_since_init());
    PrintF("Adjust amount of external memory: delta=%6" V8_PTR_PREFIX "d KB, "
           "amount=%6" V8_PTR_PREFIX "d KB, since_gc=%6" V8_PTR_PREFIX "d KB, "
           "isolate=0x%08" V8PRIxPTR ".\n",
           static_cast<intptr_t>(change_in_bytes / KB),
           static_cast<intptr_t>(amount_of_external_allocated_memory_ / KB),
           static_cast<intptr_t>(PromotedExternalMemorySize() / KB),
           reinterpret_cast<intptr_t>(isolate()));
  }
  ASSERT(amount_of_external_allocated_memory_ >= 0);
  return amount_of_external_allocated_memory_;
}


Isolate* Heap::isolate() {
  return reinterpret_cast<Isolate*>(reinterpret_cast<intptr_t>(this) -
      reinterpret_cast<size_t>(reinterpret_cast<Isolate*>(4)->heap()) + 4);
}


#ifdef DEBUG
#define GC_GREEDY_CHECK(ISOLATE) \
  if (FLAG_gc_greedy) (ISOLATE)->heap()->GarbageCollectionGreedyCheck()
#else
#define GC_GREEDY_CHECK(ISOLATE) { }
#endif

// Calls the FUNCTION_CALL function and retries it up to three times
// to guarantee that any allocations performed during the call will
// succeed if there's enough memory.

// Warning: Do not use the identifiers __object__, __maybe_object__ or
// __scope__ in a call to this macro.

#define CALL_AND_RETRY(ISOLATE, FUNCTION_CALL, RETURN_VALUE, RETURN_EMPTY, OOM)\
  do {                                                                         \
    GC_GREEDY_CHECK(ISOLATE);                                                  \
    MaybeObject* __maybe_object__ = FUNCTION_CALL;                             \
    Object* __object__ = NULL;                                                 \
    if (__maybe_object__->ToObject(&__object__)) RETURN_VALUE;                 \
    if (__maybe_object__->IsOutOfMemory()) {                                   \
      OOM;                                                                     \
    }                                                                          \
    if (!__maybe_object__->IsRetryAfterGC()) RETURN_EMPTY;                     \
    (ISOLATE)->heap()->CollectGarbage(Failure::cast(__maybe_object__)->        \
                                    allocation_space(),                        \
                                    "allocation failure");                     \
    __maybe_object__ = FUNCTION_CALL;                                          \
    if (__maybe_object__->ToObject(&__object__)) RETURN_VALUE;                 \
    if (__maybe_object__->IsOutOfMemory()) {                                   \
      OOM;                                                                     \
    }                                                                          \
    if (!__maybe_object__->IsRetryAfterGC()) RETURN_EMPTY;                     \
    (ISOLATE)->counters()->gc_last_resort_from_handles()->Increment();         \
    (ISOLATE)->heap()->CollectAllAvailableGarbage("last resort gc");           \
    {                                                                          \
      AlwaysAllocateScope __scope__;                                           \
      __maybe_object__ = FUNCTION_CALL;                                        \
    }                                                                          \
    if (__maybe_object__->ToObject(&__object__)) RETURN_VALUE;                 \
    if (__maybe_object__->IsOutOfMemory()) {                                   \
      OOM;                                                                     \
    }                                                                          \
    if (__maybe_object__->IsRetryAfterGC()) {                                  \
      /* TODO(1181417): Fix this. */                                           \
      v8::internal::Heap::FatalProcessOutOfMemory("CALL_AND_RETRY_LAST", true);\
    }                                                                          \
    RETURN_EMPTY;                                                              \
  } while (false)

#define CALL_AND_RETRY_OR_DIE(                                             \
     ISOLATE, FUNCTION_CALL, RETURN_VALUE, RETURN_EMPTY)                   \
  CALL_AND_RETRY(                                                          \
      ISOLATE,                                                             \
      FUNCTION_CALL,                                                       \
      RETURN_VALUE,                                                        \
      RETURN_EMPTY,                                                        \
      v8::internal::Heap::FatalProcessOutOfMemory("CALL_AND_RETRY", true))

#define CALL_HEAP_FUNCTION(ISOLATE, FUNCTION_CALL, TYPE)                      \
  CALL_AND_RETRY_OR_DIE(ISOLATE,                                              \
                        FUNCTION_CALL,                                        \
                        return Handle<TYPE>(TYPE::cast(__object__), ISOLATE), \
                        return Handle<TYPE>())                                \


#define CALL_HEAP_FUNCTION_VOID(ISOLATE, FUNCTION_CALL)  \
  CALL_AND_RETRY_OR_DIE(ISOLATE, FUNCTION_CALL, return, return)


#define CALL_HEAP_FUNCTION_PASS_EXCEPTION(ISOLATE, FUNCTION_CALL) \
  CALL_AND_RETRY(ISOLATE,                                         \
                 FUNCTION_CALL,                                   \
                 return __object__,                               \
                 return __maybe_object__,                         \
                 return __maybe_object__)


void ExternalStringTable::AddString(String* string) {
  ASSERT(string->IsExternalString());
  if (heap_->InNewSpace(string)) {
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
    Object* obj = Object::cast(new_space_strings_[i]);
    ASSERT(heap_->InNewSpace(obj));
    ASSERT(obj != heap_->the_hole_value());
  }
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    Object* obj = Object::cast(old_space_strings_[i]);
    ASSERT(!heap_->InNewSpace(obj));
    ASSERT(obj != heap_->the_hole_value());
  }
#endif
}


void ExternalStringTable::AddOldString(String* string) {
  ASSERT(string->IsExternalString());
  ASSERT(!heap_->InNewSpace(string));
  old_space_strings_.Add(string);
}


void ExternalStringTable::ShrinkNewStrings(int position) {
  new_space_strings_.Rewind(position);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}


void Heap::ClearInstanceofCache() {
  set_instanceof_cache_function(the_hole_value());
}


Object* Heap::ToBoolean(bool condition) {
  return condition ? true_value() : false_value();
}


void Heap::CompletelyClearInstanceofCache() {
  set_instanceof_cache_map(the_hole_value());
  set_instanceof_cache_function(the_hole_value());
}


AlwaysAllocateScope::AlwaysAllocateScope() {
  // We shouldn't hit any nested scopes, because that requires
  // non-handle code to call handle code. The code still works but
  // performance will degrade, so we want to catch this situation
  // in debug mode.
  Isolate* isolate = Isolate::Current();
  ASSERT(isolate->heap()->always_allocate_scope_depth_ == 0);
  isolate->heap()->always_allocate_scope_depth_++;
}


AlwaysAllocateScope::~AlwaysAllocateScope() {
  Isolate* isolate = Isolate::Current();
  isolate->heap()->always_allocate_scope_depth_--;
  ASSERT(isolate->heap()->always_allocate_scope_depth_ == 0);
}


#ifdef VERIFY_HEAP
NoWeakObjectVerificationScope::NoWeakObjectVerificationScope() {
  Isolate* isolate = Isolate::Current();
  isolate->heap()->no_weak_object_verification_scope_depth_++;
}


NoWeakObjectVerificationScope::~NoWeakObjectVerificationScope() {
  Isolate* isolate = Isolate::Current();
  isolate->heap()->no_weak_object_verification_scope_depth_--;
}
#endif


void VerifyPointersVisitor::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    if ((*current)->IsHeapObject()) {
      HeapObject* object = HeapObject::cast(*current);
      CHECK(object->GetIsolate()->heap()->Contains(object));
      CHECK(object->map()->IsMap());
    }
  }
}


void VerifySmisVisitor::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
     CHECK((*current)->IsSmi());
  }
}


double GCTracer::SizeOfHeapObjects() {
  return (static_cast<double>(heap_->SizeOfObjects())) / MB;
}


DisallowAllocationFailure::DisallowAllocationFailure() {
#ifdef DEBUG
  Isolate* isolate = Isolate::Current();
  old_state_ = isolate->heap()->disallow_allocation_failure_;
  isolate->heap()->disallow_allocation_failure_ = true;
#endif
}


DisallowAllocationFailure::~DisallowAllocationFailure() {
#ifdef DEBUG
  Isolate* isolate = Isolate::Current();
  isolate->heap()->disallow_allocation_failure_ = old_state_;
#endif
}


} }  // namespace v8::internal

#endif  // V8_HEAP_INL_H_
