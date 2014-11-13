// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_MATCHERS_H_
#define V8_COMPILER_NODE_MATCHERS_H_

#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/unique.h"

namespace v8 {
namespace internal {
namespace compiler {

// A pattern matcher for nodes.
struct NodeMatcher {
  explicit NodeMatcher(Node* node) : node_(node) {}

  Node* node() const { return node_; }
  const Operator* op() const { return node()->op(); }
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
template <typename T, IrOpcode::Value kOpcode>
struct ValueMatcher : public NodeMatcher {
  explicit ValueMatcher(Node* node)
      : NodeMatcher(node), value_(), has_value_(opcode() == kOpcode) {
    if (has_value_) {
      value_ = OpParameter<T>(node);
    }
  }

  bool HasValue() const { return has_value_; }
  const T& Value() const {
    DCHECK(HasValue());
    return value_;
  }

  bool Is(const T& value) const {
    return this->HasValue() && this->Value() == value;
  }

  bool IsInRange(const T& low, const T& high) const {
    return this->HasValue() && low <= this->Value() && this->Value() <= high;
  }

 private:
  T value_;
  bool has_value_;
};


// A pattern matcher for integer constants.
template <typename T, IrOpcode::Value kOpcode>
struct IntMatcher FINAL : public ValueMatcher<T, kOpcode> {
  explicit IntMatcher(Node* node) : ValueMatcher<T, kOpcode>(node) {}

  bool IsPowerOf2() const {
    return this->HasValue() && this->Value() > 0 &&
           (this->Value() & (this->Value() - 1)) == 0;
  }
};

typedef IntMatcher<int32_t, IrOpcode::kInt32Constant> Int32Matcher;
typedef IntMatcher<uint32_t, IrOpcode::kInt32Constant> Uint32Matcher;
typedef IntMatcher<int64_t, IrOpcode::kInt64Constant> Int64Matcher;
typedef IntMatcher<uint64_t, IrOpcode::kInt64Constant> Uint64Matcher;
#if V8_HOST_ARCH_32_BIT
typedef Int32Matcher IntPtrMatcher;
typedef Uint32Matcher UintPtrMatcher;
#else
typedef Int64Matcher IntPtrMatcher;
typedef Uint64Matcher UintPtrMatcher;
#endif


// A pattern matcher for floating point constants.
template <typename T, IrOpcode::Value kOpcode>
struct FloatMatcher FINAL : public ValueMatcher<T, kOpcode> {
  explicit FloatMatcher(Node* node) : ValueMatcher<T, kOpcode>(node) {}

  bool IsNaN() const { return this->HasValue() && std::isnan(this->Value()); }
};

typedef FloatMatcher<float, IrOpcode::kFloat32Constant> Float32Matcher;
typedef FloatMatcher<double, IrOpcode::kFloat64Constant> Float64Matcher;
typedef FloatMatcher<double, IrOpcode::kNumberConstant> NumberMatcher;


// A pattern matcher for heap object constants.
template <typename T>
struct HeapObjectMatcher FINAL
    : public ValueMatcher<Unique<T>, IrOpcode::kHeapConstant> {
  explicit HeapObjectMatcher(Node* node)
      : ValueMatcher<Unique<T>, IrOpcode::kHeapConstant>(node) {}
};


// For shorter pattern matching code, this struct matches both the left and
// right hand sides of a binary operation and can put constants on the right
// if they appear on the left hand side of a commutative operation.
template <typename Left, typename Right>
struct BinopMatcher FINAL : public NodeMatcher {
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
typedef BinopMatcher<IntPtrMatcher, IntPtrMatcher> IntPtrBinopMatcher;
typedef BinopMatcher<UintPtrMatcher, UintPtrMatcher> UintPtrBinopMatcher;
typedef BinopMatcher<Float64Matcher, Float64Matcher> Float64BinopMatcher;
typedef BinopMatcher<NumberMatcher, NumberMatcher> NumberBinopMatcher;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_MATCHERS_H_
