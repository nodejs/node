// Copyright 2008-2009 the V8 project authors. All rights reserved.
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

// A light-weight assembler for the Irregexp byte code.


#include "v8.h"
#include "ast.h"
#include "bytecodes-irregexp.h"


namespace v8 { namespace internal {


void RegExpMacroAssemblerIrregexp::Emit(uint32_t byte,
                                        uint32_t twenty_four_bits) {
  uint32_t word = ((twenty_four_bits << BYTECODE_SHIFT) | byte);
  ASSERT(pc_ <= buffer_.length());
  if (pc_  + 3 >= buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<uint32_t*>(buffer_.start() + pc_) = word;
  pc_ += 4;
}


void RegExpMacroAssemblerIrregexp::Emit16(uint32_t word) {
  ASSERT(pc_ <= buffer_.length());
  if (pc_ + 1 >= buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<uint16_t*>(buffer_.start() + pc_) = word;
  pc_ += 2;
}


void RegExpMacroAssemblerIrregexp::Emit32(uint32_t word) {
  ASSERT(pc_ <= buffer_.length());
  if (pc_ + 3 >= buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<uint32_t*>(buffer_.start() + pc_) = word;
  pc_ += 4;
}


} }  // namespace v8::internal
