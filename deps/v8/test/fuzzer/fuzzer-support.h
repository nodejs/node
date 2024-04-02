// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_FUZZER_FUZZER_SUPPORT_H_
#define TEST_FUZZER_FUZZER_SUPPORT_H_

#include <memory>

#include "include/libplatform/libplatform.h"
#include "include/v8-array-buffer.h"
#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"

namespace v8 {
class Context;
class Isolate;
}  // namespace v8

namespace v8_fuzzer {

class FuzzerSupport {
 public:
  FuzzerSupport(int* argc, char*** argv);
  FuzzerSupport(const FuzzerSupport&) = delete;
  FuzzerSupport& operator=(const FuzzerSupport&) = delete;

  ~FuzzerSupport();

  static void InitializeFuzzerSupport(int* argc, char*** argv);

  static FuzzerSupport* Get();

  v8::Isolate* GetIsolate() const { return isolate_; }

  v8::Local<v8::Context> GetContext();

  bool PumpMessageLoop(v8::platform::MessageLoopBehavior =
                           v8::platform::MessageLoopBehavior::kDoNotWait);

 private:
  static std::unique_ptr<FuzzerSupport> fuzzer_support_;
  std::unique_ptr<v8::Platform> platform_;
  v8::ArrayBuffer::Allocator* allocator_;
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
};

}  // namespace v8_fuzzer

#endif  //  TEST_FUZZER_FUZZER_SUPPORT_H_
