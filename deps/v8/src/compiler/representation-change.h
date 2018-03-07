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

enum IdentifyZeros { kIdentifyZeros, kDistinguishZeros };

class Truncation final {
 public:
  // Constructors.
  static Truncation None() {
    return Truncation(TruncationKind::kNone, kIdentifyZeros);
  }
  static Truncation Bool() {
    return Truncation(TruncationKind::kBool, kIdentifyZeros);
  }
  static Truncation Word32() {
    return Truncation(TruncationKind::kWord32, kIdentifyZeros);
  }
  static Truncation Word64() {
    return Truncation(TruncationKind::kWord64, kIdentifyZeros);
  }
  static Truncation Float64(IdentifyZeros identify_zeros = kDistinguishZeros) {
    return Truncation(TruncationKind::kFloat64, identify_zeros);
  }
  static Truncation Any(IdentifyZeros identify_zeros = kDistinguishZeros) {
    return Truncation(TruncationKind::kAny, identify_zeros);
  }

  static Truncation Generalize(Truncation t1, Truncation t2) {
    return Truncation(
        Generalize(t1.kind(), t2.kind()),
        GeneralizeIdentifyZeros(t1.identify_zeros(), t2.identify_zeros()));
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
  bool IdentifiesUndefinedAndZero() {
    return LessGeneral(kind_, TruncationKind::kWord32) ||
           LessGeneral(kind_, TruncationKind::kBool);
  }
  bool IdentifiesUndefinedAndNaN() {
    return LessGeneral(kind_, TruncationKind::kFloat64) ||
           LessGeneral(kind_, TruncationKind::kWord64);
  }
  bool IdentifiesZeroAndMinusZero() const {
    return identify_zeros() == kIdentifyZeros;
  }

  // Operators.
  bool operator==(Truncation other) const {
    return kind() == other.kind() && identify_zeros() == other.identify_zeros();
  }
  bool operator!=(Truncation other) const { return !(*this == other); }

  // Debug utilities.
  const char* description() const;
  bool IsLessGeneralThan(Truncation other) {
    return LessGeneral(kind(), other.kind()) &&
           LessGeneralIdentifyZeros(identify_zeros(), other.identify_zeros());
  }

  IdentifyZeros identify_zeros() const { return identify_zeros_; }

 private:
  enum class TruncationKind : uint8_t {
    kNone,
    kBool,
    kWord32,
    kWord64,
    kFloat64,
    kAny
  };

  explicit Truncation(TruncationKind kind, IdentifyZeros identify_zeros)
      : kind_(kind), identify_zeros_(identify_zeros) {
    DCHECK(kind == TruncationKind::kAny || kind == TruncationKind::kFloat64 ||
           identify_zeros == kIdentifyZeros);
  }
  TruncationKind kind() const { return kind_; }

  TruncationKind kind_;
  IdentifyZeros identify_zeros_;

  static TruncationKind Generalize(TruncationKind rep1, TruncationKind rep2);
  static IdentifyZeros GeneralizeIdentifyZeros(IdentifyZeros i1,
                                               IdentifyZeros i2);
  static bool LessGeneral(TruncationKind rep1, TruncationKind rep2);
  static bool LessGeneralIdentifyZeros(IdentifyZeros u1, IdentifyZeros u2);
};

enum class TypeCheckKind : uint8_t {
  kNone,
  kSignedSmall,
  kSigned32,
  kNumber,
  kNumberOrOddball,
  kHeapObject
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
    case TypeCheckKind::kHeapObject:
      return os << "HeapObject";
  }
  UNREACHABLE();
}

// The {UseInfo} class is used to describe a use of an input of a node.
//
// This information is used in two different ways, based on the phase:
//
// 1. During propagation, the use info is used to inform the input node
//    about what part of the input is used (we call this truncation) and what
//    is the preferred representation. For conversions that will require
//    checks, we also keep track of whether a minus zero check is needed.
//
// 2. During lowering, the use info is used to properly convert the input
//    to the preferred representation. The preferred representation might be
//    insufficient to do the conversion (e.g. word32->float64 conv), so we also
//    need the signedness information to produce the correct value.
//    Additionally, use info may contain {CheckParameters} which contains
//    information for the deoptimizer such as a CallIC on which speculation
//    should be disallowed if the check fails.
class UseInfo {
 public:
  UseInfo(MachineRepresentation representation, Truncation truncation,
          TypeCheckKind type_check = TypeCheckKind::kNone,
          const VectorSlotPair& feedback = VectorSlotPair())
      : representation_(representation),
        truncation_(truncation),
        type_check_(type_check),
        feedback_(feedback) {}
  static UseInfo TruncatingWord32() {
    return UseInfo(MachineRepresentation::kWord32, Truncation::Word32());
  }
  static UseInfo TruncatingWord64() {
    return UseInfo(MachineRepresentation::kWord64, Truncation::Word64());
  }
  static UseInfo Bool() {
    return UseInfo(MachineRepresentation::kBit, Truncation::Bool());
  }
  static UseInfo Float32() {
    return UseInfo(MachineRepresentation::kFloat32, Truncation::Any());
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
  static UseInfo CheckedHeapObjectAsTaggedPointer() {
    return UseInfo(MachineRepresentation::kTaggedPointer, Truncation::Any(),
                   TypeCheckKind::kHeapObject);
  }
  static UseInfo CheckedSignedSmallAsTaggedSigned(
      const VectorSlotPair& feedback) {
    return UseInfo(MachineRepresentation::kTaggedSigned, Truncation::Any(),
                   TypeCheckKind::kSignedSmall, feedback);
  }
  static UseInfo CheckedSignedSmallAsWord32(IdentifyZeros identify_zeros,
                                            const VectorSlotPair& feedback) {
    return UseInfo(MachineRepresentation::kWord32,
                   Truncation::Any(identify_zeros), TypeCheckKind::kSignedSmall,
                   feedback);
  }
  static UseInfo CheckedSigned32AsWord32(IdentifyZeros identify_zeros) {
    return UseInfo(MachineRepresentation::kWord32,
                   Truncation::Any(identify_zeros), TypeCheckKind::kSigned32);
  }
  static UseInfo CheckedNumberAsFloat64() {
    return UseInfo(MachineRepresentation::kFloat64, Truncation::Any(),
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
  CheckForMinusZeroMode minus_zero_check() const {
    return truncation().IdentifiesZeroAndMinusZero()
               ? CheckForMinusZeroMode::kDontCheckForMinusZero
               : CheckForMinusZeroMode::kCheckForMinusZero;
  }
  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  MachineRepresentation representation_;
  Truncation truncation_;
  TypeCheckKind type_check_;
  VectorSlotPair feedback_;
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
                                          Type* output_type, Node* use_node,
                                          UseInfo use_info);
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
  Node* InsertChangeInt32ToFloat64(Node* node);
  Node* InsertChangeTaggedSignedToInt32(Node* node);
  Node* InsertChangeTaggedToFloat64(Node* node);
  Node* InsertChangeUint32ToFloat64(Node* node);

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
