// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "bootstrapper.h"
#include "debug.h"
#include "serialize.h"
#include "stub-cache.h"
#include "oprofile-agent.h"

#if V8_TARGET_ARCH_ARM
#include "arm/simulator-arm.h"
#endif

namespace v8 {
namespace internal {

bool V8::is_running_ = false;
bool V8::has_been_setup_ = false;
bool V8::has_been_disposed_ = false;
bool V8::has_fatal_error_ = false;

bool V8::Initialize(Deserializer *des) {
  bool create_heap_objects = des == NULL;
  if (has_been_disposed_ || has_fatal_error_) return false;
  if (IsRunning()) return true;

  is_running_ = true;
  has_been_setup_ = true;
  has_fatal_error_ = false;
  has_been_disposed_ = false;
#ifdef DEBUG
  // The initialization process does not handle memory exhaustion.
  DisallowAllocationFailure disallow_allocation_failure;
#endif

  // Enable logging before setting up the heap
  Logger::Setup();
  if (des) des->GetLog();

  // Setup the platform OS support.
  OS::Setup();

  // Initialize other runtime facilities
#if !V8_HOST_ARCH_ARM && V8_TARGET_ARCH_ARM
  ::assembler::arm::Simulator::Initialize();
#endif

  // Setup the object heap
  ASSERT(!Heap::HasBeenSetup());
  if (!Heap::Setup(create_heap_objects)) {
    SetFatalError();
    return false;
  }

  Bootstrapper::Initialize(create_heap_objects);
  Builtins::Setup(create_heap_objects);
  Top::Initialize();

  if (FLAG_preemption) {
    v8::Locker locker;
    v8::Locker::StartPreemption(100);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug::Setup(create_heap_objects);
#endif
  StubCache::Initialize(create_heap_objects);

  // If we are deserializing, read the state into the now-empty heap.
  if (des != NULL) {
    des->Deserialize();
    StubCache::Clear();
  }

  // Deserializing may put strange things in the root array's copy of the
  // stack guard.
  Heap::SetStackLimit(StackGuard::jslimit());

  // Setup the CPU support. Must be done after heap setup and after
  // any deserialization because we have to have the initial heap
  // objects in place for creating the code object used for probing.
  CPU::Setup();

  OProfileAgent::Initialize();

  return true;
}


void V8::SetFatalError() {
  is_running_ = false;
  has_fatal_error_ = true;
}


void V8::TearDown() {
  if (!has_been_setup_ || has_been_disposed_) return;

  OProfileAgent::TearDown();

  if (FLAG_preemption) {
    v8::Locker locker;
    v8::Locker::StopPreemption();
  }

  Builtins::TearDown();
  Bootstrapper::TearDown();

  Top::TearDown();

  Heap::TearDown();
  Logger::TearDown();

  is_running_ = false;
  has_been_disposed_ = true;
}


uint32_t V8::Random() {
  // Random number generator using George Marsaglia's MWC algorithm.
  static uint32_t hi = 0;
  static uint32_t lo = 0;

  // Initialize seed using the system random(). If one of the seeds
  // should ever become zero again, or if random() returns zero, we
  // avoid getting stuck with zero bits in hi or lo by re-initializing
  // them on demand.
  if (hi == 0) hi = random();
  if (lo == 0) lo = random();

  // Mix the bits.
  hi = 36969 * (hi & 0xFFFF) + (hi >> 16);
  lo = 18273 * (lo & 0xFFFF) + (lo >> 16);
  return (hi << 16) + (lo & 0xFFFF);
}


bool V8::IdleNotification(bool is_high_priority) {
  if (!FLAG_use_idle_notification) return false;
  // Ignore high priority instances of V8.
  if (is_high_priority) return false;

  // Tell the heap that it may want to adjust.
  return Heap::IdleNotification();
}


Smi* V8::RandomPositiveSmi() {
  uint32_t random = Random();
  ASSERT(IsPowerOf2(Smi::kMaxValue + 1));
  return Smi::FromInt(random & Smi::kMaxValue);
}

} }  // namespace v8::internal
