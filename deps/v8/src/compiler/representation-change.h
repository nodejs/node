// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REPRESENTATION_CHANGE_H_
#define V8_COMPILER_REPRESENTATION_CHANGE_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

class Truncation final {
 public:
  // Constructors.
  static Truncation None() { return Truncation(TruncationKind::kNone); }
  static Truncation Bool() { return Truncation(TruncationKind::kBool); }
  static Truncation Word32() { return Truncation(TruncationKind::kWord32); }
  static Truncation Word64() { return Truncation(TruncationKind::kWord64); }
  static Truncation Float32() { return Truncation(TruncationKind::kFloat32); }
  static Truncation Float64() { return Truncation(TruncationKind::kFloat64); }
  static Truncation Any() { return Truncation(TruncationKind::kAny); }

  static Truncation Generalize(Truncation t1, Truncation t2) {
    return Truncation(Generalize(t1.kind(), t2.kind()));
  }

  // Queries.
  bool TruncatesToWord32() const {
    return LessGeneral(kind_, TruncationKind::kWord32);
  }
  bool TruncatesNaNToZero() {
    return LessGeneral(kind_, TruncationKind::kWord32) ||
           LessGeneral(kind_, TruncationKind::kBool);
  }
  bool TruncatesUndefinedToZeroOrNaN() {
    return LessGeneral(kind_, TruncationKind::kFloat64) ||
           LessGeneral(kind_, TruncationKind::kWord64);
  }

  // Operators.
  bool operator==(Truncation other) const { return kind() == other.kind(); }
  bool operator!=(Truncation other) const { return !(*this == other); }

  // Debug utilities.
  const char* description() const;
  bool IsLessGeneralThan(Truncation other) {
    return LessGeneral(kind(), other.kind());
  }

 private:
  enum class TruncationKind : uint8_t {
    kNone,
    kBool,
    kWord32,
    kWord64,
    kFloat32,
    kFloat64,
    kAny
  };

  explicit Truncation(TruncationKind kind) : kind_(kind) {}
  TruncationKind kind() const { return kind_; }

  TruncationKind kind_;

  static TruncationKind Generalize(TruncationKind rep1, TruncationKind rep2);
  static bool LessGeneral(TruncationKind rep1, TruncationKind rep2);
};


// Contains logic related to changing the representation of values for constants
// and other nodes, as well as lowering Simplified->Machine operators.
// Eagerly folds any representation changes for constants.
class RepresentationChanger final {
 public:
  RepresentationChanger(JSGraph* jsgraph, Isolate* isolate)
      : jsgraph_(jsgraph),
        isolate_(isolate),
        testing_type_errors_(false),
        type_error_(false) {}

  // Changes representation from {output_type} to {use_rep}. The {truncation}
  // parameter is only used for sanity checking - if the changer cannot figure
  // out signedness for the word32->float64 conversion, then we check that the
  // uses truncate to word32 (so they do not care about signedness).
  Node* GetRepresentationFor(Node* node, MachineRepresentation output_rep,
                             Type* output_type, MachineRepresentation use_rep,
                             Truncation truncation = Truncation::None());
  const Operator* Int32OperatorFor(IrOpcode::Value opcode);
  const Operator* Uint32OperatorFor(IrOpcode::Value opcode);
  const Operator* Float64OperatorFor(IrOpcode::Value opcode);

  MachineType TypeForBasePointer(const FieldAccess& access) {
    return access.tag() != 0 ? MachineType::AnyTagged()
                             : MachineType::Pointer();
  }

  MachineType TypeForBasePointer(const ElementAccess& access) {
    return access.tag() != 0 ? MachineType::AnyTagged()
                             : MachineType::Pointer();
  }

 private:
  JSGraph* jsgraph_;
  Isolate* isolate_;

  friend class RepresentationChangerTester;  // accesses the below fields.

  bool testing_type_errors_;  // If {true}, don't abort on a type error.
  bool type_error_;           // Set when a type error is detected.

  Node* GetTaggedRepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type* output_type);
  Node* GetFloat32RepresentationFor(Node* node,
                                    MachineRepresentation output_rep,
                                    Type* output_type, Truncation truncation);
  Node* GetFloat64RepresentationFor(Node* node,
                                    MachineRepresentation output_rep,
                                    Type* output_type, Truncation truncation);
  Node* GetWord32RepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type* output_type);
  Node* GetBitRepresentationFor(Node* node, MachineRepresentation output_rep,
                                Type* output_type);
  Node* GetWord64RepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type* output_type);
  Node* TypeError(Node* node, MachineRepresentation output_rep,
                  Type* output_type, MachineRepresentation use);
  Node* MakeTruncatedInt32Constant(double value);
  Node* InsertChangeFloat32ToFloat64(Node* node);
  Node* InsertChangeTaggedToFloat64(Node* node);

  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate()->factory(); }
  SimplifiedOperatorBuilder* simplified() { return jsgraph()->simplified(); }
  MachineOperatorBuilder* machine() { return jsgraph()->machine(); }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REPRESENTATION_CHANGE_H_
