// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REPRESENTATION_CHANGE_H_
#define V8_COMPILER_REPRESENTATION_CHANGE_H_

#include "src/compiler/feedback-source.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/use-info.h"

namespace v8 {
namespace internal {
namespace compiler {

// Foward declarations.
class SimplifiedLoweringVerifier;
class TypeCache;

// Contains logic related to changing the representation of values for constants
// and other nodes, as well as lowering Simplified->Machine operators.
// Eagerly folds any representation changes for constants.
class V8_EXPORT_PRIVATE RepresentationChanger final {
 public:
  RepresentationChanger(JSGraph* jsgraph, JSHeapBroker* broker,
                        SimplifiedLoweringVerifier* verifier);

  // Changes representation from {output_type} to {use_rep}. The {truncation}
  // parameter is only used for checking - if the changer cannot figure
  // out signedness for the word32->float64 conversion, then we check that the
  // uses truncate to word32 (so they do not care about signedness).
  Node* GetRepresentationFor(Node* node, MachineRepresentation output_rep,
                             Type output_type, Node* use_node,
                             UseInfo use_info);
  const Operator* Int32OperatorFor(IrOpcode::Value opcode);
  const Operator* Int32OverflowOperatorFor(IrOpcode::Value opcode);
  const Operator* Int64OperatorFor(IrOpcode::Value opcode);
  const Operator* Int64OverflowOperatorFor(IrOpcode::Value opcode);
  const Operator* BigIntOperatorFor(IrOpcode::Value opcode);
  const Operator* TaggedSignedOperatorFor(IrOpcode::Value opcode);
  const Operator* Uint32OperatorFor(IrOpcode::Value opcode);
  const Operator* Uint32OverflowOperatorFor(IrOpcode::Value opcode);
  const Operator* Float64OperatorFor(IrOpcode::Value opcode);

  MachineType TypeForBasePointer(const FieldAccess& access) {
    return access.tag() != 0 ? MachineType::AnyTagged()
                             : MachineType::Pointer();
  }

  MachineType TypeForBasePointer(const ElementAccess& access) {
    return access.tag() != 0 ? MachineType::AnyTagged()
                             : MachineType::Pointer();
  }

  bool verification_enabled() const { return verifier_ != nullptr; }

 private:
  TypeCache const* cache_;
  JSGraph* jsgraph_;
  JSHeapBroker* broker_;
  SimplifiedLoweringVerifier* verifier_;

  friend class RepresentationChangerTester;  // accesses the below fields.

  bool testing_type_errors_;  // If {true}, don't abort on a type error.
  bool type_error_;           // Set when a type error is detected.

  Node* GetTaggedSignedRepresentationFor(Node* node,
                                         MachineRepresentation output_rep,
                                         Type output_type, Node* use_node,
                                         UseInfo use_info);
  Node* GetTaggedPointerRepresentationFor(Node* node,
                                          MachineRepresentation output_rep,
                                          Type output_type, Node* use_node,
                                          UseInfo use_info);
  Node* GetTaggedRepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type output_type, Truncation truncation);
  Node* GetFloat32RepresentationFor(Node* node,
                                    MachineRepresentation output_rep,
                                    Type output_type, Truncation truncation);
  Node* GetFloat64RepresentationFor(Node* node,
                                    MachineRepresentation output_rep,
                                    Type output_type, Node* use_node,
                                    UseInfo use_info);
  Node* GetWord32RepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type output_type, Node* use_node,
                                   UseInfo use_info);
  Node* GetBitRepresentationFor(Node* node, MachineRepresentation output_rep,
                                Type output_type);
  Node* GetWord64RepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type output_type, Node* use_node,
                                   UseInfo use_info);
  Node* TypeError(Node* node, MachineRepresentation output_rep,
                  Type output_type, MachineRepresentation use);
  Node* MakeTruncatedInt32Constant(double value);
  Node* InsertChangeBitToTagged(Node* node);
  Node* InsertChangeFloat32ToFloat64(Node* node);
  Node* InsertChangeFloat64ToInt32(Node* node);
  Node* InsertChangeFloat64ToUint32(Node* node);
  Node* InsertChangeInt32ToFloat64(Node* node);
  Node* InsertChangeTaggedSignedToInt32(Node* node);
  Node* InsertChangeTaggedToFloat64(Node* node);
  Node* InsertChangeUint32ToFloat64(Node* node);
  Node* InsertCheckedFloat64ToInt32(Node* node, CheckForMinusZeroMode check,
                                    const FeedbackSource& feedback,
                                    Node* use_node);
  Node* InsertConversion(Node* node, const Operator* op, Node* use_node);
  Node* InsertTruncateInt64ToInt32(Node* node);
  Node* InsertUnconditionalDeopt(Node* node, DeoptimizeReason reason,
                                 const FeedbackSource& feedback = {});
  Node* InsertTypeOverrideForVerifier(const Type& type, Node* node);

  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  Factory* factory() const { return isolate()->factory(); }
  SimplifiedOperatorBuilder* simplified() { return jsgraph()->simplified(); }
  MachineOperatorBuilder* machine() { return jsgraph()->machine(); }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REPRESENTATION_CHANGE_H_
