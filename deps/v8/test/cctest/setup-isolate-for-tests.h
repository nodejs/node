// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

namespace v8 {
namespace internal {

class SetupIsolateDelegateForTests : public SetupIsolateDelegate {
 public:
  SetupIsolateDelegateForTests() : SetupIsolateDelegate() {}
  virtual ~SetupIsolateDelegateForTests() {}

  void SetupBuiltins(Isolate* isolate, bool create_heap_objects) override;

  void SetupInterpreter(interpreter::Interpreter* interpreter,
                        bool create_heap_objects) override;
};

}  // namespace internal
}  // namespace v8
