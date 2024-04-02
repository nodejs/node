// Copyright 2008-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODE_GENERATOR_INL_H_
#define V8_REGEXP_REGEXP_BYTECODE_GENERATOR_INL_H_

#include "src/regexp/regexp-bytecode-generator.h"

#include "src/regexp/regexp-bytecodes.h"

namespace v8 {
namespace internal {

void RegExpBytecodeGenerator::Emit(uint32_t byte, uint32_t twenty_four_bits) {
  DCHECK(is_uint24(twenty_four_bits));
  Emit32((twenty_four_bits << BYTECODE_SHIFT) | byte);
}

void RegExpBytecodeGenerator::Emit(uint32_t byte, int32_t twenty_four_bits) {
  DCHECK(is_int24(twenty_four_bits));
  Emit32((static_cast<uint32_t>(twenty_four_bits) << BYTECODE_SHIFT) | byte);
}

void RegExpBytecodeGenerator::Emit16(uint32_t word) {
  DCHECK(pc_ <= static_cast<int>(buffer_.size()));
  if (pc_ + 1 >= static_cast<int>(buffer_.size())) {
    ExpandBuffer();
  }
  *reinterpret_cast<uint16_t*>(buffer_.data() + pc_) = word;
  pc_ += 2;
}

void RegExpBytecodeGenerator::Emit8(uint32_t word) {
  DCHECK(pc_ <= static_cast<int>(buffer_.size()));
  if (pc_ == static_cast<int>(buffer_.size())) {
    ExpandBuffer();
  }
  *reinterpret_cast<unsigned char*>(buffer_.data() + pc_) = word;
  pc_ += 1;
}

void RegExpBytecodeGenerator::Emit32(uint32_t word) {
  DCHECK(pc_ <= static_cast<int>(buffer_.size()));
  if (pc_ + 3 >= static_cast<int>(buffer_.size())) {
    ExpandBuffer();
  }
  *reinterpret_cast<uint32_t*>(buffer_.data() + pc_) = word;
  pc_ += 4;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_GENERATOR_INL_H_
