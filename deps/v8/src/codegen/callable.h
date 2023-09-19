// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_CALLABLE_H_
#define V8_CODEGEN_CALLABLE_H_

#include "src/codegen/interface-descriptors.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class InstructionStream;

// Associates a body of code with an interface descriptor.
class Callable final {
 public:
  Callable(Handle<Code> code, CallInterfaceDescriptor descriptor)
      : code_(code), descriptor_(descriptor) {}

  Handle<Code> code() const { return code_; }
  CallInterfaceDescriptor descriptor() const { return descriptor_; }

 private:
  const Handle<Code> code_;
  const CallInterfaceDescriptor descriptor_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_CALLABLE_H_
