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

namespace v8 {
namespace internal {

int Heap::MaxObjectSizeInPagedSpace() {
  return Page::kMaxHeapObjectSize;
}


Object* Heap::AllocateSymbol(Vector<const char> str,
                             int chars,
                             uint32_t hash_field) {
  unibrow::Utf8InputBuffer<> buffer(str.start(),
                                    static_cast<unsigned>(str.length()));
  return AllocateInternalSymbol(&buffer, chars, hash_field);
}


Object* Heap::AllocateRaw(int size_in_bytes,
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
  } else if (CELL_SPACE == space) {
    result = cell_space_->AllocateRaw(size_in_bytes);
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


void Heap::FinalizeExternalString(String* string) {
  ASSERT(string->IsExternalString());
  v8::String::ExternalStringResourceBase** resource_addr =
      reinterpret_cast<v8::String::ExternalStringResourceBase**>(
          reinterpret_cast<byte*>(string) +
          ExternalString::kResourceOffset -
          kHeapObjectTag);
  delete *resource_addr;
  // Clear the resource pointer in the string.
  *resource_addr = NULL;
}


Object* Heap::AllocateRawMap() {
#ifdef DEBUG
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  Object* result = map_space_->AllocateRaw(Map::kSize);
  if (result->IsFailure()) old_gen_exhausted_ = true;
#ifdef DEBUG
  if (!result->IsFailure()) {
    // Maps have their own alignment.
    CHECK((OffsetFrom(result) & kMapAlignmentMask) == kHeapObjectTag);
  }
#endif
  return result;
}


Object* Heap::AllocateRawCell() {
#ifdef DEBUG
  Counters::objs_since_last_full.Increment();
  Counters::objs_since_last_young.Increment();
#endif
  Object* result = cell_space_->AllocateRaw(JSGlobalPropertyCell::kSize);
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
    Heap::CollectAllGarbage(false);                                       \
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
