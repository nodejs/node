// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate-inl.h"
#include "src/objects/code.h"
#include "src/objects/maybe-object.h"
#include "src/objects/shared-function-info.h"

#include "src/objects/osr-optimized-code-cache.h"

namespace v8 {
namespace internal {

const int OSROptimizedCodeCache::kInitialLength;
const int OSROptimizedCodeCache::kMaxLength;

void OSROptimizedCodeCache::AddOptimizedCode(
    Handle<NativeContext> native_context, Handle<SharedFunctionInfo> shared,
    Handle<Code> code, BailoutId osr_offset) {
  DCHECK(!osr_offset.IsNone());
  DCHECK(CodeKindIsOptimizedJSFunction(code->kind()));
  STATIC_ASSERT(kEntryLength == 3);
  Isolate* isolate = native_context->GetIsolate();
  DCHECK(!isolate->serializer_enabled());

  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), isolate);

  DCHECK_EQ(osr_cache->FindEntry(shared, osr_offset), -1);
  int entry = -1;
  for (int index = 0; index < osr_cache->length(); index += kEntryLength) {
    if (osr_cache->Get(index + kSharedOffset)->IsCleared() ||
        osr_cache->Get(index + kCachedCodeOffset)->IsCleared()) {
      entry = index;
      break;
    }
  }

  if (entry == -1 && osr_cache->length() + kEntryLength <= kMaxLength) {
    entry = GrowOSRCache(native_context, &osr_cache);
  } else if (entry == -1) {
    // We reached max capacity and cannot grow further. Reuse an existing entry.
    // TODO(mythria): We could use better mechanisms (like lru) to replace
    // existing entries. Though we don't expect this to be a common case, so
    // for now choosing to replace the first entry.
    entry = 0;
  }

  osr_cache->InitializeEntry(entry, *shared, *code, osr_offset);
}

void OSROptimizedCodeCache::Clear(NativeContext native_context) {
  native_context.set_osr_code_cache(
      *native_context.GetIsolate()->factory()->empty_weak_fixed_array());
}

void OSROptimizedCodeCache::Compact(Handle<NativeContext> native_context) {
  Handle<OSROptimizedCodeCache> osr_cache(
      native_context->GetOSROptimizedCodeCache(), native_context->GetIsolate());
  Isolate* isolate = native_context->GetIsolate();

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
    new_osr_cache->CopyElements(native_context->GetIsolate(), 0, *osr_cache, 0,
                                new_osr_cache->length(),
                                new_osr_cache->GetWriteBarrierMode(no_gc));
  }
  native_context->set_osr_code_cache(*new_osr_cache);
}

Code OSROptimizedCodeCache::GetOptimizedCode(Handle<SharedFunctionInfo> shared,
                                             BailoutId osr_offset,
                                             Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  int index = FindEntry(shared, osr_offset);
  if (index == -1) return Code();
  Code code = GetCodeFromEntry(index);
  if (code.is_null()) {
    ClearEntry(index, isolate);
    return code;
  }
  DCHECK(code.is_optimized_code() && !code.marked_for_deoptimization());
  return code;
}

void OSROptimizedCodeCache::EvictMarkedCode(Isolate* isolate) {
  // This is called from DeoptimizeMarkedCodeForContext that uses raw pointers
  // and hence the DisallowGarbageCollection scope here.
  DisallowGarbageCollection no_gc;
  for (int index = 0; index < length(); index += kEntryLength) {
    MaybeObject code_entry = Get(index + kCachedCodeOffset);
    HeapObject heap_object;
    if (!code_entry->GetHeapObject(&heap_object)) continue;

    DCHECK(heap_object.IsCode());
    DCHECK(Code::cast(heap_object).is_optimized_code());
    if (!Code::cast(heap_object).marked_for_deoptimization()) continue;

    ClearEntry(index, isolate);
  }
}

int OSROptimizedCodeCache::GrowOSRCache(
    Handle<NativeContext> native_context,
    Handle<OSROptimizedCodeCache>* osr_cache) {
  Isolate* isolate = native_context->GetIsolate();
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

Code OSROptimizedCodeCache::GetCodeFromEntry(int index) {
  DCHECK_LE(index + OSRCodeCacheConstants::kEntryLength, length());
  DCHECK_EQ(index % kEntryLength, 0);
  HeapObject code_entry;
  Get(index + OSRCodeCacheConstants::kCachedCodeOffset)
      ->GetHeapObject(&code_entry);
  return code_entry.is_null() ? Code() : Code::cast(code_entry);
}

SharedFunctionInfo OSROptimizedCodeCache::GetSFIFromEntry(int index) {
  DCHECK_LE(index + OSRCodeCacheConstants::kEntryLength, length());
  DCHECK_EQ(index % kEntryLength, 0);
  HeapObject sfi_entry;
  Get(index + OSRCodeCacheConstants::kSharedOffset)->GetHeapObject(&sfi_entry);
  return sfi_entry.is_null() ? SharedFunctionInfo()
                             : SharedFunctionInfo::cast(sfi_entry);
}

BailoutId OSROptimizedCodeCache::GetBailoutIdFromEntry(int index) {
  DCHECK_LE(index + OSRCodeCacheConstants::kEntryLength, length());
  DCHECK_EQ(index % kEntryLength, 0);
  Smi osr_offset_entry;
  Get(index + kOsrIdOffset)->ToSmi(&osr_offset_entry);
  return BailoutId(osr_offset_entry.value());
}

int OSROptimizedCodeCache::FindEntry(Handle<SharedFunctionInfo> shared,
                                     BailoutId osr_offset) {
  DisallowGarbageCollection no_gc;
  DCHECK(!osr_offset.IsNone());
  for (int index = 0; index < length(); index += kEntryLength) {
    if (GetSFIFromEntry(index) != *shared) continue;
    if (GetBailoutIdFromEntry(index) != osr_offset) continue;
    return index;
  }
  return -1;
}

void OSROptimizedCodeCache::ClearEntry(int index, Isolate* isolate) {
  Set(index + OSRCodeCacheConstants::kSharedOffset,
      HeapObjectReference::ClearedValue(isolate));
  Set(index + OSRCodeCacheConstants::kCachedCodeOffset,
      HeapObjectReference::ClearedValue(isolate));
  Set(index + OSRCodeCacheConstants::kOsrIdOffset,
      HeapObjectReference::ClearedValue(isolate));
}

void OSROptimizedCodeCache::InitializeEntry(int entry,
                                            SharedFunctionInfo shared,
                                            Code code, BailoutId osr_offset) {
  Set(entry + OSRCodeCacheConstants::kSharedOffset,
      HeapObjectReference::Weak(shared));
  Set(entry + OSRCodeCacheConstants::kCachedCodeOffset,
      HeapObjectReference::Weak(code));
  Set(entry + OSRCodeCacheConstants::kOsrIdOffset,
      MaybeObject::FromSmi(Smi::FromInt(osr_offset.ToInt())));
}

void OSROptimizedCodeCache::MoveEntry(int src, int dst, Isolate* isolate) {
  Set(dst + OSRCodeCacheConstants::kSharedOffset,
      Get(src + OSRCodeCacheConstants::kSharedOffset));
  Set(dst + OSRCodeCacheConstants::kCachedCodeOffset,
      Get(src + OSRCodeCacheConstants::kCachedCodeOffset));
  Set(dst + OSRCodeCacheConstants::kOsrIdOffset, Get(src + kOsrIdOffset));
  ClearEntry(src, isolate);
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

}  // namespace internal
}  // namespace v8
