// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_LABEL_H_
#define V8_INTERPRETER_BYTECODE_LABEL_H_

#include <algorithm>

#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilder;

// A label representing a loop header in a bytecode array. It is bound before
// the jump is seen, so its position is always known by the time the jump is
// reached.
class V8_EXPORT_PRIVATE BytecodeLoopHeader final {
 public:
  BytecodeLoopHeader() : offset_(kInvalidOffset) {}

  size_t offset() const {
    DCHECK_NE(offset_, kInvalidOffset);
    return offset_;
  }

 private:
  static const size_t kInvalidOffset = static_cast<size_t>(-1);

  void bind_to(size_t offset) {
    DCHECK_NE(offset, kInvalidOffset);
    DCHECK_EQ(offset_, kInvalidOffset);
    offset_ = offset;
  }

  // The bytecode offset of the loop header.
  size_t offset_;

  friend class BytecodeArrayWriter;
};

// A label representing a forward branch target in a bytecode array. When a
// label is bound, it represents a known position in the bytecode array. A label
// can only have at most one referrer jump.
class V8_EXPORT_PRIVATE BytecodeLabel final {
 public:
  BytecodeLabel() : bound_(false), jump_offset_(kInvalidOffset) {}

  bool is_bound() const { return bound_; }
  size_t jump_offset() const {
    DCHECK_NE(jump_offset_, kInvalidOffset);
    return jump_offset_;
  }

  bool has_referrer_jump() const { return jump_offset_ != kInvalidOffset; }

 private:
  static const size_t kInvalidOffset = static_cast<size_t>(-1);

  void bind() {
    DCHECK(!bound_);
    bound_ = true;
  }

  void set_referrer(size_t offset) {
    DCHECK(!bound_);
    DCHECK_NE(offset, kInvalidOffset);
    DCHECK_EQ(jump_offset_, kInvalidOffset);
    jump_offset_ = offset;
  }

  // Set when the label is bound (i.e. the start of the target basic block).
  bool bound_;
  // Set when the jump referrer is set (i.e. the location of the jump).
  size_t jump_offset_;

  friend class BytecodeArrayWriter;
};

// Class representing a branch target of multiple jumps.
class V8_EXPORT_PRIVATE BytecodeLabels {
 public:
  explicit BytecodeLabels(Zone* zone) : labels_(zone), is_bound_(false) {}

  BytecodeLabel* New();

  void Bind(BytecodeArrayBuilder* builder);

  bool is_bound() const {
    DCHECK_IMPLIES(
        is_bound_,
        std::all_of(labels_.begin(), labels_.end(), [](const BytecodeLabel& l) {
          return !l.has_referrer_jump() || l.is_bound();
        }));
    return is_bound_;
  }

  bool empty() const { return labels_.empty(); }

 private:
  ZoneLinkedList<BytecodeLabel> labels_;
  bool is_bound_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeLabels);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_LABEL_H_
