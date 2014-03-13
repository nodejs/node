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
#ifdef V8_USE_DEFAULT_PLATFORM
#include "libplatform/default-platform.h"
#endif
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
v8::Platform* V8::platform_ = NULL;


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

#ifdef V8_USE_DEFAULT_PLATFORM
  DefaultPlatform* platform = static_cast<DefaultPlatform*>(platform_);
  platform->SetThreadPoolSize(isolate->max_available_threads());
#endif

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

  Bootstrapper::TearDownExtensions();
  ElementsAccessor::TearDown();
  LOperand::TearDownCaches();
  ExternalReference::TearDownMathExpData();
  RegisteredExtension::UnregisterAll();
  Isolate::GlobalTearDown();

  delete call_completed_callbacks_;
  call_completed_callbacks_ = NULL;

  Sampler::TearDown();

#ifdef V8_USE_DEFAULT_PLATFORM
  DefaultPlatform* platform = static_cast<DefaultPlatform*>(platform_);
  platform_ = NULL;
  delete platform;
#endif
}


void V8::SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver) {
  StackFrame::SetReturnAddressLocationResolver(resolver);
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
  bool run_microtasks = isolate->autorun_microtasks() &&
                        isolate->microtask_pending();
  if (!has_call_completed_callbacks && !run_microtasks) return;

  HandleScopeImplementer* handle_scope_implementer =
      isolate->handle_scope_implementer();
  if (!handle_scope_implementer->CallDepthIsZero()) return;
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  handle_scope_implementer->IncrementCallDepth();
  if (run_microtasks) Execution::RunMicrotasks(isolate);
  if (has_call_completed_callbacks) {
    for (int i = 0; i < call_completed_callbacks_->length(); i++) {
      call_completed_callbacks_->at(i)();
    }
  }
  handle_scope_implementer->DecrementCallDepth();
}


void V8::RunMicrotasks(Isolate* isolate) {
  if (!isolate->microtask_pending())
    return;

  HandleScopeImplementer* handle_scope_implementer =
      isolate->handle_scope_implementer();
  ASSERT(handle_scope_implementer->CallDepthIsZero());

  // Increase call depth to prevent recursive callbacks.
  handle_scope_implementer->IncrementCallDepth();
  Execution::RunMicrotasks(isolate);
  handle_scope_implementer->DecrementCallDepth();
}


void V8::InitializeOncePerProcessImpl() {
  FlagList::EnforceFlagImplications();

  if (FLAG_predictable) {
    if (FLAG_random_seed == 0) {
      // Avoid random seeds in predictable mode.
      FLAG_random_seed = 12347;
    }
    FLAG_hash_seed = 0;
  }

  if (FLAG_stress_compaction) {
    FLAG_force_marking_deque_overflows = true;
    FLAG_gc_global = true;
    FLAG_max_new_space_size = (1 << (kPageSizeBits - 10)) * 2;
  }

#ifdef V8_USE_DEFAULT_PLATFORM
  platform_ = new DefaultPlatform;
#endif
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


void V8::InitializePlatform(v8::Platform* platform) {
  ASSERT(!platform_);
  ASSERT(platform);
  platform_ = platform;
}


void V8::ShutdownPlatform() {
  ASSERT(platform_);
  platform_ = NULL;
}


v8::Platform* V8::GetCurrentPlatform() {
  ASSERT(platform_);
  return platform_;
}

} }  // namespace v8::internal
