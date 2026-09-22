// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8config.h"  // For V8_TARGET_OS_LINUX.

#if V8_TARGET_OS_LINUX
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
// `sys/mman.h defines `MAP_TYPE`, but `MAP_TYPE` also gets defined within V8.
// Since we don't need `sys/mman.h`'s `MAP_TYPE`, we undefine it immediately
// after the `#include`.
#undef MAP_TYPE
#endif  // V8_TARGET_OS_LINUX

#include "include/cppgc/allocation.h"
#include "include/v8-cppgc.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles-inl.h"
#include "src/wasm/wasm-memory-map-descriptor.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8::internal::wasm {

WasmMemoryMapDescriptor::WasmMemoryMapDescriptor(PlatformFileDescriptor fd,
                                                 size_t size,
                                                 FdOwnership fd_ownership)
    : file_descriptor_(fd), size_(size), fd_ownership_(fd_ownership) {}

WasmMemoryMapDescriptor::~WasmMemoryMapDescriptor() {
  mapped_memory_.Reset();
#if V8_TARGET_OS_LINUX
  // TODO(crbug.com/339678654): Define a constant somewhere in the platform
  // instead of hardcoding -1 here.
  if (fd_ownership_ == FdOwnership::kAnonymousFdOwnedByV8 &&
      file_descriptor_ != -1) {
    close(file_descriptor_);
  }
#else
  USE(fd_ownership_);
#endif
}

void WasmMemoryMapDescriptor::Trace(cppgc::Visitor* visitor) const {
  v8::Object::Wrappable::Trace(visitor);
}

// static
v8::Local<v8::Object> WasmMemoryMapDescriptor::NewFromFileDescriptor(
    v8::Isolate* isolate, PlatformFileDescriptor fd, size_t size,
    v8::Local<v8::Object> wrapper, FdOwnership fd_ownership) {
  CHECK(v8_flags.wasm_memory_control);
  auto* cpp_descriptor = cppgc::MakeGarbageCollected<WasmMemoryMapDescriptor>(
      isolate->GetCppHeap()->GetAllocationHandle(), fd, size, fd_ownership);

  if (wrapper.IsEmpty()) {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    v8::Local<v8::ObjectTemplate> templ = Utils::ToLocal(direct_handle(
        i_isolate->native_context()->wasm_memory_map_descriptor_template(),
        i_isolate));

    wrapper = templ->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
  }

  v8::Object::Wrap<kPointerTag>(isolate, wrapper, cpp_descriptor);

  return wrapper;
}

// static
v8::MaybeLocal<v8::Object> WasmMemoryMapDescriptor::NewFromAnonymous(
    v8::Isolate* isolate, size_t length, v8::Local<v8::Object> wrapper) {
  CHECK(v8_flags.wasm_memory_control);
#if V8_TARGET_OS_LINUX
  int fd = memfd_create("wasm_memory_map_descriptor", MFD_CLOEXEC);
  if (fd == -1) return {};
  if (ftruncate(fd, length) == -1) {
    close(fd);
    return {};
  }
  return NewFromFileDescriptor(isolate, fd, length, wrapper,
                               FdOwnership::kAnonymousFdOwnedByV8);
#else
  return {};
#endif
}

size_t WasmMemoryMapDescriptor::Map(v8::Isolate* isolate,
                                    DirectHandle<WasmMemoryObject> memory,
                                    size_t offset) {
  CHECK(v8_flags.wasm_memory_control);
  if (!mapped_memory_.IsEmpty()) {
    isolate->ThrowError("WasmMemoryMapDescriptor::Map called more than once");
    return 0;
  }
  Managed<BackingStore>::Ptr backing_store = memory->backing_store();
  if (backing_store->is_shared()) {
    // TODO(ahaas): Handle concurrent calls to `MapDescriptor`. To prevent
    // concurrency issues, we disable `MapDescriptor` for shared wasm memories
    // so far.
    return 0;
  }
  if (memory->is_memory64()) {
    // TODO(339678654, ahaas): Handle memory64. It may already be possible, but
    // we need to test it.
    return 0;
  }

#if V8_TARGET_OS_LINUX
  uint8_t* target =
      reinterpret_cast<uint8_t*>(backing_store->buffer_start()) + offset;

  struct stat stat_for_size;
  if (fstat(this->file_descriptor(), &stat_for_size) == -1) {
    // Could not determine file size.
    return 0;
  }
  size_t size = RoundUp(stat_for_size.st_size,
                        GetArrayBufferPageAllocator()->AllocatePageSize());

  if (size + offset < size) {
    // Overflow
    return 0;
  }
  if (size + offset > backing_store->byte_length()) {
    return 0;
  }

  void* ret_val = mmap(target, size, PROT_READ | PROT_WRITE,
                       MAP_FIXED | MAP_SHARED, this->file_descriptor(), 0);
  if (ret_val == MAP_FAILED) {
    return 0;
  }
  CHECK_EQ(ret_val, target);
  mapped_memory_.Reset(isolate, Utils::ToLocal(memory));
  mapped_memory_.SetWeak();
  size_ = size;
  offset_ = offset;
  return size;
#else
  return 0;
#endif
}

bool WasmMemoryMapDescriptor::Unmap(v8::Isolate* isolate) {
  CHECK(v8_flags.wasm_memory_control);
  DisallowGarbageCollection no_gc;
  if (mapped_memory_.IsEmpty()) return false;

  i::DirectHandle<i::WasmMemoryObject> memory =
      Utils::OpenDirectHandle(*mapped_memory_.Get(isolate));
  mapped_memory_.Reset();
  CHECK(!memory.is_null());

  Managed<BackingStore>::Ptr backing_store = memory->backing_store();

  // The following checks already passed during `MapDescriptor`, and they should
  // still pass.
  CHECK(!memory->is_memory64());
  CHECK(!backing_store->is_shared());
  CHECK_EQ(size_ % GetArrayBufferPageAllocator()->AllocatePageSize(), 0);
  CHECK_GE(size_ + offset_, size_);
  CHECK_LE(size_ + offset_, backing_store->byte_length());

#if V8_TARGET_OS_LINUX
  uint8_t* target =
      reinterpret_cast<uint8_t*>(backing_store->buffer_start()) + offset_;

  void* ret_val = mmap(target, size_, PROT_READ | PROT_WRITE,
                       MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  CHECK_NE(ret_val, MAP_FAILED);
  CHECK_EQ(ret_val, target);
  return true;
#else
  return false;
#endif
}

}  // namespace v8::internal::wasm
