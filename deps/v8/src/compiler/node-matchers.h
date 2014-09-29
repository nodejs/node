// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_MATCHERS_H_
#define V8_COMPILER_NODE_MATCHERS_H_

#include "src/compiler/common-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// A pattern matcher for nodes.
struct NodeMatcher {
  explicit NodeMatcher(Node* node) : node_(node) {}

  Node* node() const { return node_; }
  Operator* op() const { return node()->op(); }
  IrOpcode::Value opcode() const { return node()->opcode(); }

  bool HasProperty(Operator::Property property) const {
    return op()->HasProperty(property);
  }
  Node* InputAt(int index) const { return node()->InputAt(index); }

#define DEFINE_IS_OPCODE(Opcode) \
  bool Is##Opcode() const { return opcode() == IrOpcode::k##Opcode; }
  ALL_OP_LIST(DEFINE_IS_OPCODE)
#undef DEFINE_IS_OPCODE

 private:
  Node* node_;
};


// A pattern matcher for abitrary value constants.
template <typename T>
struct ValueMatcher : public NodeMatcher {
  explicit ValueMatcher(Node* node)
      : NodeMatcher(node),
        value_(),
        has_value_(CommonOperatorTraits<T>::HasValue(node->op())) {
    if (has_value_) value_ = CommonOperatorTraits<T>::ValueOf(node->op());
  }

  bool HasValue() const { return has_value_; }
  T Value() const {
    DCHECK(HasValue());
    return value_;
  }

  bool Is(T value) const {
    return HasValue() && CommonOperatorTraits<T>::Equals(Value(), value);
  }

  bool IsInRange(T low, T high) const {
    return HasValue() && low <= value_ && value_ <= high;
  }

 private:
  T value_;
  bool has_value_;
};


// A pattern matcher for integer constants.
template <typename T>
struct IntMatcher V8_FINAL : public ValueMatcher<T> {
  explicit IntMatcher(Node* node) : ValueMatcher<T>(node) {}

  bool IsPowerOf2() const {
    return this->HasValue() && this->Value() > 0 &&
           (this->Value() & (this->Value() - 1)) == 0;
  }
};

typedef IntMatcher<int32_t> Int32Matcher;
typedef IntMatcher<uint32_t> Uint32Matcher;
typedef IntMatcher<int64_t> Int64Matcher;
typedef IntMatcher<uint64_t> Uint64Matcher;


// A pattern matcher for floating point constants.
template <typename T>
struct FloatMatcher V8_FINAL : public ValueMatcher<T> {
  explicit FloatMatcher(Node* node) : ValueMatcher<T>(node) {}

  bool IsNaN() const { return this->HasValue() && std::isnan(this->Value()); }
};

typedef FloatMatcher<double> Float64Matcher;


// For shorter pattern matching code, this struct matches both the left and
// right hand sides of a binary operation and can put constants on the right
// if they appear on the left hand side of a commutative operation.
template <typename Left, typename Right>
struct BinopMatcher V8_FINAL : public NodeMatcher {
  explicit BinopMatcher(Node* node)
      : NodeMatcher(node), left_(InputAt(0)), right_(InputAt(1)) {
    if (HasProperty(Operator::kCommutative)) PutConstantOnRight();
  }

  const Left& left() const { return left_; }
  const Right& right() const { return right_; }

  bool IsFoldable() const { return left().HasValue() && right().HasValue(); }
  bool LeftEqualsRight() const { return left().node() == right().node(); }

 private:
  void PutConstantOnRight() {
    if (left().HasValue() && !right().HasValue()) {
      std::swap(left_, right_);
      node()->ReplaceInput(0, left().node());
      node()->ReplaceInput(1, right().node());
    }
  }

  Left left_;
  Right right_;
};

typedef BinopMatcher<Int32Matcher, Int32Matcher> Int32BinopMatcher;
typedef BinopMatcher<Uint32Matcher, Uint32Matcher> Uint32BinopMatcher;
typedef BinopMatcher<Int64Matcher, Int64Matcher> Int64BinopMatcher;
typedef BinopMatcher<Uint64Matcher, Uint64Matcher> Uint64BinopMatcher;
typedef BinopMatcher<Float64Matcher, Float64Matcher> Float64BinopMatcher;
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_NODE_MATCHERS_H_
