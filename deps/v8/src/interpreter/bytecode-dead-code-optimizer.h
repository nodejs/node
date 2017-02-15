// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_DEAD_CODE_OPTIMIZER_H_
#define V8_INTERPRETER_BYTECODE_DEAD_CODE_OPTIMIZER_H_

#include "src/interpreter/bytecode-pipeline.h"

namespace v8 {
namespace internal {
namespace interpreter {

// An optimization stage for eliminating obviously dead code in bytecode
// generation.
class BytecodeDeadCodeOptimizer final : public BytecodePipelineStage,
                                        public ZoneObject {
 public:
  explicit BytecodeDeadCodeOptimizer(BytecodePipelineStage* next_stage);

  // BytecodePipelineStage interface.
  void Write(BytecodeNode* node) override;
  void WriteJump(BytecodeNode* node, BytecodeLabel* label) override;
  void BindLabel(BytecodeLabel* label) override;
  void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) override;
  Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int register_count, int parameter_count,
      Handle<FixedArray> handler_table) override;

 private:
  BytecodePipelineStage* next_stage_;
  bool exit_seen_in_block_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeDeadCodeOptimizer);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_DEAD_CODE_OPTIMIZER_H_
