// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DISASSEMBLER_H_
#define V8_DISASSEMBLER_H_

#include "src/allocation.h"
#include "src/code-reference.h"

namespace v8 {
namespace internal {

class Disassembler : public AllStatic {
 public:
  // Decode instructions in the the interval [begin, end) and print the
  // code into os. Returns the number of bytes disassembled or 1 if no
  // instruction could be decoded.
  // the code object is used for name resolution and may be null.
  // TODO(titzer): accept a {WasmCodeManager*} if {isolate} is null
  static int Decode(Isolate* isolate, std::ostream* os, byte* begin, byte* end,
                    CodeReference code = {}, Address current_pc = kNullAddress);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DISASSEMBLER_H_
