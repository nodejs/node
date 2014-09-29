// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/compiler-unittests/node-matchers.h"

#include <ostream>  // NOLINT(readability/streams)

#include "src/compiler/node-properties-inl.h"

using testing::MakeMatcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::StringMatchResultListener;

namespace v8 {
namespace internal {

// TODO(bmeurer): Find a new home for these functions.
template <typename T>
inline std::ostream& operator<<(std::ostream& os,
                                const PrintableUnique<T>& value) {
  return os << value.string();
}

namespace compiler {

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

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    *os << "is a " << IrOpcode::Mnemonic(opcode_) << " node";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
    if (node == NULL) {
      *listener << "which is NULL";
      return false;
    }
    if (node->opcode() != opcode_) {
      *listener << "whose opcode is " << IrOpcode::Mnemonic(node->opcode());
      return false;
    }
    return true;
  }

 private:
  const IrOpcode::Value opcode_;
};


class IsBranchMatcher V8_FINAL : public NodeMatcher {
 public:
  IsBranchMatcher(const Matcher<Node*>& value_matcher,
                  const Matcher<Node*>& control_matcher)
      : NodeMatcher(IrOpcode::kBranch),
        value_matcher_(value_matcher),
        control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value (";
    value_matcher_.DescribeTo(os);
    *os << ") and control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
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


template <typename T>
class IsConstantMatcher V8_FINAL : public NodeMatcher {
 public:
  IsConstantMatcher(IrOpcode::Value opcode, const Matcher<T>& value_matcher)
      : NodeMatcher(opcode), value_matcher_(value_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value (";
    value_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<T>(node), "value", value_matcher_,
                                 listener));
  }

 private:
  const Matcher<T> value_matcher_;
};


class IsPhiMatcher V8_FINAL : public NodeMatcher {
 public:
  IsPhiMatcher(const Matcher<Node*>& value0_matcher,
               const Matcher<Node*>& value1_matcher,
               const Matcher<Node*>& control_matcher)
      : NodeMatcher(IrOpcode::kPhi),
        value0_matcher_(value0_matcher),
        value1_matcher_(value1_matcher),
        control_matcher_(control_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose value0 (";
    value0_matcher_.DescribeTo(os);
    *os << "), value1 (";
    value1_matcher_.DescribeTo(os);
    *os << ") and control (";
    control_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0),
                                 "value0", value0_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1),
                                 "value1", value1_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetControlInput(node),
                                 "control", control_matcher_, listener));
  }

 private:
  const Matcher<Node*> value0_matcher_;
  const Matcher<Node*> value1_matcher_;
  const Matcher<Node*> control_matcher_;
};


class IsProjectionMatcher V8_FINAL : public NodeMatcher {
 public:
  IsProjectionMatcher(const Matcher<int32_t>& index_matcher,
                      const Matcher<Node*>& base_matcher)
      : NodeMatcher(IrOpcode::kProjection),
        index_matcher_(index_matcher),
        base_matcher_(base_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose index (";
    index_matcher_.DescribeTo(os);
    *os << ") and base (";
    base_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<int32_t>(node), "index",
                                 index_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0), "base",
                                 base_matcher_, listener));
  }

 private:
  const Matcher<int32_t> index_matcher_;
  const Matcher<Node*> base_matcher_;
};


class IsLoadMatcher V8_FINAL : public NodeMatcher {
 public:
  IsLoadMatcher(const Matcher<MachineType>& type_matcher,
                const Matcher<Node*>& base_matcher,
                const Matcher<Node*>& index_matcher,
                const Matcher<Node*>& effect_matcher)
      : NodeMatcher(IrOpcode::kLoad),
        type_matcher_(type_matcher),
        base_matcher_(base_matcher),
        index_matcher_(index_matcher),
        effect_matcher_(effect_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose type (";
    type_matcher_.DescribeTo(os);
    *os << "), base (";
    base_matcher_.DescribeTo(os);
    *os << "), index (";
    index_matcher_.DescribeTo(os);
    *os << ") and effect (";
    effect_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<MachineType>(node), "type",
                                 type_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 0), "base",
                                 base_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetValueInput(node, 1),
                                 "index", index_matcher_, listener) &&
            PrintMatchAndExplain(NodeProperties::GetEffectInput(node), "effect",
                                 effect_matcher_, listener));
  }

 private:
  const Matcher<MachineType> type_matcher_;
  const Matcher<Node*> base_matcher_;
  const Matcher<Node*> index_matcher_;
  const Matcher<Node*> effect_matcher_;
};


class IsStoreMatcher V8_FINAL : public NodeMatcher {
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

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
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
      V8_OVERRIDE {
    return (NodeMatcher::MatchAndExplain(node, listener) &&
            PrintMatchAndExplain(OpParameter<StoreRepresentation>(node).rep,
                                 "type", type_matcher_, listener) &&
            PrintMatchAndExplain(
                OpParameter<StoreRepresentation>(node).write_barrier_kind,
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


class IsBinopMatcher V8_FINAL : public NodeMatcher {
 public:
  IsBinopMatcher(IrOpcode::Value opcode, const Matcher<Node*>& lhs_matcher,
                 const Matcher<Node*>& rhs_matcher)
      : NodeMatcher(opcode),
        lhs_matcher_(lhs_matcher),
        rhs_matcher_(rhs_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose lhs (";
    lhs_matcher_.DescribeTo(os);
    *os << ") and rhs (";
    rhs_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
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


class IsUnopMatcher V8_FINAL : public NodeMatcher {
 public:
  IsUnopMatcher(IrOpcode::Value opcode, const Matcher<Node*>& input_matcher)
      : NodeMatcher(opcode), input_matcher_(input_matcher) {}

  virtual void DescribeTo(std::ostream* os) const V8_OVERRIDE {
    NodeMatcher::DescribeTo(os);
    *os << " whose input (";
    input_matcher_.DescribeTo(os);
    *os << ")";
  }

  virtual bool MatchAndExplain(Node* node, MatchResultListener* listener) const
      V8_OVERRIDE {
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


Matcher<Node*> IsInt32Constant(const Matcher<int32_t>& value_matcher) {
  return MakeMatcher(
      new IsConstantMatcher<int32_t>(IrOpcode::kInt32Constant, value_matcher));
}


Matcher<Node*> IsHeapConstant(
    const Matcher<PrintableUnique<HeapObject> >& value_matcher) {
  return MakeMatcher(new IsConstantMatcher<PrintableUnique<HeapObject> >(
      IrOpcode::kHeapConstant, value_matcher));
}


Matcher<Node*> IsPhi(const Matcher<Node*>& value0_matcher,
                     const Matcher<Node*>& value1_matcher,
                     const Matcher<Node*>& merge_matcher) {
  return MakeMatcher(
      new IsPhiMatcher(value0_matcher, value1_matcher, merge_matcher));
}


Matcher<Node*> IsProjection(const Matcher<int32_t>& index_matcher,
                            const Matcher<Node*>& base_matcher) {
  return MakeMatcher(new IsProjectionMatcher(index_matcher, base_matcher));
}


Matcher<Node*> IsLoad(const Matcher<MachineType>& type_matcher,
                      const Matcher<Node*>& base_matcher,
                      const Matcher<Node*>& index_matcher,
                      const Matcher<Node*>& effect_matcher) {
  return MakeMatcher(new IsLoadMatcher(type_matcher, base_matcher,
                                       index_matcher, effect_matcher));
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
IS_BINOP_MATCHER(Word32And)
IS_BINOP_MATCHER(Word32Sar)
IS_BINOP_MATCHER(Word32Equal)
IS_BINOP_MATCHER(Word64And)
IS_BINOP_MATCHER(Word64Sar)
IS_BINOP_MATCHER(Word64Shl)
IS_BINOP_MATCHER(Word64Equal)
IS_BINOP_MATCHER(Int32AddWithOverflow)
#undef IS_BINOP_MATCHER


#define IS_UNOP_MATCHER(Name)                                                \
  Matcher<Node*> Is##Name(const Matcher<Node*>& input_matcher) {             \
    return MakeMatcher(new IsUnopMatcher(IrOpcode::k##Name, input_matcher)); \
  }
IS_UNOP_MATCHER(ConvertInt64ToInt32)
IS_UNOP_MATCHER(ChangeInt32ToFloat64)
#undef IS_UNOP_MATCHER

}  // namespace compiler
}  // namespace internal
}  // namespace v8
