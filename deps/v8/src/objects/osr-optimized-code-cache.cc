// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/osr-optimized-code-cache.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/code.h"
#include "src/objects/maybe-object.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

// static
Handle<OSROptimizedCodeCache> OSROptimizedCodeCache::Empty(Isolate* isolate) {
  return Handle<OSROptimizedCodeCache>::cast(
      isolate->factory()->empty_weak_fixed_array());
}

// static
void OSROptimizedCodeCache::Insert(Isolate* isolate,
                                   Handle<NativeContext> native_context,
                                   Handle<SharedFunctionInfo> shared,
                                   Handle<CodeT> code,
                                   BytecodeOffset osr_offset) {
  DCHECK(!osr_offset.IsNone());
  DCHECK(!isolate->serializer_enabled());
  DCHECK(CodeKindIsOptimizedJSFunction(code->kind()));

  Handle<OSROptimizedCodeCache> osr_cache(native_context->osr_code_cache(),
                                          isolate);

  if (shared->osr_code_cache_state() == kNotCached) {
    DCHECK_EQ(osr_cache->FindEntry(*shared, osr_offset), -1);
  } else if (osr_cache->FindEntry(*shared, osr_offset) != -1) {
    return;  // Already cached for a different JSFunction.
  }

  STATIC_ASSERT(kEntryLength == 3);
  int entry = -1;
  for (int index = 0; index < osr_cache->length(); index += kEntryLength) {
    if (osr_cache->Get(index + kSharedOffset)->IsCleared() ||
        osr_cache->Get(index + kCachedCodeOffset)->IsCleared()) {
      entry = index;
      break;
    }
  }

  if (entry == -1) {
    if (osr_cache->length() + kEntryLength <= kMaxLength) {
      entry = GrowOSRCache(isolate, native_context, &osr_cache);
    } else {
      // We reached max capacity and cannot grow further. Reuse an existing
      // entry.
      // TODO(mythria): We could use better mechanisms (like lru) to replace
      // existing entries. Though we don't expect this to be a common case, so
      // for now choosing to replace the first entry.
      osr_cache->ClearEntry(0, isolate);
      entry = 0;
    }
  }

  osr_cache->InitializeEntry(entry, *shared, *code, osr_offset);
}

void OSROptimizedCodeCache::Clear(Isolate* isolate,
                                  NativeContext native_context) {
  native_context.set_osr_code_cache(*OSROptimizedCodeCache::Empty(isolate));
}

void OSROptimizedCodeCache::Compact(Isolate* isolate,
                                    Handle<NativeContext> native_context) {
  Handle<OSROptimizedCodeCache> osr_cache(native_context->osr_code_cache(),
                                          isolate);

  // Re-adjust the cache so all the valid entries are on one side. This will
  // enable us to compress the cache if needed.
  int curr_valid_index = 0;
  for (int curr_index = 0; curr_index < osr_cache->length();
       curr_index += kEntryLength) {
    if (osr_cache->Get(curr_index + kSharedOffset)->IsCleared() ||
        osr_cache->Get(curr_index + kCachedCodeOffset)->IsCleared()) {
      continue;
    }
    if (curr_valid_index != curr_index) {
      osr_cache->MoveEntry(curr_index, curr_valid_index, isolate);
    }
    curr_valid_index += kEntryLength;
  }

  if (!NeedsTrimming(curr_valid_index, osr_cache->length())) return;

  Handle<OSROptimizedCodeCache> new_osr_cache =
      Handle<OSROptimizedCodeCache>::cast(isolate->factory()->NewWeakFixedArray(
          CapacityForLength(curr_valid_index), AllocationType::kOld));
  DCHECK_LT(new_osr_cache->length(), osr_cache->length());
  {
    DisallowGarbageCollection no_gc;
    new_osr_cache->CopyElements(isolate, 0, *osr_cache, 0,
                                new_osr_cache->length(),
                                new_osr_cache->GetWriteBarrierMode(no_gc));
  }
  native_context->set_osr_code_cache(*new_osr_cache);
}

CodeT OSROptimizedCodeCache::TryGet(SharedFunctionInfo shared,
                                    BytecodeOffset osr_offset,
                                    Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  int index = FindEntry(shared, osr_offset);
  if (index == -1) return {};

  CodeT code = GetCodeFromEntry(index);
  if (code.is_null()) {
    ClearEntry(index, isolate);
    return {};
  }

  DCHECK(code.is_optimized_code() && !code.marked_for_deoptimization());
  return code;
}

void OSROptimizedCodeCache::EvictDeoptimizedCode(Isolate* isolate) {
  // This is called from DeoptimizeMarkedCodeForContext that uses raw pointers
  // and hence the DisallowGarbageCollection scope here.
  DisallowGarbageCollection no_gc;
  for (int index = 0; index < length(); index += kEntryLength) {
    MaybeObject code_entry = Get(index + kCachedCodeOffset);
    HeapObject heap_object;
    if (!code_entry->GetHeapObject(&heap_object)) continue;

    CodeT code = CodeT::cast(heap_object);
    DCHECK(code.is_optimized_code());
    if (!code.marked_for_deoptimization()) continue;

    ClearEntry(index, isolate);
  }
}

std::vector<BytecodeOffset> OSROptimizedCodeCache::OsrOffsetsFor(
    SharedFunctionInfo shared) {
  DisallowGarbageCollection gc;

  const OSRCodeCacheStateOfSFI state = shared.osr_code_cache_state();
  if (state == kNotCached) return {};

  std::vector<BytecodeOffset> offsets;
  for (int index = 0; index < length(); index += kEntryLength) {
    if (GetSFIFromEntry(index) != shared) continue;
    offsets.emplace_back(GetBytecodeOffsetFromEntry(index));
    if (state == kCachedOnce) return offsets;
  }

  return offsets;
}

base::Optional<BytecodeOffset> OSROptimizedCodeCache::FirstOsrOffsetFor(
    SharedFunctionInfo shared) {
  DisallowGarbageCollection gc;

  const OSRCodeCacheStateOfSFI state = shared.osr_code_cache_state();
  if (state == kNotCached) return {};

  for (int index = 0; index < length(); index += kEntryLength) {
    if (GetSFIFromEntry(index) != shared) continue;
    return GetBytecodeOffsetFromEntry(index);
  }

  return {};
}

int OSROptimizedCodeCache::GrowOSRCache(
    Isolate* isolate, Handle<NativeContext> native_context,
    Handle<OSROptimizedCodeCache>* osr_cache) {
  int old_length = (*osr_cache)->length();
  int grow_by = CapacityForLength(old_length) - old_length;
  DCHECK_GT(grow_by, kEntryLength);
  *osr_cache = Handle<OSROptimizedCodeCache>::cast(
      isolate->factory()->CopyWeakFixedArrayAndGrow(*osr_cache, grow_by));
  for (int i = old_length; i < (*osr_cache)->length(); i++) {
    (*osr_cache)->Set(i, HeapObjectReference::ClearedValue(isolate));
  }
  native_context->set_osr_code_cache(**osr_cache);

  return old_length;
}

CodeT OSROptimizedCodeCache::GetCodeFromEntry(int index) {
  DCHECK_LE(index + OSRCodeCacheConstants::kEntryLength, length());
  DCHECK_EQ(index % kEntryLength, 0);
  HeapObject code_entry;
  Get(index + OSRCodeCacheConstants::kCachedCodeOffset)
      ->GetHeapObject(&code_entry);
  if (code_entry.is_null()) return CodeT();
  return CodeT::cast(code_entry);
}

SharedFunctionInfo OSROptimizedCodeCache::GetSFIFromEntry(int index) {
  DCHECK_LE(index + OSRCodeCacheConstants::kEntryLength, length());
  DCHECK_EQ(index % kEntryLength, 0);
  HeapObject sfi_entry;
  Get(index + OSRCodeCacheConstants::kSharedOffset)->GetHeapObject(&sfi_entry);
  return sfi_entry.is_null() ? SharedFunctionInfo()
                             : SharedFunctionInfo::cast(sfi_entry);
}

BytecodeOffset OSROptimizedCodeCache::GetBytecodeOffsetFromEntry(int index) {
  DCHECK_LE(index + OSRCodeCacheConstants::kEntryLength, length());
  DCHECK_EQ(index % kEntryLength, 0);
  Smi osr_offset_entry;
  Get(index + kOsrIdOffset)->ToSmi(&osr_offset_entry);
  return BytecodeOffset(osr_offset_entry.value());
}

int OSROptimizedCodeCache::FindEntry(SharedFunctionInfo shared,
                                     BytecodeOffset osr_offset) {
  DisallowGarbageCollection no_gc;
  DCHECK(!osr_offset.IsNone());
  for (int index = 0; index < length(); index += kEntryLength) {
    if (GetSFIFromEntry(index) != shared) continue;
    if (GetBytecodeOffsetFromEntry(index) != osr_offset) continue;
    return index;
  }
  return -1;
}

void OSROptimizedCodeCache::ClearEntry(int index, Isolate* isolate) {
  SharedFunctionInfo shared = GetSFIFromEntry(index);
  DCHECK_GT(shared.osr_code_cache_state(), kNotCached);
  if (V8_LIKELY(shared.osr_code_cache_state() == kCachedOnce)) {
    shared.set_osr_code_cache_state(kNotCached);
  } else if (shared.osr_code_cache_state() == kCachedMultiple) {
    int osr_code_cache_count = 0;
    for (int index = 0; index < length(); index += kEntryLength) {
      if (GetSFIFromEntry(index) == shared) {
        osr_code_cache_count++;
      }
    }
    if (osr_code_cache_count == 2) {
      shared.set_osr_code_cache_state(kCachedOnce);
    }
  }
  HeapObjectReference cleared_value =
      HeapObjectReference::ClearedValue(isolate);
  Set(index + OSRCodeCacheConstants::kSharedOffset, cleared_value);
  Set(index + OSRCodeCacheConstants::kCachedCodeOffset, cleared_value);
  Set(index + OSRCodeCacheConstants::kOsrIdOffset, cleared_value);
}

void OSROptimizedCodeCache::InitializeEntry(int entry,
                                            SharedFunctionInfo shared,
                                            CodeT code,
                                            BytecodeOffset osr_offset) {
  Set(entry + OSRCodeCacheConstants::kSharedOffset,
      HeapObjectReference::Weak(shared));
  HeapObjectReference weak_code_entry = HeapObjectReference::Weak(code);
  Set(entry + OSRCodeCacheConstants::kCachedCodeOffset, weak_code_entry);
  Set(entry + OSRCodeCacheConstants::kOsrIdOffset,
      MaybeObject::FromSmi(Smi::FromInt(osr_offset.ToInt())));
  if (V8_LIKELY(shared.osr_code_cache_state() == kNotCached)) {
    shared.set_osr_code_cache_state(kCachedOnce);
  } else if (shared.osr_code_cache_state() == kCachedOnce) {
    shared.set_osr_code_cache_state(kCachedMultiple);
  }
}

void OSROptimizedCodeCache::MoveEntry(int src, int dst, Isolate* isolate) {
  Set(dst + OSRCodeCacheConstants::kSharedOffset,
      Get(src + OSRCodeCacheConstants::kSharedOffset));
  Set(dst + OSRCodeCacheConstants::kCachedCodeOffset,
      Get(src + OSRCodeCacheConstants::kCachedCodeOffset));
  Set(dst + OSRCodeCacheConstants::kOsrIdOffset, Get(src + kOsrIdOffset));
  HeapObjectReference cleared_value =
      HeapObjectReference::ClearedValue(isolate);
  Set(src + OSRCodeCacheConstants::kSharedOffset, cleared_value);
  Set(src + OSRCodeCacheConstants::kCachedCodeOffset, cleared_value);
  Set(src + OSRCodeCacheConstants::kOsrIdOffset, cleared_value);
}

int OSROptimizedCodeCache::CapacityForLength(int curr_length) {
  // TODO(mythria): This is a randomly chosen heuristic and is not based on any
  // data. We may have to tune this later.
  if (curr_length == 0) return kInitialLength;
  if (curr_length * 2 > kMaxLength) return kMaxLength;
  return curr_length * 2;
}

bool OSROptimizedCodeCache::NeedsTrimming(int num_valid_entries,
                                          int curr_length) {
  return curr_length > kInitialLength && curr_length > num_valid_entries * 3;
}

MaybeObject OSROptimizedCodeCache::RawGetForTesting(int index) const {
  return WeakFixedArray::Get(index);
}

void OSROptimizedCodeCache::RawSetForTesting(int index, MaybeObject value) {
  WeakFixedArray::Set(index, value);
}

}  // namespace internal
}  // namespace v8
