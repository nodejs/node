// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/assembler.h"
#include "src/base/once.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/frames.h"
#include "src/heap/store-buffer.h"
#include "src/heap-profiler.h"
#include "src/hydrogen.h"
#include "src/isolate.h"
#include "src/lithium-allocator.h"
#include "src/objects.h"
#include "src/runtime-profiler.h"
#include "src/sampler.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/serialize.h"
#include "src/snapshot/snapshot.h"


namespace v8 {
namespace internal {

V8_DECLARE_ONCE(init_once);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
V8_DECLARE_ONCE(init_natives_once);
V8_DECLARE_ONCE(init_snapshot_once);
#endif

v8::ArrayBuffer::Allocator* V8::array_buffer_allocator_ = NULL;
v8::Platform* V8::platform_ = NULL;


bool V8::Initialize() {
  InitializeOncePerProcess();
  return true;
}


void V8::TearDown() {
  Bootstrapper::TearDownExtensions();
  ElementsAccessor::TearDown();
  LOperand::TearDownCaches();
  ExternalReference::TearDownMathExpData();
  RegisteredExtension::UnregisterAll();
  Isolate::GlobalTearDown();
  Sampler::TearDown();
  FlagList::ResetAllFlags();  // Frees memory held by string arguments.
}


void V8::SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver) {
  StackFrame::SetReturnAddressLocationResolver(resolver);
}


void V8::InitializeOncePerProcessImpl() {
  FlagList::EnforceFlagImplications();

  if (FLAG_predictable && FLAG_random_seed == 0) {
    // Avoid random seeds in predictable mode.
    FLAG_random_seed = 12347;
  }

  if (FLAG_stress_compaction) {
    FLAG_force_marking_deque_overflows = true;
    FLAG_gc_global = true;
    FLAG_max_semi_space_size = 1;
  }

  base::OS::Initialize(FLAG_random_seed, FLAG_hard_abort, FLAG_gc_fake_mmap);

  Isolate::InitializeOncePerProcess();

  Sampler::SetUp();
  CpuFeatures::Probe(false);
  init_memcopy_functions();
  // The custom exp implementation needs 16KB of lookup data; initialize it
  // on demand.
  init_fast_sqrt_function();
#ifdef _WIN64
  init_modulo_function();
#endif
  ElementsAccessor::InitializeOncePerProcess();
  LOperand::SetUpCaches();
  SetUpJSCallerSavedCodeData();
  ExternalReference::SetUp();
  Bootstrapper::InitializeOncePerProcess();
}


void V8::InitializeOncePerProcess() {
  base::CallOnce(&init_once, &InitializeOncePerProcessImpl);
}


void V8::InitializePlatform(v8::Platform* platform) {
  CHECK(!platform_);
  CHECK(platform);
  platform_ = platform;
}


void V8::ShutdownPlatform() {
  CHECK(platform_);
  platform_ = NULL;
}


v8::Platform* V8::GetCurrentPlatform() {
  DCHECK(platform_);
  return platform_;
}


void V8::SetNativesBlob(StartupData* natives_blob) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  base::CallOnce(&init_natives_once, &SetNativesFromFile, natives_blob);
#else
  CHECK(false);
#endif
}


void V8::SetSnapshotBlob(StartupData* snapshot_blob) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  base::CallOnce(&init_snapshot_once, &SetSnapshotFromFile, snapshot_blob);
#else
  CHECK(false);
#endif
}
} }  // namespace v8::internal
