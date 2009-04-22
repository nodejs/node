// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_DISASSEMBLER_H_
#define V8_DISASSEMBLER_H_

namespace v8 { namespace internal {

class Disassembler : public AllStatic {
 public:
  // Print the bytes in the interval [begin, end) into f.
  static void Dump(FILE* f, byte* begin, byte* end);

  // Decode instructions in the the interval [begin, end) and print the
  // code into f. Returns the number of bytes disassembled or 1 if no
  // instruction could be decoded.
  static int Decode(FILE* f, byte* begin, byte* end);

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
