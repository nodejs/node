// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DECOMPRESSION_ELIMINATION_H_
#define V8_COMPILER_DECOMPRESSION_ELIMINATION_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Performs elimination of redundant decompressions within the graph.
class V8_EXPORT_PRIVATE DecompressionElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  explicit DecompressionElimination(Editor* editor, Graph* graph,
                                    MachineOperatorBuilder* machine,
                                    CommonOperatorBuilder* common);
  ~DecompressionElimination() final = default;

  const char* reducer_name() const override {
    return "DecompressionElimination";
  }

  Reduction Reduce(Node* node) final;

 private:
  // Returns true if the decompress opcode is valid for the compressed one.
  bool IsValidDecompress(IrOpcode::Value compressOpcode,
                         IrOpcode::Value decompressOpcode);

  // Returns true if the constant opcode is a reducible one in decompression
  // elimination.
  bool IsReducibleConstantOpcode(IrOpcode::Value opcode);

  // Get the new 32 bit node constant given the 64 bit one.
  Node* GetCompressedConstant(Node* constant);

  // Removes direct Decompressions & Compressions, going from
  //     Parent <- Decompression <- Compression <- Child
  // to
  //     Parent <- Child
  // Can be used for Any, Signed, and Pointer compressions.
  Reduction ReduceCompress(Node* node);

  // Removes direct Compressions & Decompressions, analogously to ReduceCompress
  Reduction ReduceDecompress(Node* node);

  // Replaces Phi's input decompressions with their input node, if and only if
  // all of the Phi's inputs are Decompress nodes.
  Reduction ReducePhi(Node* node);

  // Replaces TypedStateValues's input decompressions with their input node.
  Reduction ReduceTypedStateValues(Node* node);

  // Replaces a Word64Equal with a Word32Equal if both of its inputs are
  // Decompress nodes, or if one is a Decompress node and the other a constant.
  // In the case of two decompresses, it uses the original inputs before they
  // are decompressed. In the case of having a constant, it uses the compressed
  // value of that constant.
  Reduction ReduceWord64Equal(Node* node);

  // This is a workaround for load elimination test.
  // Replaces Compress -> BitcastWordToTaggedSigned -> ReducibleConstant
  // to CompressedConstant on both inputs of Word32Equal operation.
  Reduction ReduceWord32Equal(Node* node);

  Graph* graph() const { return graph_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  CommonOperatorBuilder* common() const { return common_; }

  Graph* const graph_;
  MachineOperatorBuilder* const machine_;
  CommonOperatorBuilder* const common_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DECOMPRESSION_ELIMINATION_H_
