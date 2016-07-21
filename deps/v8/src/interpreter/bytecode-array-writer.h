// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_WRITER_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_WRITER_H_

#include "src/interpreter/bytecode-pipeline.h"

namespace v8 {
namespace internal {
namespace interpreter {

class SourcePositionTableBuilder;

// Class for emitting bytecode as the final stage of the bytecode
// generation pipeline.
class BytecodeArrayWriter final : public BytecodePipelineStage {
 public:
  BytecodeArrayWriter(
      Zone* zone, SourcePositionTableBuilder* source_position_table_builder);
  virtual ~BytecodeArrayWriter();

  void Write(BytecodeNode* node) override;
  size_t FlushForOffset() override;
  void FlushBasicBlock() override;

  // Get the bytecode vector.
  ZoneVector<uint8_t>* bytecodes() { return &bytecodes_; }

  // Returns the size in bytes of the frame associated with the
  // bytecode written.
  int GetMaximumFrameSizeUsed();

 private:
  void EmitBytecode(const BytecodeNode* const node);
  void UpdateSourcePositionTable(const BytecodeNode* const node);

  ZoneVector<uint8_t> bytecodes_;
  int max_register_count_;
  SourcePositionTableBuilder* source_position_table_builder_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayWriter);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_WRITER_H_
