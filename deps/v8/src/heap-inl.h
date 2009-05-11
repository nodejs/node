// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "log.h"
#include "v8-counters.h"

namespace v8 { namespace internal {

int Heap::MaxHeapObjectSize() {
  return Page::kMaxHeapObjectSize;
}


Object* Heap::AllocateSymbol(Vector<const char> str,
                             int chars,
                             uint32_t length_field) {
  unibrow::Utf8InputBuffer<> buffer(str.start(),
                                    static_cast<unsigned>(str.length()));
  return AllocateInternalSymbol(&buffer, chars, length_field);
}


Object* Heap::AllocateRaw(int size_in_bytes,
                          AllocationSpace space,
                          AllocationSpace retry_space) {
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
  ASSERT(space != NEW_SPACE ||
         retry_space == OLD_POINTER_SPACE ||
         retry_space == OLD_DATA_SPACE);
#ifdef DEBUG
  if (FLAG_gc_interval >= 0 &&
      !disallow_allocation_failure_ &&
      Heap::allocation_timeout_-- <= 0) {
    return Failure::RetryAfterGC(size_in_bytes, space);
  }
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  Object* result;
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
  } else {
    ASSERT(MAP_SPACE == space);
    result = map_space_->AllocateRaw(size_in_bytes);
  }
  if (result->IsFailure()) old_gen_exhausted_ = true;
  return result;
}


Object* Heap::NumberFromInt32(int32_t value) {
  if (Smi::IsValid(value)) return Smi::FromInt(value);
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastI2D(value));
}


Object* Heap::NumberFromUint32(uint32_t value) {
  if ((int32_t)value >= 0 && Smi::IsValid((int32_t)value)) {
    return Smi::FromInt((int32_t)value);
  }
  // Bypass NumberFromDouble to avoid various redundant checks.
  return AllocateHeapNumber(FastUI2D(value));
}


Object* Heap::AllocateRawMap(int size_in_bytes) {
#ifdef DEBUG
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  Object* result = map_space_->AllocateRaw(size_in_bytes);
  if (result->IsFailure()) old_gen_exhausted_ = true;
  return result;
}


bool Heap::InNewSpace(Object* object) {
  return new_space_.Contains(object);
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
  Page::SetRSet(address, offset);
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
  ASSERT((type != CODE_TYPE) && (type != MAP_TYPE));
  bool has_pointers =
      type != HEAP_NUMBER_TYPE &&
      (type >= FIRST_NONSTRING_TYPE ||
       (type & kStringRepresentationMask) != kSeqStringTag);
  return has_pointers ? OLD_POINTER_SPACE : OLD_DATA_SPACE;
}


void Heap::CopyBlock(Object** dst, Object** src, int byte_size) {
  ASSERT(IsAligned(byte_size, kPointerSize));

  // Use block copying memcpy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  static const int kBlockCopyLimit = 16 * kPointerSize;

  if (byte_size >= kBlockCopyLimit) {
    memcpy(dst, src, byte_size);
  } else {
    int remaining = byte_size / kPointerSize;
    do {
      remaining--;
      *dst++ = *src++;
    } while (remaining > 0);
  }
}


Object* Heap::GetKeyedLookupCache() {
  if (keyed_lookup_cache()->IsUndefined()) {
    Object* obj = LookupCache::Allocate(4);
    if (obj->IsFailure()) return obj;
    keyed_lookup_cache_ = obj;
  }
  return keyed_lookup_cache();
}


void Heap::SetKeyedLookupCache(LookupCache* cache) {
  keyed_lookup_cache_ = cache;
}


void Heap::ClearKeyedLookupCache() {
  keyed_lookup_cache_ = undefined_value();
}


void Heap::SetLastScriptId(Object* last_script_id) {
  last_script_id_ = last_script_id;
}


#define GC_GREEDY_CHECK() \
  ASSERT(!FLAG_gc_greedy || v8::internal::Heap::GarbageCollectionGreedyCheck())


// Calls the FUNCTION_CALL function and retries it up to three times
// to guarantee that any allocations performed during the call will
// succeed if there's enough memory.

// Warning: Do not use the identifiers __object__ or __scope__ in a
// call to this macro.

#define CALL_AND_RETRY(FUNCTION_CALL, RETURN_VALUE, RETURN_EMPTY)         \
  do {                                                                    \
    GC_GREEDY_CHECK();                                                    \
    Object* __object__ = FUNCTION_CALL;                                   \
    if (!__object__->IsFailure()) RETURN_VALUE;                           \
    if (__object__->IsOutOfMemoryFailure()) {                             \
      v8::internal::V8::FatalProcessOutOfMemory("CALL_AND_RETRY_0");      \
    }                                                                     \
    if (!__object__->IsRetryAfterGC()) RETURN_EMPTY;                      \
    Heap::CollectGarbage(Failure::cast(__object__)->requested(),          \
                         Failure::cast(__object__)->allocation_space());  \
    __object__ = FUNCTION_CALL;                                           \
    if (!__object__->IsFailure()) RETURN_VALUE;                           \
    if (__object__->IsOutOfMemoryFailure()) {                             \
      v8::internal::V8::FatalProcessOutOfMemory("CALL_AND_RETRY_1");      \
    }                                                                     \
    if (!__object__->IsRetryAfterGC()) RETURN_EMPTY;                      \
    Counters::gc_last_resort_from_handles.Increment();                    \
    Heap::CollectAllGarbage();                                            \
    {                                                                     \
      AlwaysAllocateScope __scope__;                                      \
      __object__ = FUNCTION_CALL;                                         \
    }                                                                     \
    if (!__object__->IsFailure()) RETURN_VALUE;                           \
    if (__object__->IsOutOfMemoryFailure() ||                             \
        __object__->IsRetryAfterGC()) {                                   \
      /* TODO(1181417): Fix this. */                                      \
      v8::internal::V8::FatalProcessOutOfMemory("CALL_AND_RETRY_2");      \
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


} }  // namespace v8::internal

#endif  // V8_HEAP_INL_H_
