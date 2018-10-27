// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_CALL_TESTER_H_
#define V8_CCTEST_COMPILER_CALL_TESTER_H_

#include "src/handles.h"
#include "src/objects/code.h"
#include "src/simulator.h"
#include "test/cctest/compiler/c-signature.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename R>
class CallHelper {
 public:
  explicit CallHelper(Isolate* isolate, MachineSignature* csig)
      : csig_(csig), isolate_(isolate) {
    USE(isolate_);
  }
  virtual ~CallHelper() = default;

  template <typename... Params>
  R Call(Params... args) {
    CSignature::VerifyParams<Params...>(csig_);
    Address entry = Generate();
    auto fn = GeneratedCode<R, Params...>::FromAddress(isolate_, entry);
    return fn.Call(args...);
  }

 protected:
  MachineSignature* csig_;

  virtual Address Generate() = 0;

 private:
  Isolate* isolate_;
};

// A call helper that calls the given code object assuming C calling convention.
template <typename T>
class CodeRunner : public CallHelper<T> {
 public:
  CodeRunner(Isolate* isolate, Handle<Code> code, MachineSignature* csig)
      : CallHelper<T>(isolate, csig), code_(code) {}
  ~CodeRunner() override = default;

  Address Generate() override { return code_->entry(); }

 private:
  Handle<Code> code_;
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_CALL_TESTER_H_
