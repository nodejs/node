// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_V8_H_
#define V8_INIT_V8_H_

#include <string_view>

#include "src/common/globals.h"

namespace v8 {

using CrashKey = void*;
enum class CrashKeySize;
using AllocateCrashKeyCallback = CrashKey (*)(const char key[],
                                              CrashKeySize size);
using SetCrashKeyCallback = void (*)(CrashKey key, std::string_view value);

struct OOMDetails;
class Platform;
class StartupData;

namespace internal {

class Isolate;

class V8 : public AllStatic {
 public:
  // Global actions.
  static void Initialize();
  static void Dispose();

  // Report process out of memory. Implementation found in api.cc.
  // This function will not return, but will terminate the execution.
  // IMPORTANT: Update the Google-internal crash processer if this signature
  // changes to be able to extract detailed v8::internal::HeapStats on OOM.
  [[noreturn]] V8_EXPORT_PRIVATE static void FatalProcessOutOfMemory(
      Isolate* isolate, const char* location,
      const OOMDetails& details = kNoOOMDetails);

  // Constants to be used for V8::FatalProcessOutOfMemory. They avoid having
  // to include v8-callbacks.h in all callers.
  V8_EXPORT_PRIVATE static const OOMDetails kNoOOMDetails;
  V8_EXPORT_PRIVATE static const OOMDetails kHeapOOM;

  static void InitializePlatform(v8::Platform* platform);
  V8_EXPORT_PRIVATE static void InitializePlatformForTesting(
      v8::Platform* platform);
  static void DisposePlatform();
  V8_EXPORT_PRIVATE static v8::Platform* GetCurrentPlatform();
  // Replaces the current platform with the given platform.
  // Should be used only for testing.
  V8_EXPORT_PRIVATE static void SetPlatformForTesting(v8::Platform* platform);

  static void SetSnapshotBlob(StartupData* snapshot_blob);

  static void SetCrashKeyCallbacks(AllocateCrashKeyCallback allocate_callback,
                                   SetCrashKeyCallback set_callback);
  V8_EXPORT_PRIVATE static bool HasCrashKeyCallbacks();
  // Allocates a new crash key on every call (even for the same `key` name),
  // which is never freed. If a crash key can be set more than once, cache the
  // returned `CrashKey` in a `static` variable and update it via `SetCrashKey`
  // (synchronizing concurrent updates to the same `CrashKey`).
  V8_EXPORT_PRIVATE static CrashKey AllocateCrashKey(const char key[],
                                                     CrashKeySize size);
  // Convenience helper that allocates and sets a crash key in one step. Only
  // use this on fatal crash paths that execute at most once per process;
  // otherwise repeated calls will leak a new crash key on every invocation.
  V8_EXPORT_PRIVATE static CrashKey AddCrashKey(const char key[],
                                                CrashKeySize size,
                                                std::string_view value);
  V8_EXPORT_PRIVATE static void SetCrashKey(CrashKey crash_key,
                                            std::string_view value);

 private:
  static v8::Platform* platform_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_V8_H_
