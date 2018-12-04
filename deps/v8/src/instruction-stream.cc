// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/instruction-stream.h"

#include "src/builtins/builtins.h"
#include "src/objects-inl.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

// static
bool InstructionStream::PcIsOffHeap(Isolate* isolate, Address pc) {
  if (FLAG_embedded_builtins) {
    const Address start = reinterpret_cast<Address>(isolate->embedded_blob());
    return start <= pc && pc < start + isolate->embedded_blob_size();
  } else {
    return false;
  }
}

// static
Code* InstructionStream::TryLookupCode(Isolate* isolate, Address address) {
  if (!PcIsOffHeap(isolate, address)) return nullptr;

  EmbeddedData d = EmbeddedData::FromBlob();

  int l = 0, r = Builtins::builtin_count;
  while (l < r) {
    const int mid = (l + r) / 2;
    Address start = d.InstructionStartOfBuiltin(mid);
    Address end = start + d.InstructionSizeOfBuiltin(mid);

    if (address < start) {
      r = mid;
    } else if (address >= end) {
      l = mid + 1;
    } else {
      return isolate->builtins()->builtin(mid);
    }
  }

  UNREACHABLE();
}

// static
void InstructionStream::CreateOffHeapInstructionStream(Isolate* isolate,
                                                       uint8_t** data,
                                                       uint32_t* size) {
  EmbeddedData d = EmbeddedData::FromIsolate(isolate);

  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  const uint32_t page_size =
      static_cast<uint32_t>(page_allocator->AllocatePageSize());
  const uint32_t allocated_size = RoundUp(d.size(), page_size);

  uint8_t* allocated_bytes = static_cast<uint8_t*>(
      AllocatePages(page_allocator, isolate->heap()->GetRandomMmapAddr(),
                    allocated_size, page_size, PageAllocator::kReadWrite));
  CHECK_NOT_NULL(allocated_bytes);

  std::memcpy(allocated_bytes, d.data(), d.size());
  CHECK(SetPermissions(page_allocator, allocated_bytes, allocated_size,
                       PageAllocator::kReadExecute));

  *data = allocated_bytes;
  *size = d.size();

  d.Dispose();
}

// static
void InstructionStream::FreeOffHeapInstructionStream(uint8_t* data,
                                                     uint32_t size) {
  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  const uint32_t page_size =
      static_cast<uint32_t>(page_allocator->AllocatePageSize());
  CHECK(FreePages(page_allocator, data, RoundUp(size, page_size)));
}

}  // namespace internal
}  // namespace v8
