// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "assembler.h"
#include "isolate.h"
#include "elements.h"
#include "bootstrapper.h"
#include "debug.h"
#include "deoptimizer.h"
#include "frames.h"
#include "heap-profiler.h"
#include "hydrogen.h"
#include "lithium-allocator.h"
#include "log.h"
#include "once.h"
#include "platform.h"
#include "runtime-profiler.h"
#include "serialize.h"
#include "store-buffer.h"

namespace v8 {
namespace internal {

V8_DECLARE_ONCE(init_once);

bool V8::is_running_ = false;
bool V8::has_been_set_up_ = false;
bool V8::has_been_disposed_ = false;
bool V8::has_fatal_error_ = false;
bool V8::use_crankshaft_ = true;
List<CallCompletedCallback>* V8::call_completed_callbacks_ = NULL;

static LazyMutex entropy_mutex = LAZY_MUTEX_INITIALIZER;

static EntropySource entropy_source;


bool V8::Initialize(Deserializer* des) {
  FlagList::EnforceFlagImplications();

  InitializeOncePerProcess();

  // The current thread may not yet had entered an isolate to run.
  // Note the Isolate::Current() may be non-null because for various
  // initialization purposes an initializing thread may be assigned an isolate
  // but not actually enter it.
  if (i::Isolate::CurrentPerIsolateThreadData() == NULL) {
    i::Isolate::EnterDefaultIsolate();
  }

  ASSERT(i::Isolate::CurrentPerIsolateThreadData() != NULL);
  ASSERT(i::Isolate::CurrentPerIsolateThreadData()->thread_id().Equals(
           i::ThreadId::Current()));
  ASSERT(i::Isolate::CurrentPerIsolateThreadData()->isolate() ==
         i::Isolate::Current());

  if (IsDead()) return false;

  Isolate* isolate = Isolate::Current();
  if (isolate->IsInitialized()) return true;

  is_running_ = true;
  has_been_set_up_ = true;
  has_fatal_error_ = false;
  has_been_disposed_ = false;

  return isolate->Init(des);
}


void V8::SetFatalError() {
  is_running_ = false;
  has_fatal_error_ = true;
}


void V8::TearDown() {
  Isolate* isolate = Isolate::Current();
  ASSERT(isolate->IsDefaultIsolate());

  if (!has_been_set_up_ || has_been_disposed_) return;

  ElementsAccessor::TearDown();
  LOperand::TearDownCaches();
  RegisteredExtension::UnregisterAll();

  isolate->TearDown();
  delete isolate;

  is_running_ = false;
  has_been_disposed_ = true;

  delete call_completed_callbacks_;
  call_completed_callbacks_ = NULL;

  OS::TearDown();
}


static void seed_random(uint32_t* state) {
  for (int i = 0; i < 2; ++i) {
    if (FLAG_random_seed != 0) {
      state[i] = FLAG_random_seed;
    } else if (entropy_source != NULL) {
      uint32_t val;
      ScopedLock lock(entropy_mutex.Pointer());
      entropy_source(reinterpret_cast<unsigned char*>(&val), sizeof(uint32_t));
      state[i] = val;
    } else {
      state[i] = random();
    }
  }
}


// Random number generator using George Marsaglia's MWC algorithm.
static uint32_t random_base(uint32_t* state) {
  // Initialize seed using the system random().
  // No non-zero seed will ever become zero again.
  if (state[0] == 0) seed_random(state);

  // Mix the bits.  Never replaces state[i] with 0 if it is nonzero.
  state[0] = 18273 * (state[0] & 0xFFFF) + (state[0] >> 16);
  state[1] = 36969 * (state[1] & 0xFFFF) + (state[1] >> 16);

  return (state[0] << 14) + (state[1] & 0x3FFFF);
}


void V8::SetEntropySource(EntropySource source) {
  entropy_source = source;
}


void V8::SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver) {
  StackFrame::SetReturnAddressLocationResolver(resolver);
}


// Used by JavaScript APIs
uint32_t V8::Random(Context* context) {
  ASSERT(context->IsGlobalContext());
  ByteArray* seed = context->random_seed();
  return random_base(reinterpret_cast<uint32_t*>(seed->GetDataStartAddress()));
}


// Used internally by the JIT and memory allocator for security
// purposes. So, we keep a different state to prevent informations
// leaks that could be used in an exploit.
uint32_t V8::RandomPrivate(Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
  return random_base(isolate->private_random_seed());
}


bool V8::IdleNotification(int hint) {
  // Returning true tells the caller that there is no need to call
  // IdleNotification again.
  if (!FLAG_use_idle_notification) return true;

  // Tell the heap that it may want to adjust.
  return HEAP->IdleNotification(hint);
}


void V8::AddCallCompletedCallback(CallCompletedCallback callback) {
  if (call_completed_callbacks_ == NULL) {  // Lazy init.
    call_completed_callbacks_ = new List<CallCompletedCallback>();
  }
  for (int i = 0; i < call_completed_callbacks_->length(); i++) {
    if (callback == call_completed_callbacks_->at(i)) return;
  }
  call_completed_callbacks_->Add(callback);
}


void V8::RemoveCallCompletedCallback(CallCompletedCallback callback) {
  if (call_completed_callbacks_ == NULL) return;
  for (int i = 0; i < call_completed_callbacks_->length(); i++) {
    if (callback == call_completed_callbacks_->at(i)) {
      call_completed_callbacks_->Remove(i);
    }
  }
}


void V8::FireCallCompletedCallback(Isolate* isolate) {
  if (call_completed_callbacks_ == NULL) return;
  HandleScopeImplementer* handle_scope_implementer =
      isolate->handle_scope_implementer();
  if (!handle_scope_implementer->CallDepthIsZero()) return;
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  handle_scope_implementer->IncrementCallDepth();
  for (int i = 0; i < call_completed_callbacks_->length(); i++) {
    call_completed_callbacks_->at(i)();
  }
  handle_scope_implementer->DecrementCallDepth();
}


// Use a union type to avoid type-aliasing optimizations in GCC.
typedef union {
  double double_value;
  uint64_t uint64_t_value;
} double_int_union;


Object* V8::FillHeapNumberWithRandom(Object* heap_number,
                                     Context* context) {
  double_int_union r;
  uint64_t random_bits = Random(context);
  // Convert 32 random bits to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  static const double binary_million = 1048576.0;
  r.double_value = binary_million;
  r.uint64_t_value |= random_bits;
  r.double_value -= binary_million;

  HeapNumber::cast(heap_number)->set_value(r.double_value);
  return heap_number;
}

void V8::InitializeOncePerProcessImpl() {
  OS::SetUp();

  use_crankshaft_ = FLAG_crankshaft;

  if (Serializer::enabled()) {
    use_crankshaft_ = false;
  }

  CPU::SetUp();
  if (!CPU::SupportsCrankshaft()) {
    use_crankshaft_ = false;
  }

  OS::PostSetUp();

  RuntimeProfiler::GlobalSetUp();

  ElementsAccessor::InitializeOncePerProcess();

  if (FLAG_stress_compaction) {
    FLAG_force_marking_deque_overflows = true;
    FLAG_gc_global = true;
    FLAG_max_new_space_size = (1 << (kPageSizeBits - 10)) * 2;
  }

  LOperand::SetUpCaches();
  SetUpJSCallerSavedCodeData();
  SamplerRegistry::SetUp();
  ExternalReference::SetUp();
}

void V8::InitializeOncePerProcess() {
  CallOnce(&init_once, &InitializeOncePerProcessImpl);
}

} }  // namespace v8::internal
