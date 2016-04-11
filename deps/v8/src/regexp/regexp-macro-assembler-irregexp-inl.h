// Copyright 2008-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_MACRO_ASSEMBLER_IRREGEXP_INL_H_
#define V8_REGEXP_REGEXP_MACRO_ASSEMBLER_IRREGEXP_INL_H_

#include "src/ast/ast.h"
#include "src/regexp/bytecodes-irregexp.h"

namespace v8 {
namespace internal {

#ifdef V8_INTERPRETED_REGEXP

void RegExpMacroAssemblerIrregexp::Emit(uint32_t byte,
                                        uint32_t twenty_four_bits) {
  uint32_t word = ((twenty_four_bits << BYTECODE_SHIFT) | byte);
  DCHECK(pc_ <= buffer_.length());
  if (pc_  + 3 >= buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<uint32_t*>(buffer_.start() + pc_) = word;
  pc_ += 4;
}


void RegExpMacroAssemblerIrregexp::Emit16(uint32_t word) {
  DCHECK(pc_ <= buffer_.length());
  if (pc_ + 1 >= buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<uint16_t*>(buffer_.start() + pc_) = word;
  pc_ += 2;
}


void RegExpMacroAssemblerIrregexp::Emit8(uint32_t word) {
  DCHECK(pc_ <= buffer_.length());
  if (pc_ == buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<unsigned char*>(buffer_.start() + pc_) = word;
  pc_ += 1;
}


void RegExpMacroAssemblerIrregexp::Emit32(uint32_t word) {
  DCHECK(pc_ <= buffer_.length());
  if (pc_ + 3 >= buffer_.length()) {
    Expand();
  }
  *reinterpret_cast<uint32_t*>(buffer_.start() + pc_) = word;
  pc_ += 4;
}

#endif  // V8_INTERPRETED_REGEXP

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_MACRO_ASSEMBLER_IRREGEXP_INL_H_
