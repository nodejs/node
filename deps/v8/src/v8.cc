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
#include "deoptimizer.h"
#include "heap-profiler.h"
#include "hydrogen.h"
#include "lithium-allocator.h"
#include "log.h"
#include "runtime-profiler.h"
#include "serialize.h"
#include "simulator.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

bool V8::is_running_ = false;
bool V8::has_been_setup_ = false;
bool V8::has_been_disposed_ = false;
bool V8::has_fatal_error_ = false;
bool V8::use_crankshaft_ = true;


bool V8::Initialize(Deserializer* des) {
  bool create_heap_objects = des == NULL;
  if (has_been_disposed_ || has_fatal_error_) return false;
  if (IsRunning()) return true;

#if defined(V8_TARGET_ARCH_ARM) && !defined(USE_ARM_EABI)
  use_crankshaft_ = false;
#else
  use_crankshaft_ = FLAG_crankshaft;
#endif

  // Peephole optimization might interfere with deoptimization.
  FLAG_peephole_optimization = !use_crankshaft_;
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

  CpuProfiler::Setup();
  HeapProfiler::Setup();

  // Setup the platform OS support.
  OS::Setup();

  // Initialize other runtime facilities
#if defined(USE_SIMULATOR)
#if defined(V8_TARGET_ARCH_ARM)
  Simulator::Initialize();
#elif defined(V8_TARGET_ARCH_MIPS)
  ::assembler::mips::Simulator::Initialize();
#endif
#endif

  { // NOLINT
    // Ensure that the thread has a valid stack guard.  The v8::Locker object
    // will ensure this too, but we don't have to use lockers if we are only
    // using one thread.
    ExecutionAccess lock;
    StackGuard::InitThread(lock);
  }

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
  Heap::SetStackLimits();

  // Setup the CPU support. Must be done after heap setup and after
  // any deserialization because we have to have the initial heap
  // objects in place for creating the code object used for probing.
  CPU::Setup();

  Deoptimizer::Setup();
  LAllocator::Setup();
  RuntimeProfiler::Setup();

  // If we are deserializing, log non-function code objects and compiled
  // functions found in the snapshot.
  if (des != NULL && FLAG_log_code) {
    HandleScope scope;
    LOG(LogCodeObjects());
    LOG(LogCompiledFunctions());
  }

  return true;
}


void V8::SetFatalError() {
  is_running_ = false;
  has_fatal_error_ = true;
}


void V8::TearDown() {
  if (!has_been_setup_ || has_been_disposed_) return;

  if (FLAG_time_hydrogen) HStatistics::Instance()->Print();

  // We must stop the logger before we tear down other components.
  Logger::EnsureTickerStopped();

  Deoptimizer::TearDown();

  if (FLAG_preemption) {
    v8::Locker locker;
    v8::Locker::StopPreemption();
  }

  Builtins::TearDown();
  Bootstrapper::TearDown();

  Top::TearDown();

  HeapProfiler::TearDown();
  CpuProfiler::TearDown();
  RuntimeProfiler::TearDown();

  Logger::TearDown();
  Heap::TearDown();

  is_running_ = false;
  has_been_disposed_ = true;
}


static uint32_t random_seed() {
  if (FLAG_random_seed == 0) {
    return random();
  }
  return FLAG_random_seed;
}


typedef struct {
  uint32_t hi;
  uint32_t lo;
} random_state;


// Random number generator using George Marsaglia's MWC algorithm.
static uint32_t random_base(random_state *state) {
  // Initialize seed using the system random(). If one of the seeds
  // should ever become zero again, or if random() returns zero, we
  // avoid getting stuck with zero bits in hi or lo by re-initializing
  // them on demand.
  if (state->hi == 0) state->hi = random_seed();
  if (state->lo == 0) state->lo = random_seed();

  // Mix the bits.
  state->hi = 36969 * (state->hi & 0xFFFF) + (state->hi >> 16);
  state->lo = 18273 * (state->lo & 0xFFFF) + (state->lo >> 16);
  return (state->hi << 16) + (state->lo & 0xFFFF);
}


// Used by JavaScript APIs
uint32_t V8::Random() {
  static random_state state = {0, 0};
  return random_base(&state);
}


// Used internally by the JIT and memory allocator for security
// purposes. So, we keep a different state to prevent informations
// leaks that could be used in an exploit.
uint32_t V8::RandomPrivate() {
  static random_state state = {0, 0};
  return random_base(&state);
}


bool V8::IdleNotification() {
  // Returning true tells the caller that there is no need to call
  // IdleNotification again.
  if (!FLAG_use_idle_notification) return true;

  // Tell the heap that it may want to adjust.
  return Heap::IdleNotification();
}


// Use a union type to avoid type-aliasing optimizations in GCC.
typedef union {
  double double_value;
  uint64_t uint64_t_value;
} double_int_union;


Object* V8::FillHeapNumberWithRandom(Object* heap_number) {
  uint64_t random_bits = Random();
  // Make a double* from address (heap_number + sizeof(double)).
  double_int_union* r = reinterpret_cast<double_int_union*>(
      reinterpret_cast<char*>(heap_number) +
      HeapNumber::kValueOffset - kHeapObjectTag);
  // Convert 32 random bits to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  const double binary_million = 1048576.0;
  r->double_value = binary_million;
  r->uint64_t_value |=  random_bits;
  r->double_value -= binary_million;

  return heap_number;
}

} }  // namespace v8::internal
