// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-stack.h"

#include "src/execution/isolate.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

RegExpStackScope::RegExpStackScope(Isolate* isolate)
    : regexp_stack_(isolate->regexp_stack()),
      old_sp_top_delta_(regexp_stack_->sp_top_delta()) {
  DCHECK(regexp_stack_->IsValid());
}

RegExpStackScope::~RegExpStackScope() {
  CHECK_EQ(old_sp_top_delta_, regexp_stack_->sp_top_delta());
  regexp_stack_->ResetIfEmpty();
}

RegExpStack::RegExpStack() : thread_local_(this) {}

RegExpStack::~RegExpStack() { thread_local_.FreeAndInvalidate(); }

// static
RegExpStack* RegExpStack::New() {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // TODO(426514762): RegExpStack objects must currently be accessible to
  // sandboxed code (which is unsafe). As such we need to register them as
  // sandbox extension memory, which requires allocating them on full OS pages.
  VirtualAddressSpace* vas = GetPlatformVirtualAddressSpace();
  CHECK_LT(sizeof(RegExpStack), vas->allocation_granularity());
  Address regexp_stack_memory = vas->AllocatePages(
      VirtualAddressSpace::kNoHint, vas->allocation_granularity(),
      vas->allocation_granularity(), PagePermissions::kReadWrite);
  SandboxHardwareSupport::RegisterUnsafeSandboxExtensionMemory(
      regexp_stack_memory, vas->allocation_granularity());
  return new (reinterpret_cast<void*>(regexp_stack_memory)) RegExpStack();
#else
  return new RegExpStack();
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

// static
void RegExpStack::Delete(RegExpStack* instance) {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // TODO(426514762): we currently allocate RegExpStack objects on full pages.
  instance->~RegExpStack();
  VirtualAddressSpace* vas = GetPlatformVirtualAddressSpace();
  Address page = reinterpret_cast<Address>(instance);
  DCHECK(IsAligned(page, vas->allocation_granularity()));
  vas->FreePages(page, vas->allocation_granularity());
#else
  delete instance;
#endif
}

char* RegExpStack::ArchiveStack(char* to) {
  if (!thread_local_.owns_memory_) {
    // Force dynamic stacks prior to archiving. Any growth will do. A dynamic
    // stack is needed because stack archival & restoration rely on `memory_`
    // pointing at a fixed-location backing store, whereas the static stack is
    // tied to a RegExpStack instance.
    EnsureCapacity(thread_local_.memory_size_ + 1);
    DCHECK(thread_local_.owns_memory_);
  }

  MemCopy(reinterpret_cast<void*>(to), &thread_local_, kThreadLocalSize);
  thread_local_ = ThreadLocal(this);
  return to + kThreadLocalSize;
}


char* RegExpStack::RestoreStack(char* from) {
  MemCopy(&thread_local_, reinterpret_cast<void*>(from), kThreadLocalSize);
  return from + kThreadLocalSize;
}

void RegExpStack::ThreadLocal::ResetToStaticStack(RegExpStack* regexp_stack) {
  DeleteDynamicStack();

  memory_ = regexp_stack->static_stack_;
  memory_top_ = regexp_stack->static_stack_ + kStaticStackSize;
  memory_size_ = kStaticStackSize;
  stack_pointer_ = memory_top_;
  limit_ = reinterpret_cast<Address>(regexp_stack->static_stack_) +
           kStackLimitSlackSize;
  owns_memory_ = false;
}

void RegExpStack::ThreadLocal::FreeAndInvalidate() {
  DeleteDynamicStack();

  // This stack may not be used after being freed. Just reset to invalid values
  // to ensure we don't accidentally use old memory areas.
  memory_ = nullptr;
  memory_top_ = nullptr;
  memory_size_ = 0;
  stack_pointer_ = nullptr;
  limit_ = kMemoryTop;
}

// static
uint8_t* RegExpStack::ThreadLocal::NewDynamicStack(size_t size) {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // Stack memory must be accessible to sandboxed code, so we must register it
  // as sandbox extension memory. As such, we need to allocate full OS pages.
  // TODO(426514762): determine if stack memory is always safe to be
  // corrupted by an attacker. If so, consider moving it into the sandbox.
  // TODO(426514762): if we're anyway switching this to full OS pages, would
  // there be a benefit from adding guard regions around the stack memory to
  // catch stack overflows and similar bugs?
  VirtualAddressSpace* vas = GetPlatformVirtualAddressSpace();
  size_t allocation_size = RoundUp(size, vas->allocation_granularity());
  uint8_t* new_memory = reinterpret_cast<uint8_t*>(vas->AllocatePages(
      VirtualAddressSpace::kNoHint, allocation_size,
      vas->allocation_granularity(), PagePermissions::kReadWrite));
  SandboxHardwareSupport::RegisterUnsafeSandboxExtensionMemory(
      reinterpret_cast<Address>(new_memory), allocation_size);
#else
  uint8_t* new_memory = NewArray<uint8_t>(size);
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  return new_memory;
}

void RegExpStack::ThreadLocal::DeleteDynamicStack() {
  if (owns_memory_) {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
    VirtualAddressSpace* vas = GetPlatformVirtualAddressSpace();
    size_t allocation_size =
        RoundUp(memory_size_, vas->allocation_granularity());
    vas->FreePages(reinterpret_cast<Address>(memory_), allocation_size);
#else
    DeleteArray(memory_);
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  }
}

Address RegExpStack::EnsureCapacity(size_t size) {
  if (size > kMaximumStackSize) return kNullAddress;
  if (thread_local_.memory_size_ < size) {
    if (size < kMinimumDynamicStackSize) size = kMinimumDynamicStackSize;
    uint8_t* new_memory = ThreadLocal::NewDynamicStack(size);
    if (thread_local_.memory_size_ > 0) {
      // Copy original memory into top of new memory.
      MemCopy(new_memory + size - thread_local_.memory_size_,
              thread_local_.memory_, thread_local_.memory_size_);
      thread_local_.DeleteDynamicStack();
    }
    ptrdiff_t delta = sp_top_delta();
    thread_local_.memory_ = new_memory;
    thread_local_.memory_top_ = new_memory + size;
    thread_local_.memory_size_ = size;
    thread_local_.stack_pointer_ = thread_local_.memory_top_ + delta;
    thread_local_.limit_ =
        reinterpret_cast<Address>(new_memory) + kStackLimitSlackSize;
    thread_local_.owns_memory_ = true;
  }
  return reinterpret_cast<Address>(thread_local_.memory_top_);
}


}  // namespace internal
}  // namespace v8
