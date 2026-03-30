// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ BEFORE CHANGING THIS FILE!
//
// This file contains code that is used both inside and outside the out of
// bounds trap handler. Because this code runs in a trap handler context,
// use extra care when modifying this file. Here are some rules to follow.
//
// 1. Do not introduce any new external dependencies. This file needs
//    to be self contained so it is easy to audit everything that a
//    trap handler might do.
//
// 2. Any changes must be reviewed by someone from the crash reporting
//    or security team. See OWNERS for suggested reviewers.
//
// For more information, see:
// https://docs.google.com/document/d/17y4kxuHFrVxAiuCP_FFtFA2HP5sNPsCD10KEx17Hz6M

#include "src/trap-handler/trap-handler-internal.h"

namespace v8 {
namespace internal {
namespace trap_handler {

#if defined(V8_OS_AIX)
__thread bool TrapHandlerGuard::is_active_ = 0;
#else
thread_local bool TrapHandlerGuard::is_active_ = 0;
#endif

size_t gNumCodeObjects = 0;
CodeProtectionInfoListEntry* gCodeObjects = nullptr;
SandboxRecord* gSandboxRecordsHead = nullptr;
std::atomic_size_t gRecoveredTrapCount = {0};
std::atomic<uintptr_t> gLandingPad = {0};

#if !defined(__cpp_lib_atomic_value_initialization) || \
    __cpp_lib_atomic_value_initialization < 201911L
std::atomic_flag MetadataLock::spinlock_ = ATOMIC_FLAG_INIT;
std::atomic_flag SandboxRecordsLock::spinlock_ = ATOMIC_FLAG_INIT;
#else
std::atomic_flag MetadataLock::spinlock_;
std::atomic_flag SandboxRecordsLock::spinlock_;
#endif

MetadataLock::MetadataLock() {
  // This lock is taken from inside the trap handler. As such, we must only
  // take this lock if the trap handler guard is active on this thread. This
  // way, we avoid a deadlock in case we cause a fault while holding the lock.
  TH_CHECK(TrapHandlerGuard::IsActiveOnCurrentThread());

  while (spinlock_.test_and_set(std::memory_order_acquire)) {
  }
}

MetadataLock::~MetadataLock() {
  TH_CHECK(TrapHandlerGuard::IsActiveOnCurrentThread());

  spinlock_.clear(std::memory_order_release);
}

SandboxRecordsLock::SandboxRecordsLock() {
  // This lock is taken from inside the trap handler. As such, we must only
  // take this lock if the trap handler guard is active on this thread. This
  // way, we avoid a deadlock in case we cause a fault while holding the lock.
  TH_CHECK(TrapHandlerGuard::IsActiveOnCurrentThread());

  while (spinlock_.test_and_set(std::memory_order_acquire)) {
  }
}

SandboxRecordsLock::~SandboxRecordsLock() {
  TH_CHECK(TrapHandlerGuard::IsActiveOnCurrentThread());

  spinlock_.clear(std::memory_order_release);
}

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
