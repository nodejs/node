// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DISASSEMBLER_H_
#define V8_DISASSEMBLER_H_

#include "allocation.h"

namespace v8 {
namespace internal {

class Disassembler : public AllStatic {
 public:
  // Print the bytes in the interval [begin, end) into f.
  static void Dump(FILE* f, byte* begin, byte* end);

  // Decode instructions in the the interval [begin, end) and print the
  // code into f. Returns the number of bytes disassembled or 1 if no
  // instruction could be decoded.
  static int Decode(Isolate* isolate, FILE* f, byte* begin, byte* end);

  // Decode instructions in code.
  static void Decode(FILE* f, Code* code);
 private:
  // Decode instruction at pc and print disassembled instruction into f.
  // Returns the instruction length in bytes, or 1 if the instruction could
  // not be decoded.  The number of characters written is written into
  // the out parameter char_count.
  static int Decode(FILE* f, byte* pc, int* char_count);
};

} }  // namespace v8::internal

#endif  // V8_DISASSEMBLER_H_
