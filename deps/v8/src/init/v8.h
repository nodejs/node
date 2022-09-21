// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_V8_H_
#define V8_INIT_V8_H_

#include "src/common/globals.h"

namespace v8 {

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

  // Another variant of FatalProcessOutOfMemory, which constructs the OOMDetails
  // struct internally from another "detail" c-string.
  // This can be removed once we support designated initializers (C++20).
  [[noreturn]] V8_EXPORT_PRIVATE static void FatalProcessOutOfMemory(
      Isolate* isolate, const char* location, const char* detail);

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
