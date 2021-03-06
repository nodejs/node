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

  static bool Initialize();
  static void TearDown();

  // Report process out of memory. Implementation found in api.cc.
  // This function will not return, but will terminate the execution.
  [[noreturn]] static void FatalProcessOutOfMemory(Isolate* isolate,
                                                   const char* location,
                                                   bool is_heap_oom = false);

  static void InitializePlatform(v8::Platform* platform);
  static void ShutdownPlatform();
  V8_EXPORT_PRIVATE static v8::Platform* GetCurrentPlatform();
  // Replaces the current platform with the given platform.
  // Should be used only for testing.
  V8_EXPORT_PRIVATE static void SetPlatformForTesting(v8::Platform* platform);

  static void SetSnapshotBlob(StartupData* snapshot_blob);

 private:
  static void InitializeOncePerProcessImpl();
  static void InitializeOncePerProcess();

  // v8::Platform to use.
  static v8::Platform* platform_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_V8_H_
