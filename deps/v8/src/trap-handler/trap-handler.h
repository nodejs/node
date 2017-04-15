// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_H_
#define V8_TRAP_HANDLER_H_

namespace v8 {
namespace internal {
namespace trap_handler {

struct ProtectedInstructionData {
  // The offset of this instruction from the start of its code object.
  int32_t instr_offset;

  // The offset of the landing pad from the start of its code object.
  //
  // TODO(eholk): Using a single landing pad and store parameters here.
  int32_t landing_offset;
};

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_H_
