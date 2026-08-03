// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_DISASSEMBLER_H_
#define V8_DIAGNOSTICS_DISASSEMBLER_H_

#include "src/codegen/code-reference.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class Disassembler : public AllStatic {
 public:
  // Decode instructions in the the interval [begin, end) and print the
  // code into os. Returns the number of bytes disassembled or 1 if no
  // instruction could be decoded.
  // Does not abort on unimplemented opcodes, but prints them as 'Unimplemented
  // Instruction'.
  // the code object is used for name resolution and may be null.
  // TODO(titzer): accept a {WasmCodeManager*} if {isolate} is null
  V8_EXPORT_PRIVATE static int Decode(Isolate* isolate, std::ostream& os,
                                      uint8_t* begin, uint8_t* end,
                                      CodeReference code = {},
                                      Address current_pc = kNullAddress,
                                      size_t range_limit = 0);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DIAGNOSTICS_DISASSEMBLER_H_
