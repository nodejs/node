// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_INL_H_
#define V8_HEAP_HEAP_INL_H_

#include <cmath>

#include "src/base/platform/platform.h"
#include "src/counters.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/store-buffer.h"
#include "src/heap/store-buffer-inl.h"
#include "src/isolate.h"
#include "src/list-inl.h"
#include "src/log.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {

void PromotionQueue::insert(HeapObject* target, int size) {
  if (emergency_stack_ != NULL) {
    emergency_stack_->Add(Entry(target, size));
    return;
  }

  if ((rear_ - 2) < limit_) {
    RelocateQueueHead();
    emergency_stack_->Add(Entry(target, size));
    return;
  }

  *(--rear_) = reinterpret_cast<intptr_t>(target);
  *(--rear_) = size;
// Assert no overflow into live objects.
#ifdef DEBUG
  SemiSpace::AssertValidRange(target->GetIsolate()->heap()->new_space()->top(),
                              reinterpret_cast<Address>(rear_));
#endif
}


#define ROOT_ACCESSOR(type, name, camel_name) \
  type* Heap::name() { return type::cast(roots_[k##camel_name##RootIndex]); }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name) \
  Map* Heap::name##_map() { return Map::cast(roots_[k##Name##MapRootIndex]); }
STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) \
  String* Heap::name() { return String::cast(roots_[k##name##RootIndex]); }
INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) \
  Symbol* Heap::name() { return Symbol::cast(roots_[k##name##RootIndex]); }
PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description) \
  Symbol* Heap::name() { return Symbol::cast(roots_[k##name##RootIndex]); }
PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define ROOT_ACCESSOR(type, name, camel_name)                                 \
  void Heap::set_##name(type* value) {                                        \
    /* The deserializer makes use of the fact that these common roots are */  \
    /* never in new space and never on a page that is being compacted.    */  \
    DCHECK(!deserialization_complete() ||                                     \
           RootCanBeWrittenAfterInitialization(k##camel_name##RootIndex));    \
    DCHECK(k##camel_name##RootIndex >= kOldSpaceRoots || !InNewSpace(value)); \
    roots_[k##camel_name##RootIndex] = value;                                 \
  }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR


template <>
bool inline Heap::IsOneByte(Vector<const char> str, int chars) {
  // TODO(dcarney): incorporate Latin-1 check when Latin-1 is supported?
  return chars == str.length();
}


template <>
bool inline Heap::IsOneByte(String* str, int chars) {
  return str->IsOneByteRepresentation();
}


AllocationResult Heap::AllocateInternalizedStringFromUtf8(
    Vector<const char> str, int chars, uint32_t hash_field) {
  if (IsOneByte(str, chars)) {
    return AllocateOneByteInternalizedString(Vector<const uint8_t>::cast(str),
                                             hash_field);
  }
  return AllocateInternalizedStringImpl<false>(str, chars, hash_field);
}


template <typename T>
AllocationResult Heap::AllocateInternalizedStringImpl(T t, int chars,
                                                      uint32_t hash_field) {
  if (IsOneByte(t, chars)) {
    return AllocateInternalizedStringImpl<true>(t, chars, hash_field);
  }
  return AllocateInternalizedStringImpl<false>(t, chars, hash_field);
}


AllocationResult Heap::AllocateOneByteInternalizedString(
    Vector<const uint8_t> str, uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, str.length());
  // Compute map and object size.
  Map* map = one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(str.length());

  // Allocate string.
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  // String maps are all immortal immovable objects.
  result->set_map_no_write_barrier(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  DCHECK_EQ(size, answer->Size());

  // Fill in the characters.
  MemCopy(answer->address() + SeqOneByteString::kHeaderSize, str.start(),
          str.length());

  return answer;
}


AllocationResult Heap::AllocateTwoByteInternalizedString(Vector<const uc16> str,
                                                         uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, str.length());
  // Compute map and object size.
  Map* map = internalized_string_map();
  int size = SeqTwoByteString::SizeFor(str.length());

  // Allocate string.
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(str.length());
  answer->set_hash_field(hash_field);

  DCHECK_EQ(size, answer->Size());

  // Fill in the characters.
  MemCopy(answer->address() + SeqTwoByteString::kHeaderSize, str.start(),
          str.length() * kUC16Size);

  return answer;
}

AllocationResult Heap::CopyFixedArray(FixedArray* src) {
  if (src->length() == 0) return src;
  return CopyFixedArrayWithMap(src, src->map());
}


AllocationResult Heap::CopyFixedDoubleArray(FixedDoubleArray* src) {
  if (src->length() == 0) return src;
  return CopyFixedDoubleArrayWithMap(src, src->map());
}


AllocationResult Heap::AllocateRaw(int size_in_bytes, AllocationSpace space,
                                   AllocationAlignment alignment) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(gc_state_ == NOT_IN_GC);
#ifdef DEBUG
  if (FLAG_gc_interval >= 0 && !always_allocate() &&
      Heap::allocation_timeout_-- <= 0) {
    return AllocationResult::Retry(space);
  }
  isolate_->counters()->objs_since_last_full()->Increment();
  isolate_->counters()->objs_since_last_young()->Increment();
#endif

  bool large_object = size_in_bytes > Page::kMaxRegularHeapObjectSize;
  HeapObject* object = nullptr;
  AllocationResult allocation;
  if (NEW_SPACE == space) {
    if (large_object) {
      space = LO_SPACE;
    } else {
      allocation = new_space_.AllocateRaw(size_in_bytes, alignment);
      if (allocation.To(&object)) {
        OnAllocationEvent(object, size_in_bytes);
      }
      return allocation;
    }
  }

  // Here we only allocate in the old generation.
  if (OLD_SPACE == space) {
    if (large_object) {
      allocation = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
    } else {
      allocation = old_space_->AllocateRaw(size_in_bytes, alignment);
    }
  } else if (CODE_SPACE == space) {
    if (size_in_bytes <= code_space()->AreaSize()) {
      allocation = code_space_->AllocateRawUnaligned(size_in_bytes);
    } else {
      allocation = lo_space_->AllocateRaw(size_in_bytes, EXECUTABLE);
    }
  } else if (LO_SPACE == space) {
    DCHECK(large_object);
    allocation = lo_space_->AllocateRaw(size_in_bytes, NOT_EXECUTABLE);
  } else if (MAP_SPACE == space) {
    allocation = map_space_->AllocateRawUnaligned(size_in_bytes);
  } else {
    // NEW_SPACE is not allowed here.
    UNREACHABLE();
  }
  if (allocation.To(&object)) {
    OnAllocationEvent(object, size_in_bytes);
  } else {
    old_gen_exhausted_ = true;
  }
  return allocation;
}


void Heap::OnAllocationEvent(HeapObject* object, int size_in_bytes) {
  HeapProfiler* profiler = isolate_->heap_profiler();
  if (profiler->is_tracking_allocations()) {
    profiler->AllocationEvent(object->address(), size_in_bytes);
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(object);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAlloctionsHash();
    }
  }

  if (FLAG_trace_allocation_stack_interval > 0) {
    if (!FLAG_verify_predictable) ++allocations_count_;
    if (allocations_count_ % FLAG_trace_allocation_stack_interval == 0) {
      isolate()->PrintStack(stdout, Isolate::kPrintStackConcise);
    }
  }
}


void Heap::OnMoveEvent(HeapObject* target, HeapObject* source,
                       int size_in_bytes) {
  HeapProfiler* heap_profiler = isolate_->heap_profiler();
  if (heap_profiler->is_tracking_object_moves()) {
    heap_profiler->ObjectMoveEvent(source->address(), target->address(),
                                   size_in_bytes);
  }
  if (target->IsSharedFunctionInfo()) {
    LOG_CODE_EVENT(isolate_, SharedFunctionInfoMoveEvent(source->address(),
                                                         target->address()));
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(source);
    UpdateAllocationsHash(target);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAlloctionsHash();
    }
  }
}


void Heap::UpdateAllocationsHash(HeapObject* object) {
  Address object_address = object->address();
  MemoryChunk* memory_chunk = MemoryChunk::FromAddress(object_address);
  AllocationSpace allocation_space = memory_chunk->owner()->identity();

  STATIC_ASSERT(kSpaceTagSize + kPageSizeBits <= 32);
  uint32_t value =
      static_cast<uint32_t>(object_address - memory_chunk->address()) |
      (static_cast<uint32_t>(allocation_space) << kPageSizeBits);

  UpdateAllocationsHash(value);
}


void Heap::UpdateAllocationsHash(uint32_t value) {
  uint16_t c1 = static_cast<uint16_t>(value);
  uint16_t c2 = static_cast<uint16_t>(value >> 16);
  raw_allocations_hash_ =
      StringHasher::AddCharacterCore(raw_allocations_hash_, c1);
  raw_allocations_hash_ =
      StringHasher::AddCharacterCore(raw_allocations_hash_, c2);
}


void Heap::RegisterExternalString(String* string) {
  external_string_table_.AddString(string);
}


void Heap::FinalizeExternalString(String* string) {
  DCHECK(string->IsExternalString());
  v8::String::ExternalStringResourceBase** resource_addr =
      reinterpret_cast<v8::String::ExternalStringResourceBase**>(
          reinterpret_cast<byte*>(string) + ExternalString::kResourceOffset -
          kHeapObjectTag);

  // Dispose of the C++ object if it has not already been disposed.
  if (*resource_addr != NULL) {
    (*resource_addr)->Dispose();
    *resource_addr = NULL;
  }
}


bool Heap::InNewSpace(Object* object) {
  bool result = new_space_.Contains(object);
  DCHECK(!result ||                 // Either not in new space
         gc_state_ != NOT_IN_GC ||  // ... or in the middle of GC
         InToSpace(object));        // ... or in to-space (where we allocate).
  return result;
}


bool Heap::InNewSpace(Address address) { return new_space_.Contains(address); }


bool Heap::InFromSpace(Object* object) {
  return new_space_.FromSpaceContains(object);
}


bool Heap::InToSpace(Object* object) {
  return new_space_.ToSpaceContains(object);
}


bool Heap::InOldSpace(Address address) { return old_space_->Contains(address); }


bool Heap::InOldSpace(Object* object) {
  return InOldSpace(reinterpret_cast<Address>(object));
}


bool Heap::OldGenerationAllocationLimitReached() {
  if (!incremental_marking()->IsStopped()) return false;
  return OldGenerationSpaceAvailable() < 0;
}


bool Heap::ShouldBePromoted(Address old_address, int object_size) {
  NewSpacePage* page = NewSpacePage::FromAddress(old_address);
  Address age_mark = new_space_.age_mark();
  return page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK) &&
         (!page->ContainsLimit(age_mark) || old_address < age_mark);
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


bool Heap::AllowedToBeMigrated(HeapObject* obj, AllocationSpace dst) {
  // Object migration is governed by the following rules:
  //
  // 1) Objects in new-space can be migrated to the old space
  //    that matches their target space or they stay in new-space.
  // 2) Objects in old-space stay in the same space when migrating.
  // 3) Fillers (two or more words) can migrate due to left-trimming of
  //    fixed arrays in new-space or old space.
  // 4) Fillers (one word) can never migrate, they are skipped by
  //    incremental marking explicitly to prevent invalid pattern.
  //
  // Since this function is used for debugging only, we do not place
  // asserts here, but check everything explicitly.
  if (obj->map() == one_pointer_filler_map()) return false;
  InstanceType type = obj->map()->instance_type();
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  AllocationSpace src = chunk->owner()->identity();
  switch (src) {
    case NEW_SPACE:
      return dst == src || dst == OLD_SPACE;
    case OLD_SPACE:
      return dst == src &&
             (dst == OLD_SPACE || obj->IsFiller() || obj->IsExternalString());
    case CODE_SPACE:
      return dst == src && type == CODE_TYPE;
    case MAP_SPACE:
    case LO_SPACE:
      return false;
  }
  UNREACHABLE();
  return false;
}


void Heap::CopyBlock(Address dst, Address src, int byte_size) {
  CopyWords(reinterpret_cast<Object**>(dst), reinterpret_cast<Object**>(src),
            static_cast<size_t>(byte_size / kPointerSize));
}


void Heap::MoveBlock(Address dst, Address src, int byte_size) {
  DCHECK(IsAligned(byte_size, kPointerSize));

  int size_in_words = byte_size / kPointerSize;

  if ((dst < src) || (dst >= (src + byte_size))) {
    Object** src_slot = reinterpret_cast<Object**>(src);
    Object** dst_slot = reinterpret_cast<Object**>(dst);
    Object** end_slot = src_slot + size_in_words;

    while (src_slot != end_slot) {
      *dst_slot++ = *src_slot++;
    }
  } else {
    MemMove(dst, src, static_cast<size_t>(byte_size));
  }
}


AllocationMemento* Heap::FindAllocationMemento(HeapObject* object) {
  // Check if there is potentially a memento behind the object. If
  // the last word of the memento is on another page we return
  // immediately.
  Address object_address = object->address();
  Address memento_address = object_address + object->Size();
  Address last_memento_word_address = memento_address + kPointerSize;
  if (!NewSpacePage::OnSamePage(object_address, last_memento_word_address)) {
    return NULL;
  }

  HeapObject* candidate = HeapObject::FromAddress(memento_address);
  Map* candidate_map = candidate->map();
  // This fast check may peek at an uninitialized word. However, the slow check
  // below (memento_address == top) ensures that this is safe. Mark the word as
  // initialized to silence MemorySanitizer warnings.
  MSAN_MEMORY_IS_INITIALIZED(&candidate_map, sizeof(candidate_map));
  if (candidate_map != allocation_memento_map()) return NULL;

  // Either the object is the last object in the new space, or there is another
  // object of at least word size (the header map word) following it, so
  // suffices to compare ptr and top here. Note that technically we do not have
  // to compare with the current top pointer of the from space page during GC,
  // since we always install filler objects above the top pointer of a from
  // space page when performing a garbage collection. However, always performing
  // the test makes it possible to have a single, unified version of
  // FindAllocationMemento that is used both by the GC and the mutator.
  Address top = NewSpaceTop();
  DCHECK(memento_address == top ||
         memento_address + HeapObject::kHeaderSize <= top ||
         !NewSpacePage::OnSamePage(memento_address, top - 1));
  if (memento_address == top) return NULL;

  AllocationMemento* memento = AllocationMemento::cast(candidate);
  if (!memento->IsValid()) return NULL;
  return memento;
}


void Heap::UpdateAllocationSite(HeapObject* object,
                                HashMap* pretenuring_feedback) {
  DCHECK(InFromSpace(object));
  if (!FLAG_allocation_site_pretenuring ||
      !AllocationSite::CanTrack(object->map()->instance_type()))
    return;
  AllocationMemento* memento = FindAllocationMemento(object);
  if (memento == nullptr) return;

  AllocationSite* key = memento->GetAllocationSite();
  DCHECK(!key->IsZombie());

  if (pretenuring_feedback == global_pretenuring_feedback_) {
    // For inserting in the global pretenuring storage we need to first
    // increment the memento found count on the allocation site.
    if (key->IncrementMementoFoundCount()) {
      global_pretenuring_feedback_->LookupOrInsert(
          key, static_cast<uint32_t>(bit_cast<uintptr_t>(key)));
    }
  } else {
    // Any other pretenuring storage than the global one is used as a cache,
    // where the count is later on merge in the allocation site.
    HashMap::Entry* e = pretenuring_feedback->LookupOrInsert(
        key, static_cast<uint32_t>(bit_cast<uintptr_t>(key)));
    DCHECK(e != nullptr);
    (*bit_cast<intptr_t*>(&e->value))++;
  }
}


void Heap::RemoveAllocationSitePretenuringFeedback(AllocationSite* site) {
  global_pretenuring_feedback_->Remove(
      site, static_cast<uint32_t>(bit_cast<uintptr_t>(site)));
}


bool Heap::CollectGarbage(AllocationSpace space, const char* gc_reason,
                          const v8::GCCallbackFlags callbackFlags) {
  const char* collector_reason = NULL;
  GarbageCollector collector = SelectGarbageCollector(space, &collector_reason);
  return CollectGarbage(collector, gc_reason, collector_reason, callbackFlags);
}


Isolate* Heap::isolate() {
  return reinterpret_cast<Isolate*>(
      reinterpret_cast<intptr_t>(this) -
      reinterpret_cast<size_t>(reinterpret_cast<Isolate*>(16)->heap()) + 16);
}


void Heap::ExternalStringTable::AddString(String* string) {
  DCHECK(string->IsExternalString());
  if (heap_->InNewSpace(string)) {
    new_space_strings_.Add(string);
  } else {
    old_space_strings_.Add(string);
  }
}


void Heap::ExternalStringTable::Iterate(ObjectVisitor* v) {
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
void Heap::ExternalStringTable::Verify() {
#ifdef DEBUG
  for (int i = 0; i < new_space_strings_.length(); ++i) {
    Object* obj = Object::cast(new_space_strings_[i]);
    DCHECK(heap_->InNewSpace(obj));
    DCHECK(obj != heap_->the_hole_value());
  }
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    Object* obj = Object::cast(old_space_strings_[i]);
    DCHECK(!heap_->InNewSpace(obj));
    DCHECK(obj != heap_->the_hole_value());
  }
#endif
}


void Heap::ExternalStringTable::AddOldString(String* string) {
  DCHECK(string->IsExternalString());
  DCHECK(!heap_->InNewSpace(string));
  old_space_strings_.Add(string);
}


void Heap::ExternalStringTable::ShrinkNewStrings(int position) {
  new_space_strings_.Rewind(position);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}


int DescriptorLookupCache::Lookup(Map* source, Name* name) {
  if (!name->IsUniqueName()) return kAbsent;
  int index = Hash(source, name);
  Key& key = keys_[index];
  if ((key.source == source) && (key.name == name)) return results_[index];
  return kAbsent;
}


void DescriptorLookupCache::Update(Map* source, Name* name, int result) {
  DCHECK(result != kAbsent);
  if (name->IsUniqueName()) {
    int index = Hash(source, name);
    Key& key = keys_[index];
    key.source = source;
    key.name = name;
    results_[index] = result;
  }
}


void Heap::ClearInstanceofCache() {
  set_instanceof_cache_function(Smi::FromInt(0));
}


Object* Heap::ToBoolean(bool condition) {
  return condition ? true_value() : false_value();
}


void Heap::CompletelyClearInstanceofCache() {
  set_instanceof_cache_map(Smi::FromInt(0));
  set_instanceof_cache_function(Smi::FromInt(0));
}


uint32_t Heap::HashSeed() {
  uint32_t seed = static_cast<uint32_t>(hash_seed()->value());
  DCHECK(FLAG_randomize_hashes || seed == 0);
  return seed;
}


int Heap::NextScriptId() {
  int last_id = last_script_id()->value();
  if (last_id == Smi::kMaxValue) {
    last_id = 1;
  } else {
    last_id++;
  }
  set_last_script_id(Smi::FromInt(last_id));
  return last_id;
}


void Heap::SetArgumentsAdaptorDeoptPCOffset(int pc_offset) {
  DCHECK(arguments_adaptor_deopt_pc_offset() == Smi::FromInt(0));
  set_arguments_adaptor_deopt_pc_offset(Smi::FromInt(pc_offset));
}


void Heap::SetConstructStubDeoptPCOffset(int pc_offset) {
  DCHECK(construct_stub_deopt_pc_offset() == Smi::FromInt(0));
  set_construct_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
}


void Heap::SetGetterStubDeoptPCOffset(int pc_offset) {
  DCHECK(getter_stub_deopt_pc_offset() == Smi::FromInt(0));
  set_getter_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
}


void Heap::SetSetterStubDeoptPCOffset(int pc_offset) {
  DCHECK(setter_stub_deopt_pc_offset() == Smi::FromInt(0));
  set_setter_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
}


AlwaysAllocateScope::AlwaysAllocateScope(Isolate* isolate)
    : heap_(isolate->heap()) {
  heap_->always_allocate_scope_count_.Increment(1);
}


AlwaysAllocateScope::~AlwaysAllocateScope() {
  heap_->always_allocate_scope_count_.Increment(-1);
}


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
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_INL_H_
