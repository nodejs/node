// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"

#include <fstream>

#include "include/cppgc/platform.h"
#include "src/api/api.h"
#include "src/base/atomicops.h"
#include "src/base/once.h"
#include "src/base/platform/platform.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/interface-descriptors.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/execution/simulator.h"
#include "src/init/bootstrapper.h"
#include "src/libsampler/sampler.h"
#include "src/objects/elements.h"
#include "src/objects/objects-inl.h"
#include "src/profiler/heap-profiler.h"
#include "src/sandbox/sandbox.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/tracing-category-observer.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-engine.h"
#endif  // V8_ENABLE_WEBASSEMBLY

#if defined(V8_OS_WIN) && defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
#include "src/diagnostics/system-jit-win.h"
#endif

namespace v8 {
namespace internal {

v8::Platform* V8::platform_ = nullptr;

namespace {
enum class V8StartupState {
  kIdle,
  kPlatformInitializing,
  kPlatformInitialized,
  kV8Initializing,
  kV8Initialized,
  kV8Disposing,
  kV8Disposed,
  kPlatformDisposing,
  kPlatformDisposed
};

std::atomic<V8StartupState> v8_startup_state_(V8StartupState::kIdle);

void AdvanceStartupState(V8StartupState expected_next_state) {
  V8StartupState current_state = v8_startup_state_;
  CHECK_NE(current_state, V8StartupState::kPlatformDisposed);
  V8StartupState next_state =
      static_cast<V8StartupState>(static_cast<int>(current_state) + 1);
  if (next_state != expected_next_state) {
    // Ensure the following order:
    // v8::V8::InitializePlatform(platform);
    // v8::V8::Initialize();
    // v8::Isolate* isolate = v8::Isolate::New(...);
    // ...
    // isolate->Dispose();
    // v8::V8::Dispose();
    // v8::V8::DisposePlatform();
    FATAL("Wrong initialization order: got %d expected %d!",
          static_cast<int>(current_state), static_cast<int>(next_state));
  }
  if (!v8_startup_state_.compare_exchange_strong(current_state, next_state)) {
    FATAL(
        "Multiple threads are initializating V8 in the wrong order: expected "
        "%d got %d!",
        static_cast<int>(current_state),
        static_cast<int>(v8_startup_state_.load()));
  }
}

}  // namespace

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
V8_DECLARE_ONCE(init_snapshot_once);
#endif

void V8::InitializePlatform(v8::Platform* platform) {
  AdvanceStartupState(V8StartupState::kPlatformInitializing);
  CHECK(!platform_);
  CHECK_NOT_NULL(platform);
  platform_ = platform;
  v8::base::SetPrintStackTrace(platform_->GetStackTracePrinter());
  v8::tracing::TracingCategoryObserver::SetUp();
#if defined(V8_OS_WIN) && defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
  if (FLAG_enable_system_instrumentation) {
    // TODO(sartang@microsoft.com): Move to platform specific diagnostics object
    v8::internal::ETWJITInterface::Register();
  }
#endif
  AdvanceStartupState(V8StartupState::kPlatformInitialized);
}

#ifdef V8_SANDBOX
bool V8::InitializeSandbox() {
  // Platform must have been initialized already.
  CHECK(platform_);
  v8::VirtualAddressSpace* vas = GetPlatformVirtualAddressSpace();
  return GetProcessWideSandbox()->Initialize(vas);
}
#endif  // V8_SANDBOX

#define DISABLE_FLAG(flag)                                                    \
  if (FLAG_##flag) {                                                          \
    PrintF(stderr,                                                            \
           "Warning: disabling flag --" #flag " due to conflicting flags\n"); \
    FLAG_##flag = false;                                                      \
  }

void V8::Initialize() {
  AdvanceStartupState(V8StartupState::kV8Initializing);
  CHECK(platform_);

#ifdef V8_SANDBOX
  if (!GetProcessWideSandbox()->is_initialized()) {
    // For now, we still allow the cage to be disabled even if V8 was compiled
    // with V8_SANDBOX. This will eventually be forbidden.
    CHECK(kAllowBackingStoresOutsideSandbox);
    GetProcessWideSandbox()->Disable();
  }
#endif

  // Update logging information before enforcing flag implications.
  bool* log_all_flags[] = {&FLAG_turbo_profiling_log_builtins,
                           &FLAG_log_all,
                           &FLAG_log_code,
                           &FLAG_log_code_disassemble,
                           &FLAG_log_source_code,
                           &FLAG_log_function_events,
                           &FLAG_log_internal_timer_events,
                           &FLAG_log_deopt,
                           &FLAG_log_ic,
                           &FLAG_log_maps};
  if (FLAG_log_all) {
    // Enable all logging flags
    for (auto* flag : log_all_flags) {
      *flag = true;
    }
    FLAG_log = true;
  } else if (!FLAG_log) {
    // Enable --log if any log flag is set.
    for (const auto* flag : log_all_flags) {
      if (!*flag) continue;
      FLAG_log = true;
      break;
    }
    // Profiling flags depend on logging.
    FLAG_log |= FLAG_perf_prof || FLAG_perf_basic_prof || FLAG_ll_prof ||
                FLAG_prof || FLAG_prof_cpp;
    FLAG_log |= FLAG_gdbjit;
#if defined(V8_OS_WIN) && defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
    FLAG_log |= FLAG_enable_system_instrumentation;
#endif
  }

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
#if V8_ENABLE_WEBASSEMBLY
  if (FLAG_jitless && !FLAG_correctness_fuzzer_suppressions) {
    DISABLE_FLAG(expose_wasm);
  }
#endif

  // When fuzzing and concurrent compilation is enabled, disable Turbofan
  // tracing flags since reading/printing heap state is not thread-safe and
  // leads to false positives on TSAN bots.
  // TODO(chromium:1205289): Teach relevant fuzzers to not pass TF tracing
  // flags instead, and remove this section.
  if (FLAG_fuzzing && FLAG_concurrent_recompilation) {
    DISABLE_FLAG(trace_turbo);
    DISABLE_FLAG(trace_turbo_graph);
    DISABLE_FLAG(trace_turbo_scheduled);
    DISABLE_FLAG(trace_turbo_reduction);
    DISABLE_FLAG(trace_turbo_trimming);
    DISABLE_FLAG(trace_turbo_jt);
    DISABLE_FLAG(trace_turbo_ceq);
    DISABLE_FLAG(trace_turbo_loop);
    DISABLE_FLAG(trace_turbo_alloc);
    DISABLE_FLAG(trace_all_uses);
    DISABLE_FLAG(trace_representation);
    DISABLE_FLAG(trace_turbo_stack_accesses);
  }

  // The --jitless and --interpreted-frames-native-stack flags are incompatible
  // since the latter requires code generation while the former prohibits code
  // generation.
  CHECK(!FLAG_interpreted_frames_native_stack || !FLAG_jitless);

  base::OS::Initialize(FLAG_hard_abort, FLAG_gc_fake_mmap);

  if (FLAG_random_seed) SetRandomMmapSeed(FLAG_random_seed);

  if (FLAG_print_flag_values) FlagList::PrintValues();

  // Initialize the default FlagList::Hash
  FlagList::Hash();

#if defined(V8_USE_PERFETTO)
  if (perfetto::Tracing::IsInitialized()) TrackEvent::Register();
#endif
  IsolateAllocator::InitializeOncePerProcess();
  Isolate::InitializeOncePerProcess();

#if defined(USE_SIMULATOR)
  Simulator::InitializeOncePerProcess();
#endif
  CpuFeatures::Probe(false);
  ElementsAccessor::InitializeOncePerProcess();
  Bootstrapper::InitializeOncePerProcess();
  CallDescriptors::InitializeOncePerProcess();
#if V8_ENABLE_WEBASSEMBLY
  wasm::WasmEngine::InitializeOncePerProcess();
#endif  // V8_ENABLE_WEBASSEMBLY

  ExternalReferenceTable::InitializeOncePerProcess();

  AdvanceStartupState(V8StartupState::kV8Initialized);
}

#undef DISABLE_FLAG

void V8::Dispose() {
  AdvanceStartupState(V8StartupState::kV8Disposing);
  CHECK(platform_);
#if V8_ENABLE_WEBASSEMBLY
  wasm::WasmEngine::GlobalTearDown();
#endif  // V8_ENABLE_WEBASSEMBLY
#if defined(USE_SIMULATOR)
  Simulator::GlobalTearDown();
#endif
  CallDescriptors::TearDown();
  ElementsAccessor::TearDown();
  RegisteredExtension::UnregisterAll();
  Isolate::DisposeOncePerProcess();
  FlagList::ResetAllFlags();  // Frees memory held by string arguments.
  AdvanceStartupState(V8StartupState::kV8Disposed);
}

void V8::DisposePlatform() {
  AdvanceStartupState(V8StartupState::kPlatformDisposing);
  CHECK(platform_);
#if defined(V8_OS_WIN) && defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
  if (FLAG_enable_system_instrumentation) {
    v8::internal::ETWJITInterface::Unregister();
  }
#endif
  v8::tracing::TracingCategoryObserver::TearDown();
  v8::base::SetPrintStackTrace(nullptr);

#ifdef V8_SANDBOX
  // TODO(chromium:1218005) alternatively, this could move to its own
  // public TearDownSandbox function.
  GetProcessWideSandbox()->TearDown();
#endif

  platform_ = nullptr;
  AdvanceStartupState(V8StartupState::kPlatformDisposed);
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
