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
  bool IsUnused() const { return kind_ == TruncationKind::kNone; }
  bool IsUsedAsBool() const {
    return LessGeneral(kind_, TruncationKind::kBool);
  }
  bool IsUsedAsWord32() const {
    return LessGeneral(kind_, TruncationKind::kWord32);
  }
  bool IsUsedAsFloat64() const {
    return LessGeneral(kind_, TruncationKind::kFloat64);
  }
  bool IdentifiesNaNAndZero() {
    return LessGeneral(kind_, TruncationKind::kWord32) ||
           LessGeneral(kind_, TruncationKind::kBool);
  }
  bool IdentifiesUndefinedAndNaNAndZero() {
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

enum class TypeCheckKind : uint8_t {
  kNone,
  kSignedSmall,
  kSigned32,
  kNumber,
  kNumberOrOddball
};

inline std::ostream& operator<<(std::ostream& os, TypeCheckKind type_check) {
  switch (type_check) {
    case TypeCheckKind::kNone:
      return os << "None";
    case TypeCheckKind::kSignedSmall:
      return os << "SignedSmall";
    case TypeCheckKind::kSigned32:
      return os << "Signed32";
    case TypeCheckKind::kNumber:
      return os << "Number";
    case TypeCheckKind::kNumberOrOddball:
      return os << "NumberOrOddball";
  }
  UNREACHABLE();
  return os;
}

// The {UseInfo} class is used to describe a use of an input of a node.
//
// This information is used in two different ways, based on the phase:
//
// 1. During propagation, the use info is used to inform the input node
//    about what part of the input is used (we call this truncation) and what
//    is the preferred representation.
//
// 2. During lowering, the use info is used to properly convert the input
//    to the preferred representation. The preferred representation might be
//    insufficient to do the conversion (e.g. word32->float64 conv), so we also
//    need the signedness information to produce the correct value.
class UseInfo {
 public:
  UseInfo(MachineRepresentation representation, Truncation truncation,
          TypeCheckKind type_check = TypeCheckKind::kNone)
      : representation_(representation),
        truncation_(truncation),
        type_check_(type_check) {}
  static UseInfo TruncatingWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Word32());
  }
  static UseInfo TruncatingWord64() {
    return UseInfo(MachineRepresentation::kWord64, Truncation::Word64());
  }
  static UseInfo Bool() {
    return UseInfo(MachineRepresentation::kBit, Truncation::Bool());
  }
  static UseInfo TruncatingFloat32() {
    return UseInfo(MachineRepresentation::kFloat32, Truncation::Float32());
  }
  static UseInfo TruncatingFloat64() {
    return UseInfo(MachineRepresentation::kFloat64, Truncation::Float64());
  }
  static UseInfo PointerInt() {
    return kPointerSize == 4 ? TruncatingWord32() : TruncatingWord64();
  }
  static UseInfo AnyTagged() {
    return UseInfo(MachineRepresentation::kTagged, Truncation::Any());
  }
  static UseInfo TaggedSigned() {
    return UseInfo(MachineRepresentation::kTaggedSigned, Truncation::Any());
  }
  static UseInfo TaggedPointer() {
    return UseInfo(MachineRepresentation::kTaggedPointer, Truncation::Any());
  }

  // Possibly deoptimizing conversions.
  static UseInfo CheckedSignedSmallAsTaggedSigned() {
    return UseInfo(MachineRepresentation::kTaggedSigned, Truncation::Any(),
                   TypeCheckKind::kSignedSmall);
  }
  static UseInfo CheckedSignedSmallAsWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Any(),
                   TypeCheckKind::kSignedSmall);
  }
  static UseInfo CheckedSigned32AsWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Any(),
                   TypeCheckKind::kSigned32);
  }
  static UseInfo CheckedNumberAsFloat64() {
    return UseInfo(MachineRepresentation::kFloat64, Truncation::Float64(),
                   TypeCheckKind::kNumber);
  }
  static UseInfo CheckedNumberAsWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Word32(),
                   TypeCheckKind::kNumber);
  }
  static UseInfo CheckedNumberOrOddballAsFloat64() {
    return UseInfo(MachineRepresentation::kFloat64, Truncation::Any(),
                   TypeCheckKind::kNumberOrOddball);
  }
  static UseInfo CheckedNumberOrOddballAsWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Word32(),
                   TypeCheckKind::kNumberOrOddball);
  }

  // Undetermined representation.
  static UseInfo Any() {
    return UseInfo(MachineRepresentation::kNone, Truncation::Any());
  }
  static UseInfo AnyTruncatingToBool() {
    return UseInfo(MachineRepresentation::kNone, Truncation::Bool());
  }

  // Value not used.
  static UseInfo None() {
    return UseInfo(MachineRepresentation::kNone, Truncation::None());
  }

  MachineRepresentation representation() const { return representation_; }
  Truncation truncation() const { return truncation_; }
  TypeCheckKind type_check() const { return type_check_; }

 private:
  MachineRepresentation representation_;
  Truncation truncation_;
  TypeCheckKind type_check_;
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
                             Type* output_type, Node* use_node,
                             UseInfo use_info);
  const Operator* Int32OperatorFor(IrOpcode::Value opcode);
  const Operator* Int32OverflowOperatorFor(IrOpcode::Value opcode);
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

 private:
  JSGraph* jsgraph_;
  Isolate* isolate_;

  friend class RepresentationChangerTester;  // accesses the below fields.

  bool testing_type_errors_;  // If {true}, don't abort on a type error.
  bool type_error_;           // Set when a type error is detected.

  Node* GetTaggedSignedRepresentationFor(Node* node,
                                         MachineRepresentation output_rep,
                                         Type* output_type, Node* use_node,
                                         UseInfo use_info);
  Node* GetTaggedPointerRepresentationFor(Node* node,
                                          MachineRepresentation output_rep,
                                          Type* output_type);
  Node* GetTaggedRepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type* output_type, Truncation truncation);
  Node* GetFloat32RepresentationFor(Node* node,
                                    MachineRepresentation output_rep,
                                    Type* output_type, Truncation truncation);
  Node* GetFloat64RepresentationFor(Node* node,
                                    MachineRepresentation output_rep,
                                    Type* output_type, Node* use_node,
                                    UseInfo use_info);
  Node* GetWord32RepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type* output_type, Node* use_node,
                                   UseInfo use_info);
  Node* GetBitRepresentationFor(Node* node, MachineRepresentation output_rep,
                                Type* output_type);
  Node* GetWord64RepresentationFor(Node* node, MachineRepresentation output_rep,
                                   Type* output_type);
  Node* TypeError(Node* node, MachineRepresentation output_rep,
                  Type* output_type, MachineRepresentation use);
  Node* MakeTruncatedInt32Constant(double value);
  Node* InsertChangeBitToTagged(Node* node);
  Node* InsertChangeFloat32ToFloat64(Node* node);
  Node* InsertChangeFloat64ToInt32(Node* node);
  Node* InsertChangeFloat64ToUint32(Node* node);
  Node* InsertChangeTaggedSignedToInt32(Node* node);
  Node* InsertChangeTaggedToFloat64(Node* node);

  Node* InsertConversion(Node* node, const Operator* op, Node* use_node);

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
