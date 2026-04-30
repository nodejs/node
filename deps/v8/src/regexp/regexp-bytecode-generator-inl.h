// Copyright 2008-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODE_GENERATOR_INL_H_
#define V8_REGEXP_REGEXP_BYTECODE_GENERATOR_INL_H_

#include "src/regexp/regexp-bytecode-generator.h"
// Include the non-inl header before the rest of the headers.

#include "src/regexp/regexp-bytecodes-inl.h"

namespace v8 {
namespace internal {

template <typename T>
void RegExpBytecodeWriter::Emit(T value, int offset) {
  const int new_pc_within_bc = pc_ + offset;
  DCHECK(base::IsInRange(new_pc_within_bc, pc_within_bc_, end_of_bc_));
  DCHECK_LE(new_pc_within_bc + sizeof(T), buffer_.size());
  DCHECK_LE(new_pc_within_bc + sizeof(T), end_of_bc_);
  DCHECK(IsAligned(new_pc_within_bc, sizeof(T)));
  EMIT_PADDING(offset);
  *reinterpret_cast<T*>(buffer_.data() + new_pc_within_bc) = value;
#ifdef DEBUG
  pc_within_bc_ = new_pc_within_bc + sizeof(T);
#endif
}

template <typename T>
void RegExpBytecodeWriter::OverwriteValue(T value, int absolute_offset) {
  // TODO(jgruber): Consider specializing this function; there should be very
  // few uses (updating jump offsets).
  DCHECK(IsAligned(absolute_offset, sizeof(T)));
  DCHECK_LE(absolute_offset + sizeof(T), buffer_.size());
  *reinterpret_cast<T*>(buffer_.data() + absolute_offset) = value;
}

void RegExpBytecodeWriter::EmitBytecode(RegExpBytecode bc) {
  DCHECK_EQ(pc_, end_of_bc_);
  DCHECK_EQ(pc_within_bc_, end_of_bc_);
#ifdef DEBUG
  end_of_bc_ = pc_ + RegExpBytecodes::Size(bc);
  pc_within_bc_ = pc_;
#endif
  EnsureCapacity(RegExpBytecodes::Size(bc));
  Emit(RegExpBytecodes::ToByte(bc), 0);
}

void RegExpBytecodeWriter::EnsureCapacity(size_t size_delta) {
  const size_t required_size = pc_ + size_delta;
  size_t size = buffer_.size();
  if (V8_LIKELY(size >= required_size)) return;
  if (required_size < kInitialBufferSizeInBytes) {
    size = kInitialBufferSizeInBytes;
  } else if (required_size <= kMaxBufferGrowthInBytes) {
    // We use a doubling strategy until hitting the limit.
    size = base::bits::RoundUpToPowerOfTwo(required_size);
  } else {
    // .. and kMaxBufferGrowthInBytes chunks afterwards.
    size = RoundUp<kMaxBufferGrowthInBytes>(required_size);
  }
  ExpandBuffer(size);
  DCHECK_LE(required_size, buffer_.size());
}

void RegExpBytecodeWriter::ResetPc(int new_pc) {
  // Resetting is only allowed at the beginning of a bytecode.
  DCHECK_EQ(pc_, pc_within_bc_);
  DCHECK_LE(new_pc, pc_);
  pc_ = new_pc;
#ifdef DEBUG
  pc_within_bc_ = pc_;
  end_of_bc_ = pc_;
#endif
}

#ifdef DEBUG
void RegExpBytecodeWriter::EmitPadding(int offset) {
  const int padding_to = pc_ + offset;
  DCHECK_LE(padding_to, buffer_.size());
  DCHECK_LE(padding_to, end_of_bc_);
  DCHECK_GE(padding_to, pc_within_bc_);
  static constexpr uint8_t kPaddingByte = 0x0;
  for (int i = pc_within_bc_; i < padding_to; ++i) {
    buffer_[i] = kPaddingByte;
  }
  pc_within_bc_ = padding_to;
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_GENERATOR_INL_H_
