// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SANDBOXABLE_THREAD_H_
#define V8_SANDBOX_SANDBOXABLE_THREAD_H_

#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {

//
// A thread that can be used with the V8 Sandbox's hardware support.
//
// This class ensures that the thread is properly set up for hardware
// sandboxing. In general, any thread that will run JavaScript and/or
// WebAssembly code (which run sandboxed) must be sandboxable.
//
class V8_EXPORT_PRIVATE SandboxableThread : public v8::base::Thread {
 public:
  explicit SandboxableThread(const Options& options)
      : v8::base::Thread(options) {}

 protected:
  // Perform per-thread hardware sandbox initialization before calling Run().
  void Dispatch() override;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_SANDBOXABLE_THREAD_H_
