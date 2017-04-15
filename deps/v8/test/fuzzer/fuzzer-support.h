// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_FUZZER_FUZZER_SUPPORT_H_
#define TEST_FUZZER_FUZZER_SUPPORT_H_

#include "include/v8.h"

namespace v8_fuzzer {

class FuzzerSupport {
 public:
  FuzzerSupport(int* argc, char*** argv);
  ~FuzzerSupport();

  static FuzzerSupport* Get();

  v8::Isolate* GetIsolate();
  v8::Local<v8::Context> GetContext();

 private:
  // Prevent copying. Not implemented.
  FuzzerSupport(const FuzzerSupport&);
  FuzzerSupport& operator=(const FuzzerSupport&);


  v8::Platform* platform_;
  v8::ArrayBuffer::Allocator* allocator_;
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
};

}  // namespace

#endif  //  TEST_FUZZER_FUZZER_SUPPORT_H_
