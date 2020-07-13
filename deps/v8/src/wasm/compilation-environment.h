// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_COMPILATION_ENVIRONMENT_H_
#define V8_WASM_COMPILATION_ENVIRONMENT_H_

#include <memory>

#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {

class Counters;

namespace wasm {

class NativeModule;
class WasmCode;
class WasmError;

enum RuntimeExceptionSupport : bool {
  kRuntimeExceptionSupport = true,
  kNoRuntimeExceptionSupport = false
};

enum UseTrapHandler : bool { kUseTrapHandler = true, kNoTrapHandler = false };

enum LowerSimd : bool { kLowerSimd = true, kNoLowerSimd = false };

// The {CompilationEnv} encapsulates the module data that is used during
// compilation. CompilationEnvs are shareable across multiple compilations.
struct CompilationEnv {
  // A pointer to the decoded module's static representation.
  const WasmModule* const module;

  // True if trap handling should be used in compiled code, rather than
  // compiling in bounds checks for each memory access.
  const UseTrapHandler use_trap_handler;

  // If the runtime doesn't support exception propagation,
  // we won't generate stack checks, and trap handling will also
  // be generated differently.
  const RuntimeExceptionSupport runtime_exception_support;

  // The smallest size of any memory that could be used with this module, in
  // bytes.
  const uint64_t min_memory_size;

  // The largest size of any memory that could be used with this module, in
  // bytes.
  const uint64_t max_memory_size;

  // Features enabled for this compilation.
  const WasmFeatures enabled_features;

  const LowerSimd lower_simd;

  constexpr CompilationEnv(const WasmModule* module,
                           UseTrapHandler use_trap_handler,
                           RuntimeExceptionSupport runtime_exception_support,
                           const WasmFeatures& enabled_features,
                           LowerSimd lower_simd = kNoLowerSimd)
      : module(module),
        use_trap_handler(use_trap_handler),
        runtime_exception_support(runtime_exception_support),
        min_memory_size(module ? module->initial_pages * uint64_t{kWasmPageSize}
                               : 0),
        max_memory_size((module && module->has_maximum_pages
                             ? module->maximum_pages
                             : max_initial_mem_pages()) *
                        uint64_t{kWasmPageSize}),
        enabled_features(enabled_features),
        lower_simd(lower_simd) {}
};

// The wire bytes are either owned by the StreamingDecoder, or (after streaming)
// by the NativeModule. This class abstracts over the storage location.
class WireBytesStorage {
 public:
  virtual ~WireBytesStorage() = default;
  virtual Vector<const uint8_t> GetCode(WireBytesRef) const = 0;
};

// Callbacks will receive either {kFailedCompilation} or both
// {kFinishedBaselineCompilation} and {kFinishedTopTierCompilation}, in that
// order. If tier up is off, both events are delivered right after each other.
enum class CompilationEvent : uint8_t {
  kFinishedBaselineCompilation,
  kFinishedTopTierCompilation,
  kFailedCompilation,
  kFinishedRecompilation
};

// The implementation of {CompilationState} lives in module-compiler.cc.
// This is the PIMPL interface to that private class.
class CompilationState {
 public:
  using callback_t = std::function<void(CompilationEvent)>;

  ~CompilationState();

  void AbortCompilation();

  void SetError();

  void SetWireBytesStorage(std::shared_ptr<WireBytesStorage>);

  V8_EXPORT_PRIVATE std::shared_ptr<WireBytesStorage> GetWireBytesStorage()
      const;

  void AddCallback(callback_t);

  bool failed() const;
  V8_EXPORT_PRIVATE bool baseline_compilation_finished() const;
  V8_EXPORT_PRIVATE bool top_tier_compilation_finished() const;
  V8_EXPORT_PRIVATE bool recompilation_finished() const;

  // Override {operator delete} to avoid implicit instantiation of {operator
  // delete} with {size_t} argument. The {size_t} argument would be incorrect.
  void operator delete(void* ptr) { ::operator delete(ptr); }

  CompilationState() = delete;

 private:
  // NativeModule is allowed to call the static {New} method.
  friend class NativeModule;

  // The CompilationState keeps a {std::weak_ptr} back to the {NativeModule}
  // such that it can keep it alive (by regaining a {std::shared_ptr}) in
  // certain scopes.
  static std::unique_ptr<CompilationState> New(
      const std::shared_ptr<NativeModule>&, std::shared_ptr<Counters>);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_COMPILATION_ENVIRONMENT_H_
