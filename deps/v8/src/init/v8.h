// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_V8_H_
#define V8_INIT_V8_H_

#include "src/common/globals.h"

namespace v8 {

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
  [[noreturn]] static void FatalProcessOutOfMemory(Isolate* isolate,
                                                   const char* location,
                                                   bool is_heap_oom = false);

#ifdef V8_SANDBOX
  static bool InitializeSandbox();
#endif

  static void InitializePlatform(v8::Platform* platform);
  static void DisposePlatform();
  V8_EXPORT_PRIVATE static v8::Platform* GetCurrentPlatform();
  // Replaces the current platform with the given platform.
  // Should be used only for testing.
  V8_EXPORT_PRIVATE static void SetPlatformForTesting(v8::Platform* platform);

  static void SetSnapshotBlob(StartupData* snapshot_blob);

 private:
  static v8::Platform* platform_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_V8_H_
