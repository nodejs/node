// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-unittest.h"

#include <ostream>  // NOLINT(readability/streams)

#include "src/compiler/node-properties-inl.h"

using testing::_;
using testing::MakeMatcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::StringMatchResultListener;

namespace v8 {
namespace internal {

// TODO(bmeurer): Find a new home for these functions.
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Unique<T>& value) {
  return os << *value.handle();
}
inline std::ostream& operator<<(std::ostream& os,
                                const ExternalReference& value) {
  OStringStream ost;
  compiler::StaticParameterTraits<ExternalReference>::PrintTo(ost, value);
  return os << ost.c_str();
}

namespace compiler {

GraphTest::GraphTest(int num_parameters) : common_(zone()), graph_(zone()) {
  graph()->SetStart(graph()->NewNode(common()->Start(num_parameters)));
}


GraphTest::~GraphTest() {}


Node* GraphTest::Parameter(int32_t index) {
  return graph()->NewNode(common()->Parameter(index), graph()->start());
}


Node* GraphTest::Float32Constant(volatile float value) {
  return graph()->NewNode(common()->Float32Constant(value));
}


Node* GraphTest::Float64Constant(volatile double value) {
  return graph()->NewNode(common()->Float64Constant(value));
}


Node* GraphTest::Int32Constant(int32_t value) {
  return graph()->NewNode(common()->Int32Constant(value));
}


Node* GraphTest::Int64Constant(int64_t value) {
  return graph()->NewNode(common()->Int64Constant(value));
}


Node* GraphTest::NumberConstant(volatile double value) {
  return graph()->NewNode(common()->NumberConstant(value));
}


Node* GraphTest::HeapConstant(const Unique<HeapObject>& value) {
  return graph()->NewNode(common()->HeapConstant(value));
}


Node* GraphTest::FalseConstant() {
  return HeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->false_value()));
}


Node* GraphTest::TrueConstant() {
  return HeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->true_value()));
}


Matcher<Node*> GraphTest::IsFalseConstant() {
  return IsHeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->false_value()));
}


Matcher<Node*> GraphTest::IsTrueConstant() {
  return IsHeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->true_value()));
}

namespace {

template <typename T>
bool PrintMatchAndExplain(const T& value, const char* value_name,
                          const Matcher<T>& value_matcher,
                          MatchResultListener* listener) {
  StringMatchResultListener value_listener;
  if (!value_matcher.MatchAndExplain(value, &value_listener)) {
    *listener << "whose " << value_name << " " << value << " doesn't match";
    if (value_listener.str() != "") {
      *listener << ", " << value_listener.str();
    }
    return false;
  }
  return true;
}


class NodeMatcher : public MatcherInterface<Node*> {
 public:
  explicit NodeMatcher(IrOpcode::Value opcode) : opcode_(opcode) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    *os << "is a " << IrOpcode::Mnemonic(opcode_) << " node";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    if (node == NULL) {
      *listener << "which is NULL";
      return false;
    }
    if (node->opcode() != opcode_) {
      *listener << "whose opcode is " << IrOpcode::Mnemonic(node->opcode())
                << " but should have been " << IrOpcode::Mnemonic(opcode_);
      return false;
    }
    return true;
  }

 private:
  const IrOpcode::Value opcode_;
};


class IsBranchMatcher FINAL : public NodeMatcher {
 public:
  IsBranchMatcher(const Matcher<Node*>& value_matcher,
                  const Matcher<Node*>& control_matcher)
      : NodeMatcher(IrOpcode::kBranch),
        value_matcher_(value_matcher),
        control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value (";
    value_matcher_.DescribeTo(os);
    *os << ") and control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0),
                                 "value", value_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node),
                                 "control", control_matcher_, listener));
  }

 private:
  const Matcher<Node*> value_matcher_;
  const Matcher<Node*> control_matcher_;
};


class IsMergeMatcher FINAL : public NodeMatcher {
 public:
  IsMergeMatcher(const Matcher<Node*>& control0_matcher,
                 const Matcher<Node*>& control1_matcher)
      : NodeMatcher(IrOpcode::kMerge),
        control0_matcher_(control0_matcher),
        control1_matcher_(control1_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose control0 (";
    control0_matcher_.DescribeTo(os);
    *os << ") and control1 (";
    control1_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node, 0),
                                 "control0", control0_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node, 1),
                                 "control1", control1_matcher_, listener));
  }

 private:
  const Matcher<Node*> control0_matcher_;
  const Matcher<Node*> control1_matcher_;
};


class IsControl1Matcher FINAL : public NodeMatcher {
 public:
  IsControl1Matcher(IrOpcode::Value opcode,
                    const Matcher<Node*>& control_matcher)
      : NodeMatcher(opcode), control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node),
                                 "control", control_matcher_, listener));
  }

 private:
  const Matcher<Node*> control_matcher_;
};


class IsFinishMatcher FINAL : public NodeMatcher {
 public:
  IsFinishMatcher(const Matcher<Node*>& value_matcher,
                  const Matcher<Node*>& effect_matcher)
      : NodeMatcher(IrOpcode::kFinish),
        value_matcher_(value_matcher),
        effect_matcher_(effect_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value (";
    value_matcher_.DescribeTo(os);
    *os << ") and effect (";
    effect_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0),
                                 "value", value_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetEffectInput(node), "effect",
                                 effect_matcher_, listener));
  }

 private:
  const Matcher<Node*> value_matcher_;
  const Matcher<Node*> effect_matcher_;
};


template <typename T>
class IsConstantMatcher FINAL : public NodeMatcher {
 public:
  IsConstantMatcher(IrOpcode::Value opcode, const Matcher<T>& value_matcher)
      : NodeMatcher(opcode), value_matcher_(value_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value (";
    value_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<T>(node), "value", value_matcher_,
                                 listener));
  }

 private:
  const Matcher<T> value_matcher_;
};


class IsPhiMatcher FINAL : public NodeMatcher {
 public:
  IsPhiMatcher(const Matcher<MachineType>& type_matcher,
               const Matcher<Node*>& value0_matcher,
               const Matcher<Node*>& value1_matcher,
               const Matcher<Node*>& control_matcher)
      : NodeMatcher(IrOpcode::kPhi),
        type_matcher_(type_matcher),
        value0_matcher_(value0_matcher),
        value1_matcher_(value1_matcher),
        control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose type (";
    type_matcher_.DescribeTo(os);
    *os << "), value0 (";
    value0_matcher_.DescribeTo(os);
    *os << "), value1 (";
    value1_matcher_.DescribeTo(os);
    *os << ") and control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<MachineType>(node), "type",
                                 type_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0),
                                 "value0", value0_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1),
                                 "value1", value1_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node),
                                 "control", control_matcher_, listener));
  }

 private:
  const Matcher<MachineType> type_matcher_;
  const Matcher<Node*> value0_matcher_;
  const Matcher<Node*> value1_matcher_;
  const Matcher<Node*> control_matcher_;
};


class IsProjectionMatcher FINAL : public NodeMatcher {
 public:
  IsProjectionMatcher(const Matcher<size_t>& index_matcher,
                      const Matcher<Node*>& base_matcher)
      : NodeMatcher(IrOpcode::kProjection),
        index_matcher_(index_matcher),
        base_matcher_(base_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose index (";
    index_matcher_.DescribeTo(os);
    *os << ") and base (";
    base_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<size_t>(node), "index",
                                 index_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0), "base",
                                 base_matcher_, listener));
  }

 private:
  const Matcher<size_t> index_matcher_;
  const Matcher<Node*> base_matcher_;
};


class IsCallMatcher FINAL : public NodeMatcher {
 public:
  IsCallMatcher(const Matcher<CallDescriptor*>& descriptor_matcher,
                const Matcher<Node*>& value0_matcher,
                const Matcher<Node*>& value1_matcher,
                const Matcher<Node*>& value2_matcher,
                const Matcher<Node*>& value3_matcher,
                const Matcher<Node*>& effect_matcher,
                const Matcher<Node*>& control_matcher)
      : NodeMatcher(IrOpcode::kCall),
        descriptor_matcher_(descriptor_matcher),
        value0_matcher_(value0_matcher),
        value1_matcher_(value1_matcher),
        value2_matcher_(value2_matcher),
        value3_matcher_(value3_matcher),
        effect_matcher_(effect_matcher),
        control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value0 (";
    value0_matcher_.DescribeTo(os);
    *os << ") and value1 (";
    value1_matcher_.DescribeTo(os);
    *os << ") and value2 (";
    value2_matcher_.DescribeTo(os);
    *os << ") and value3 (";
    value3_matcher_.DescribeTo(os);
    *os << ") and effect (";
    effect_matcher_.DescribeTo(os);
    *os << ") and control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<CallDescriptor*>(node),
                                 "descriptor", descriptor_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0),
                                 "value0", value0_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1),
                                 "value1", value1_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 2),
                                 "value2", value2_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 3),
                                 "value3", value3_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetEffectInput(node), "effect",
                                 effect_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node),
                                 "control", control_matcher_, listener));
  }

 private:
  const Matcher<CallDescriptor*> descriptor_matcher_;
  const Matcher<Node*> value0_matcher_;
  const Matcher<Node*> value1_matcher_;
  const Matcher<Node*> value2_matcher_;
  const Matcher<Node*> value3_matcher_;
  const Matcher<Node*> effect_matcher_;
  const Matcher<Node*> control_matcher_;
};


class IsLoadMatcher FINAL : public NodeMatcher {
 public:
  IsLoadMatcher(const Matcher<LoadRepresentation>& rep_matcher,
                const Matcher<Node*>& base_matcher,
                const Matcher<Node*>& index_matcher,
                const Matcher<Node*>& effect_matcher)
      : NodeMatcher(IrOpcode::kLoad),
        rep_matcher_(rep_matcher),
        base_matcher_(base_matcher),
        index_matcher_(index_matcher),
        effect_matcher_(effect_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose rep (";
    rep_matcher_.DescribeTo(os);
    *os << "), base (";
    base_matcher_.DescribeTo(os);
    *os << "), index (";
    index_matcher_.DescribeTo(os);
    *os << ") and effect (";
    effect_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<LoadRepresentation>(node), "rep",
                                 rep_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0), "base",
                                 base_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1),
                                 "index", index_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetEffectInput(node), "effect",
                                 effect_matcher_, listener));
  }

 private:
  const Matcher<LoadRepresentation> rep_matcher_;
  const Matcher<Node*> base_matcher_;
  const Matcher<Node*> index_matcher_;
  const Matcher<Node*> effect_matcher_;
};


class IsStoreMatcher FINAL : public NodeMatcher {
 public:
  IsStoreMatcher(const Matcher<MachineType>& type_matcher,
                 const Matcher<WriteBarrierKind> write_barrier_matcher,
                 const Matcher<Node*>& base_matcher,
                 const Matcher<Node*>& index_matcher,
                 const Matcher<Node*>& value_matcher,
                 const Matcher<Node*>& effect_matcher,
                 const Matcher<Node*>& control_matcher)
      : NodeMatcher(IrOpcode::kStore),
        type_matcher_(type_matcher),
        write_barrier_matcher_(write_barrier_matcher),
        base_matcher_(base_matcher),
        index_matcher_(index_matcher),
        value_matcher_(value_matcher),
        effect_matcher_(effect_matcher),
        control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose type (";
    type_matcher_.DescribeTo(os);
    *os << "), write barrier (";
    write_barrier_matcher_.DescribeTo(os);
    *os << "), base (";
    base_matcher_.DescribeTo(os);
    *os << "), index (";
    index_matcher_.DescribeTo(os);
    *os << "), value (";
    value_matcher_.DescribeTo(os);
    *os << "), effect (";
    effect_matcher_.DescribeTo(os);
    *os << ") and control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(
                OpParameter<StoreRepresentation>(node).machine_type(), "type",
                type_matcher_, listener) &&
            PrintMatchAndExplain(
                OpParameter<StoreRepresentation>(node).write_barrier_kind(),
                "write barrier", write_barrier_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0), "base",
                                 base_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1),
                                 "index", index_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 2),
                                 "value", value_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetEffectInput(node), "effect",
                                 effect_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node),
                                 "control", control_matcher_, listener));
  }

 private:
  const Matcher<MachineType> type_matcher_;
  const Matcher<WriteBarrierKind> write_barrier_matcher_;
  const Matcher<Node*> base_matcher_;
  const Matcher<Node*> index_matcher_;
  const Matcher<Node*> value_matcher_;
  const Matcher<Node*> effect_matcher_;
  const Matcher<Node*> control_matcher_;
};


class IsBinopMatcher FINAL : public NodeMatcher {
 public:
  IsBinopMatcher(IrOpcode::Value opcode, const Matcher<Node*>& lhs_matcher,
                 const Matcher<Node*>& rhs_matcher)
      : NodeMatcher(opcode),
        lhs_matcher_(lhs_matcher),
        rhs_matcher_(rhs_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose lhs (";
    lhs_matcher_.DescribeTo(os);
    *os << ") and rhs (";
    rhs_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0), "lhs",
                                 lhs_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1), "rhs",
                                 rhs_matcher_, listener));
  }

 private:
  const Matcher<Node*> lhs_matcher_;
  const Matcher<Node*> rhs_matcher_;
};


class IsUnopMatcher FINAL : public NodeMatcher {
 public:
  IsUnopMatcher(IrOpcode::Value opcode, const Matcher<Node*>& input_matcher)
      : NodeMatcher(opcode), input_matcher_(input_matcher) {}

  virtual void DescribeTo(std::ostream* os) const OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose input (";
    input_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0),
                                 "input", input_matcher_, listener));
  }

 private:
  const Matcher<Node*> input_matcher_;
};
}


Matcher<Node*> IsBranch(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& control_matcher) {
  return MakeMatcher(new IsBranchMatcher(value_matcher, control_matcher));
}


Matcher<Node*> IsMerge(const Matcher<Node*>& control0_matcher,
                       const Matcher<Node*>& control1_matcher) {
  return MakeMatcher(new IsMergeMatcher(control0_matcher, control1_matcher));
}


Matcher<Node*> IsIfTrue(const Matcher<Node*>& control_matcher) {
  return MakeMatcher(new IsControl1Matcher(IrOpcode::kIfTrue, control_matcher));
}


Matcher<Node*> IsIfFalse(const Matcher<Node*>& control_matcher) {
  return MakeMatcher(
      new IsControl1Matcher(IrOpcode::kIfFalse, control_matcher));
}


Matcher<Node*> IsControlEffect(const Matcher<Node*>& control_matcher) {
  return MakeMatcher(
      new IsControl1Matcher(IrOpcode::kControlEffect, control_matcher));
}


Matcher<Node*> IsValueEffect(const Matcher<Node*>& value_matcher) {
  return MakeMatcher(new IsUnopMatcher(IrOpcode::kValueEffect, value_matcher));
}


Matcher<Node*> IsFinish(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& effect_matcher) {
  return MakeMatcher(new IsFinishMatcher(value_matcher, effect_matcher));
}


Matcher<Node*> IsExternalConstant(
    const Matcher<ExternalReference>& value_matcher) {
  return MakeMatcher(new IsConstantMatcher<ExternalReference>(
      IrOpcode::kExternalConstant, value_matcher));
}


Matcher<Node*> IsHeapConstant(
    const Matcher<Unique<HeapObject> >& value_matcher) {
  return MakeMatcher(new IsConstantMatcher<Unique<HeapObject> >(
      IrOpcode::kHeapConstant, value_matcher));
}


Matcher<Node*> IsInt32Constant(const Matcher<int32_t>& value_matcher) {
  return MakeMatcher(
      new IsConstantMatcher<int32_t>(IrOpcode::kInt32Constant, value_matcher));
}


Matcher<Node*> IsInt64Constant(const Matcher<int64_t>& value_matcher) {
  return MakeMatcher(
      new IsConstantMatcher<int64_t>(IrOpcode::kInt64Constant, value_matcher));
}


Matcher<Node*> IsFloat32Constant(const Matcher<float>& value_matcher) {
  return MakeMatcher(
      new IsConstantMatcher<float>(IrOpcode::kFloat32Constant, value_matcher));
}


Matcher<Node*> IsFloat64Constant(const Matcher<double>& value_matcher) {
  return MakeMatcher(
      new IsConstantMatcher<double>(IrOpcode::kFloat64Constant, value_matcher));
}


Matcher<Node*> IsNumberConstant(const Matcher<double>& value_matcher) {
  return MakeMatcher(
      new IsConstantMatcher<double>(IrOpcode::kNumberConstant, value_matcher));
}


Matcher<Node*> IsPhi(const Matcher<MachineType>& type_matcher,
                     const Matcher<Node*>& value0_matcher,
                     const Matcher<Node*>& value1_matcher,
                     const Matcher<Node*>& merge_matcher) {
  return MakeMatcher(new IsPhiMatcher(type_matcher, value0_matcher,
                                      value1_matcher, merge_matcher));
}


Matcher<Node*> IsProjection(const Matcher<size_t>& index_matcher,
                            const Matcher<Node*>& base_matcher) {
  return MakeMatcher(new IsProjectionMatcher(index_matcher, base_matcher));
}


Matcher<Node*> IsCall(const Matcher<CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& value2_matcher,
                      const Matcher<Node*>& value3_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher) {
  return MakeMatcher(new IsCallMatcher(
      descriptor_matcher, value0_matcher, value1_matcher, value2_matcher,
      value3_matcher, effect_matcher, control_matcher));
}


Matcher<Node*> IsLoad(const Matcher<LoadRepresentation>& rep_matcher,
                      const Matcher<Node*>& base_matcher,
                      const Matcher<Node*>& index_matcher,
                      const Matcher<Node*>& effect_matcher) {
  return MakeMatcher(new IsLoadMatcher(rep_matcher, base_matcher, index_matcher,
                                       effect_matcher));
}


Matcher<Node*> IsStore(const Matcher<MachineType>& type_matcher,
                       const Matcher<WriteBarrierKind>& write_barrier_matcher,
                       const Matcher<Node*>& base_matcher,
                       const Matcher<Node*>& index_matcher,
                       const Matcher<Node*>& value_matcher,
                       const Matcher<Node*>& effect_matcher,
                       const Matcher<Node*>& control_matcher) {
  return MakeMatcher(new IsStoreMatcher(
      type_matcher, write_barrier_matcher, base_matcher, index_matcher,
      value_matcher, effect_matcher, control_matcher));
}


#define IS_BINOP_MATCHER(Name)                                            \
  Matcher<Node*> Is##Name(const Matcher<Node*>& lhs_matcher,              \
                          const Matcher<Node*>& rhs_matcher) {            \
    return MakeMatcher(                                                   \
        new IsBinopMatcher(IrOpcode::k##Name, lhs_matcher, rhs_matcher)); \
  }
IS_BINOP_MATCHER(NumberLessThan)
IS_BINOP_MATCHER(NumberSubtract)
IS_BINOP_MATCHER(Word32And)
IS_BINOP_MATCHER(Word32Sar)
IS_BINOP_MATCHER(Word32Shl)
IS_BINOP_MATCHER(Word32Ror)
IS_BINOP_MATCHER(Word32Equal)
IS_BINOP_MATCHER(Word64And)
IS_BINOP_MATCHER(Word64Sar)
IS_BINOP_MATCHER(Word64Shl)
IS_BINOP_MATCHER(Word64Equal)
IS_BINOP_MATCHER(Int32AddWithOverflow)
IS_BINOP_MATCHER(Int32Mul)
IS_BINOP_MATCHER(Uint32LessThanOrEqual)
#undef IS_BINOP_MATCHER


#define IS_UNOP_MATCHER(Name)                                                \
  Matcher<Node*> Is##Name(const Matcher<Node*>& input_matcher) {             \
    return MakeMatcher(new IsUnopMatcher(IrOpcode::k##Name, input_matcher)); \
  }
IS_UNOP_MATCHER(ChangeFloat64ToInt32)
IS_UNOP_MATCHER(ChangeFloat64ToUint32)
IS_UNOP_MATCHER(ChangeInt32ToFloat64)
IS_UNOP_MATCHER(ChangeInt32ToInt64)
IS_UNOP_MATCHER(ChangeUint32ToFloat64)
IS_UNOP_MATCHER(ChangeUint32ToUint64)
IS_UNOP_MATCHER(TruncateFloat64ToFloat32)
IS_UNOP_MATCHER(TruncateFloat64ToInt32)
IS_UNOP_MATCHER(TruncateInt64ToInt32)
IS_UNOP_MATCHER(Float64Sqrt)
#undef IS_UNOP_MATCHER

}  // namespace compiler
}  // namespace internal
}  // namespace v8
