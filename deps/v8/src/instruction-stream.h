// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSTRUCTION_STREAM_H_
#define V8_INSTRUCTION_STREAM_H_

#include "src/base/macros.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Code;
class Isolate;

// Wraps an mmap'ed off-heap instruction stream. This class will likely become
// unneeded once --stress-off-heap-code is removed.
class InstructionStream final {
 public:
  explicit InstructionStream(Code* code);
  ~InstructionStream();

  // Returns the corresponding Code object if it exists, and nullptr otherwise.
  static Code* TryLookupCode(Isolate* isolate, Address address);

  // Returns the corresponding stream if it exists, and nullptr otherwise.
  static InstructionStream* TryLookupInstructionStream(Isolate* isolate,
                                                       Code* code);

  bool Contains(Address address) const;

  int builtin_index() const { return builtin_index_; }
  size_t byte_length() const { return byte_length_; }
  uint8_t* bytes() const { return bytes_; }

 private:
  size_t byte_length_;
  uint8_t* bytes_;
  int builtin_index_;

  DISALLOW_COPY_AND_ASSIGN(InstructionStream)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INSTRUCTION_STREAM_H_
