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
#include "objects.h"
#include "once.h"
#include "platform.h"
#include "sampler.h"
#include "runtime-profiler.h"
#include "serialize.h"
#include "store-buffer.h"

namespace v8 {
namespace internal {

V8_DECLARE_ONCE(init_once);

List<CallCompletedCallback>* V8::call_completed_callbacks_ = NULL;
v8::ArrayBuffer::Allocator* V8::array_buffer_allocator_ = NULL;


bool V8::Initialize(Deserializer* des) {
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

  Isolate* isolate = Isolate::Current();
  if (isolate->IsDead()) return false;
  if (isolate->IsInitialized()) return true;

  return isolate->Init(des);
}


void V8::TearDown() {
  Isolate* isolate = Isolate::Current();
  ASSERT(isolate->IsDefaultIsolate());
  if (!isolate->IsInitialized()) return;

  // The isolate has to be torn down before clearing the LOperand
  // caches so that the optimizing compiler thread (if running)
  // doesn't see an inconsistent view of the lithium instructions.
  isolate->TearDown();
  delete isolate;

  ElementsAccessor::TearDown();
  LOperand::TearDownCaches();
  ExternalReference::TearDownMathExpData();
  RegisteredExtension::UnregisterAll();
  Isolate::GlobalTearDown();

  delete call_completed_callbacks_;
  call_completed_callbacks_ = NULL;

  Sampler::TearDown();
}


void V8::SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver) {
  StackFrame::SetReturnAddressLocationResolver(resolver);
}


// Used by JavaScript APIs
uint32_t V8::Random(Context* context) {
  ASSERT(context->IsNativeContext());
  ByteArray* seed = context->random_seed();
  uint32_t* state = reinterpret_cast<uint32_t*>(seed->GetDataStartAddress());

  // When we get here, the RNG must have been initialized,
  // see the Genesis constructor in file bootstrapper.cc.
  ASSERT_NE(0, state[0]);
  ASSERT_NE(0, state[1]);

  // Mix the bits.  Never replaces state[i] with 0 if it is nonzero.
  state[0] = 18273 * (state[0] & 0xFFFF) + (state[0] >> 16);
  state[1] = 36969 * (state[1] & 0xFFFF) + (state[1] >> 16);

  return (state[0] << 14) + (state[1] & 0x3FFFF);
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
  bool has_call_completed_callbacks = call_completed_callbacks_ != NULL;
  bool observer_delivery_pending =
      FLAG_harmony_observation && isolate->observer_delivery_pending();
  if (!has_call_completed_callbacks && !observer_delivery_pending) return;
  HandleScopeImplementer* handle_scope_implementer =
      isolate->handle_scope_implementer();
  if (!handle_scope_implementer->CallDepthIsZero()) return;
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  handle_scope_implementer->IncrementCallDepth();
  if (observer_delivery_pending) {
    JSObject::DeliverChangeRecords(isolate);
  }
  if (has_call_completed_callbacks) {
    for (int i = 0; i < call_completed_callbacks_->length(); i++) {
      call_completed_callbacks_->at(i)();
    }
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
  FlagList::EnforceFlagImplications();
  if (FLAG_stress_compaction) {
    FLAG_force_marking_deque_overflows = true;
    FLAG_gc_global = true;
    FLAG_max_new_space_size = (1 << (kPageSizeBits - 10)) * 2;
  }

  if (FLAG_concurrent_recompilation &&
      (FLAG_trace_hydrogen || FLAG_trace_hydrogen_stubs)) {
    FLAG_concurrent_recompilation = false;
    PrintF("Concurrent recompilation has been disabled for tracing.\n");
  }

  if (FLAG_sweeper_threads <= 0) {
    if (FLAG_concurrent_sweeping) {
      FLAG_sweeper_threads = SystemThreadManager::
          NumberOfParallelSystemThreads(
              SystemThreadManager::CONCURRENT_SWEEPING);
    } else if (FLAG_parallel_sweeping) {
      FLAG_sweeper_threads = SystemThreadManager::
          NumberOfParallelSystemThreads(
              SystemThreadManager::PARALLEL_SWEEPING);
    }
    if (FLAG_sweeper_threads == 0) {
      FLAG_concurrent_sweeping = false;
      FLAG_parallel_sweeping = false;
    }
  } else if (!FLAG_concurrent_sweeping && !FLAG_parallel_sweeping) {
    FLAG_sweeper_threads = 0;
  }

  if (FLAG_parallel_marking) {
    if (FLAG_marking_threads <= 0) {
      FLAG_marking_threads = SystemThreadManager::
          NumberOfParallelSystemThreads(
              SystemThreadManager::PARALLEL_MARKING);
    }
    if (FLAG_marking_threads == 0) {
      FLAG_parallel_marking = false;
    }
  } else {
    FLAG_marking_threads = 0;
  }

  if (FLAG_concurrent_recompilation &&
      SystemThreadManager::NumberOfParallelSystemThreads(
          SystemThreadManager::PARALLEL_RECOMPILATION) == 0) {
    FLAG_concurrent_recompilation = false;
  }

  Sampler::SetUp();
  CPU::SetUp();
  OS::PostSetUp();
  ElementsAccessor::InitializeOncePerProcess();
  LOperand::SetUpCaches();
  SetUpJSCallerSavedCodeData();
  ExternalReference::SetUp();
  Bootstrapper::InitializeOncePerProcess();
}


void V8::InitializeOncePerProcess() {
  CallOnce(&init_once, &InitializeOncePerProcessImpl);
}

} }  // namespace v8::internal
