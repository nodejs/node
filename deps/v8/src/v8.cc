// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include <fstream>

#include "src/api.h"
#include "src/base/atomicops.h"
#include "src/base/once.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/cpu-features.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "src/libsampler/sampler.h"
#include "src/objects-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/runtime-profiler.h"
#include "src/simulator.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

V8_DECLARE_ONCE(init_once);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
V8_DECLARE_ONCE(init_natives_once);
V8_DECLARE_ONCE(init_snapshot_once);
#endif

v8::Platform* V8::platform_ = nullptr;

bool V8::Initialize() {
  InitializeOncePerProcess();
  return true;
}


void V8::TearDown() {
  wasm::WasmEngine::GlobalTearDown();
#if defined(USE_SIMULATOR)
  Simulator::GlobalTearDown();
#endif
  CallDescriptors::TearDown();
  ElementsAccessor::TearDown();
  RegisteredExtension::UnregisterAll();
  FlagList::ResetAllFlags();  // Frees memory held by string arguments.
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

  if (FLAG_trace_turbo) {
    // Create an empty file shared by the process (e.g. the wasm engine).
    std::ofstream(Isolate::GetTurboCfgFileName(nullptr).c_str(),
                  std::ios_base::trunc);
  }

  // Do not expose wasm in jitless mode.
  //
  // Even in interpreter-only mode, wasm currently still creates executable
  // memory at runtime. Unexpose wasm until this changes.
  // The correctness fuzzers are a special case: many of their test cases are
  // built by fetching a random property from the the global object, and thus
  // the global object layout must not change between configs. That is why we
  // continue exposing wasm on correctness fuzzers even in jitless mode.
  // TODO(jgruber): Remove this once / if wasm can run without executable
  // memory.
  if (FLAG_jitless && !FLAG_abort_on_stack_or_string_length_overflow) {
    FLAG_expose_wasm = false;
  }

  base::OS::Initialize(FLAG_hard_abort, FLAG_gc_fake_mmap);

  if (FLAG_random_seed) SetRandomMmapSeed(FLAG_random_seed);

  Isolate::InitializeOncePerProcess();

#if defined(USE_SIMULATOR)
  Simulator::InitializeOncePerProcess();
#endif
  CpuFeatures::Probe(false);
  ElementsAccessor::InitializeOncePerProcess();
  Bootstrapper::InitializeOncePerProcess();
  CallDescriptors::InitializeOncePerProcess();
  wasm::WasmEngine::InitializeOncePerProcess();
}


void V8::InitializeOncePerProcess() {
  base::CallOnce(&init_once, &InitializeOncePerProcessImpl);
}


void V8::InitializePlatform(v8::Platform* platform) {
  CHECK(!platform_);
  CHECK(platform);
  platform_ = platform;
  v8::base::SetPrintStackTrace(platform_->GetStackTracePrinter());
  v8::tracing::TracingCategoryObserver::SetUp();
}


void V8::ShutdownPlatform() {
  CHECK(platform_);
  v8::tracing::TracingCategoryObserver::TearDown();
  v8::base::SetPrintStackTrace(nullptr);
  platform_ = nullptr;
}


v8::Platform* V8::GetCurrentPlatform() {
  v8::Platform* platform = reinterpret_cast<v8::Platform*>(
      base::Relaxed_Load(reinterpret_cast<base::AtomicWord*>(&platform_)));
  DCHECK(platform);
  return platform;
}

void V8::SetPlatformForTesting(v8::Platform* platform) {
  base::Relaxed_Store(reinterpret_cast<base::AtomicWord*>(&platform_),
                      reinterpret_cast<base::AtomicWord>(platform));
}

void V8::SetNativesBlob(StartupData* natives_blob) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  base::CallOnce(&init_natives_once, &SetNativesFromFile, natives_blob);
#else
  UNREACHABLE();
#endif
}


void V8::SetSnapshotBlob(StartupData* snapshot_blob) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  base::CallOnce(&init_snapshot_once, &SetSnapshotFromFile, snapshot_blob);
#else
  UNREACHABLE();
#endif
}
}  // namespace internal

// static
double Platform::SystemClockTimeMillis() {
  return base::OS::TimeCurrentMillis();
}
}  // namespace v8
