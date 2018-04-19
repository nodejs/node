// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/instruction-stream.h"

#include "src/builtins/builtins.h"
#include "src/heap/heap.h"
#include "src/objects-inl.h"
#include "src/objects/code-inl.h"

namespace v8 {
namespace internal {

InstructionStream::InstructionStream(Code* code)
    : builtin_index_(code->builtin_index()) {
  DCHECK(Builtins::IsOffHeapBuiltin(code));
  const size_t page_size = AllocatePageSize();
  byte_length_ =
      RoundUp(static_cast<size_t>(code->instruction_size()), page_size);

  bytes_ = static_cast<uint8_t*>(AllocatePages(
      GetRandomMmapAddr(), byte_length_, page_size, PageAllocator::kReadWrite));
  CHECK_NOT_NULL(bytes_);

  std::memcpy(bytes_, code->instruction_start(), code->instruction_size());
  CHECK(SetPermissions(bytes_, byte_length_, PageAllocator::kReadExecute));
}

InstructionStream::~InstructionStream() {
  CHECK(FreePages(bytes_, byte_length_));
}

// static
Code* InstructionStream::TryLookupCode(Isolate* isolate, Address address) {
  DCHECK(FLAG_stress_off_heap_code);
  // TODO(jgruber,v8:6666): Replace with binary search through range checks
  // once off-heap code is mapped into a contiguous memory space.
  for (const InstructionStream* stream : isolate->off_heap_code_) {
    if (stream->Contains(address)) {
      return isolate->builtins()->builtin(stream->builtin_index());
    }
  }
  return nullptr;
}

// static
InstructionStream* InstructionStream::TryLookupInstructionStream(
    Isolate* isolate, Code* code) {
  DCHECK(FLAG_stress_off_heap_code);
  // TODO(jgruber,v8:6666): Replace with binary search through range checks
  // once off-heap code is mapped into a contiguous memory space.
  const int builtin_index = code->builtin_index();
  DCHECK(Builtins::IsBuiltinId(builtin_index));
  for (InstructionStream* stream : isolate->off_heap_code_) {
    if (stream->builtin_index() == builtin_index) return stream;
  }
  return nullptr;
}

bool InstructionStream::Contains(Address address) const {
  return bytes_ <= address && address < bytes_ + byte_length_;
}

}  // namespace internal
}  // namespace v8
