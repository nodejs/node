// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TESTING_H_
#define V8_SANDBOX_TESTING_H_

#include <unordered_map>

#include "src/base/flags.h"
#include "src/common/globals.h"
#include "src/objects/instance-type.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX

// Infrastructure for testing the security properties of the sandbox.
class SandboxTesting : public AllStatic {
 public:
  // The different sandbox testing modes.
  enum class Mode {
    // Sandbox testing mode is not active.
    kDisabled,

    // Mode for testing the sandbox.
    //
    // This will enable the crash filter and install the memory corruption API
    // (if enabled at compile time). If a harmless crash is detected, the
    // process is terminated with exist status zero. This is useful so that
    // tests pass if they for example fail a CHECK.
    kForTesting,

    // Mode for fuzzing the sandbox.
    //
    // Similar to kForTesting, but if a harmless crash is detected, the process
    // is terminated with a non-zero exit status so fuzzers can determine that
    // execution terminated prematurely.
    kForFuzzing,
  };

  // Enable sandbox testing mode.
  //
  // This will initialize the sandbox crash filter. The crash filter is a
  // signal handler for a number of fatal signals (e.g. SIGSEGV and SIGBUS) that
  // filters out all crashes that are not considered "sandbox violations".
  // Examples of such "safe" crahses (in the context of the sandbox) are memory
  // access violations inside the sandbox address space or access violations
  // that always lead to an immediate crash (for example, an access to a
  // non-canonical address which may be the result of a tag mismatch in one of
  // the sandbox's pointer tables). On the other hand, if the crash represents
  // a legitimate sandbox violation, the signal is forwarded to the original
  // signal handler which will report the crash appropriately.
  //
  // Currently supported on Linux only.
  V8_EXPORT_PRIVATE static void Enable(Mode mode);

  // Disable sandbox testing mode.
  //
  // This will uninstall the sandbox crash filter if it was previously enabled.
  // This is mostly useful for manually reporting sandbox violations, in which
  // case this method must be called prior to terminating the process via (for
  // example) Abort().
  V8_EXPORT_PRIVATE static void Disable();

  // Returns whether sandbox testing mode is enabled.
  static bool IsEnabled() { return mode_ != Mode::kDisabled; }

  // The different types of memory accesses. Used when classifying access
  // violations in the crash filter and for registering "safe" memory regions
  // via RegisterSafeMemoryRegion.
  enum class MemoryAccessType {
    kRead = 1 << 0,
    kWrite = 1 << 1,
    kExecute = 1 << 2,
  };
  using MemoryAccessTypes = base::Flags<MemoryAccessType>;

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
  // A JavaScript API that emulates typical exploit primitives.
  //
  // This can be used for testing the sandbox, for example to write regression
  // tests for bugs in the sandbox or to develop fuzzers.
  V8_EXPORT_PRIVATE static void InstallMemoryCorruptionApi(Isolate* isolate);

  // Convenience constants to use for RegisterSafeMemoryRegion.
  static constexpr MemoryAccessTypes kReadAndWriteAccessIsSafe =
      MemoryAccessTypes(static_cast<int>(MemoryAccessType::kRead) |
                        static_cast<int>(MemoryAccessType::kWrite));
  static constexpr MemoryAccessTypes kOnlyReadAccessIsSafe =
      MemoryAccessType::kRead;

  // Record a memory region that is considered "safe" to crash in.
  //
  // When the sandbox crash filter analyses a crash, it will check if the
  // faulting address is inside any of the known-safe regions, and if so,
  // discard it. Examples of such "safe-to-crash" regions include the various
  // pointer tables in which a (safe) OOB access can be triggered by corrupting
  // in-sandbox table handles.
  //
  // In the future, when/if it is always possible to assign custom names to
  // virtual memory regions, we could change this mechanism to instead rely on
  // well-known memory region names.
  //
  // This API is currently only available when the memory corruption API is
  // enabled to avoid the (likely small) overhead from keeping track of memory
  // regions in production builds. Instead of V8_ENABLE_MEMORY_CORRUPTION_API,
  // we could also consider introducing a special build flag to indicate that
  // this is a "sandbox testing" build and then put this API behind that.
  V8_EXPORT_PRIVATE static void RegisterSafeMemoryRegion(
      Address start, size_t size, MemoryAccessTypes safe_access_types);

  // Unregister a previously registered safe memory region.
  V8_EXPORT_PRIVATE static void UnregisterSafeMemoryRegion(Address start);
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API

  // The current sandbox testing mode.
  static Mode mode() { return mode_; }

  // Returns a mapping of type names to their InstanceType.
  using InstanceTypeMap = std::unordered_map<std::string, InstanceType>;
  static InstanceTypeMap& GetInstanceTypeMap();

  // Returns a mapping of instance types to known field offsets. This is useful
  // mainly for the Sandbox.getFieldOffsetOf API which provides access to
  // internal field offsets of HeapObject to JavaScript.
  using FieldOffsets = std::unordered_map<std::string, int>;
  using FieldOffsetMap = std::unordered_map<InstanceType, FieldOffsets>;
  static FieldOffsetMap& GetFieldOffsetMap();

  // Returns the field offset based on instance type and field name, or throws
  // an error in the isolate and returns an empty optional.
  V8_EXPORT_PRIVATE static std::optional<int> GetFieldOffset(
      v8::Isolate* isolate_for_errors, InstanceType instance_type,
      const std::string& field_name);

 private:
  static Mode mode_;
};

#endif  // V8_ENABLE_SANDBOX

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TESTING_H_
