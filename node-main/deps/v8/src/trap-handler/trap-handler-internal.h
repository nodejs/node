// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_TRAP_HANDLER_INTERNAL_H_
#define V8_TRAP_HANDLER_TRAP_HANDLER_INTERNAL_H_

// This file should not be included (even transitively) by files outside of
// src/trap-handler.

#include "src/trap-handler/trap-handler.h"

#include <atomic>

namespace v8 {
namespace internal {
namespace trap_handler {

// On Windows, asan installs its own exception handler which maps shadow memory.
// Since our exception handler may be executed before the asan exception
// handler, we have to make sure that asan shadow memory is not accessed in
// various helper functions in this file.

// Mechanism to guard against nested faults and deadlocks in the trap handler.
//
// When we enter the trap handler or take a lock required by it, we use these
// scope objects to set a thread-local global flag, indicating that it is not
// safe to run the trap handler on the current thread. When that flag is set,
// we bail out early from the trap handler and thereby avoid handling nested
// faults (e.g. in case of corrupted trap handler data structures) or causing
// deadlocks (if a lock needed by the trap handler is held on the same thread).
class TrapHandlerGuard {
 public:
  TH_DISABLE_ASAN TrapHandlerGuard() {
    TH_CHECK(!is_active_);
    is_active_ = true;
  }
  TH_DISABLE_ASAN ~TrapHandlerGuard() { is_active_ = false; }

  TrapHandlerGuard(const TrapHandlerGuard&) = delete;
  void operator=(const TrapHandlerGuard&) = delete;

  TH_DISABLE_ASAN static bool IsActiveOnCurrentThread() { return is_active_; }

 private:
#if defined(V8_OS_AIX)
  // `thread_local` does not link on AIX:
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100641
  static __thread bool is_active_;
#else
  static thread_local bool is_active_;
#endif
};

// This describes a chunk of code that the trap handler will be able to handle
// faults in. {base} points to the beginning of the chunk, and {size} is the
// number of bytes in the code chunk. The remainder of the struct is a list of
// protected memory access instructions and an offset to a landing pad to handle
// faults on that instruction.
struct CodeProtectionInfo {
  uintptr_t base;
  size_t size;
  size_t num_protected_instructions;
  ProtectedInstructionData instructions[1];
};

class MetadataLock {
  static std::atomic_flag spinlock_;

 public:
  TH_DISABLE_ASAN MetadataLock();
  TH_DISABLE_ASAN ~MetadataLock();

  MetadataLock(const MetadataLock&) = delete;
  void operator=(const MetadataLock&) = delete;
};

// To enable constant time registration of handler data, we keep a free list of
// entries in the gCodeObjects table. Each entry contains a {next_free} field,
// which can be used to figure out where the next entry should be inserted.
// In order to avoid having to initialize all the links to start with, we use
// 0 to indicate that this is a fresh, never-used list entry and that therefore
// the next entry is known to be free. If {next_entry} is greater than zero,
// then {next_entry - 1} is the index that we should insert into next.
struct CodeProtectionInfoListEntry {
  CodeProtectionInfo* code_info;
  size_t next_free;
};

extern size_t gNumCodeObjects;
extern CodeProtectionInfoListEntry* gCodeObjects;

// This list describes sandboxes as bases and sizes.
struct SandboxRecord {
  uintptr_t base;
  size_t size;
  SandboxRecord* next;
};

class SandboxRecordsLock {
  static std::atomic_flag spinlock_;

 public:
  TH_DISABLE_ASAN SandboxRecordsLock();
  TH_DISABLE_ASAN ~SandboxRecordsLock();

  SandboxRecordsLock(const SandboxRecordsLock&) = delete;
  void operator=(const SandboxRecordsLock&) = delete;
};

extern SandboxRecord* gSandboxRecordsHead;

extern std::atomic_size_t gRecoveredTrapCount;

extern std::atomic<uintptr_t> gLandingPad;

// Searches the fault location table for an entry matching fault_addr. If found,
// returns true, otherwise, returns false.
TH_DISABLE_ASAN bool IsFaultAddressCovered(uintptr_t fault_addr);

// Checks whether the accessed memory is covered by the trap handler. In
// particular, when the V8 sandbox is enabled, only faulting accesses to memory
// inside the sandbox are handled by the trap handler since all Wasm memory
// objects are inside the sandbox.
TH_DISABLE_ASAN bool IsAccessedMemoryCovered(uintptr_t accessed_addr);

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_TRAP_HANDLER_INTERNAL_H_
