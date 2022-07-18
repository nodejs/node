// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TESTING_H_
#define V8_SANDBOX_TESTING_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX

class SandboxTesting {
 public:
#ifdef V8_EXPOSE_MEMORY_CORRUPTION_API
  // A JavaScript API that emulates typical exploit primitives.
  //
  // This can be used for testing the sandbox, for example to write regression
  // tests for bugs in the sandbox or to develop fuzzers.
  V8_EXPORT_PRIVATE static void InstallMemoryCorruptionApi(Isolate* isolate);
#endif  // V8_EXPOSE_MEMORY_CORRUPTION_API

  // A signal handler that filters out "harmless" (for the sandbox) crashes.
  //
  // If an access violation (i.e. a SIGSEGV or SIGBUS) happens inside the
  // sandbox address space it is considered harmless for the sandbox and so the
  // process is simply terminated through _exit. On the other hand, an access
  // violation outside the sandbox address space indicates a security issue with
  // the sandbox. In that case, the signal is forwarded to the original signal
  // handler which will report the violation appropriately.
  // Currently supported on Linux only.
  V8_EXPORT_PRIVATE static void InstallSandboxCrashFilter();
};

#endif  // V8_ENABLE_SANDBOX

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TESTING_H_
