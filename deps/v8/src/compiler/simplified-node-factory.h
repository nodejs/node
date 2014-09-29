// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_NODE_FACTORY_H_
#define V8_COMPILER_SIMPLIFIED_NODE_FACTORY_H_

#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

#define SIMPLIFIED() static_cast<NodeFactory*>(this)->simplified()
#define NEW_NODE_1(op, a) static_cast<NodeFactory*>(this)->NewNode(op, a)
#define NEW_NODE_2(op, a, b) static_cast<NodeFactory*>(this)->NewNode(op, a, b)
#define NEW_NODE_3(op, a, b, c) \
  static_cast<NodeFactory*>(this)->NewNode(op, a, b, c)

template <typename NodeFactory>
class SimplifiedNodeFactory {
 public:
  Node* BooleanNot(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->BooleanNot(), a);
  }

  Node* NumberEqual(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberEqual(), a, b);
  }
  Node* NumberNotEqual(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberNotEqual(), a, b);
  }
  Node* NumberLessThan(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberLessThan(), a, b);
  }
  Node* NumberLessThanOrEqual(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberLessThanOrEqual(), a, b);
  }
  Node* NumberAdd(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberAdd(), a, b);
  }
  Node* NumberSubtract(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberSubtract(), a, b);
  }
  Node* NumberMultiply(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberMultiply(), a, b);
  }
  Node* NumberDivide(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberDivide(), a, b);
  }
  Node* NumberModulus(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->NumberModulus(), a, b);
  }
  Node* NumberToInt32(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->NumberToInt32(), a);
  }
  Node* NumberToUint32(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->NumberToUint32(), a);
  }

  Node* ReferenceEqual(Type* type, Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->ReferenceEqual(), a, b);
  }

  Node* StringEqual(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->StringEqual(), a, b);
  }
  Node* StringLessThan(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->StringLessThan(), a, b);
  }
  Node* StringLessThanOrEqual(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->StringLessThanOrEqual(), a, b);
  }
  Node* StringAdd(Node* a, Node* b) {
    return NEW_NODE_2(SIMPLIFIED()->StringAdd(), a, b);
  }

  Node* ChangeTaggedToInt32(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeTaggedToInt32(), a);
  }
  Node* ChangeTaggedToUint32(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeTaggedToUint32(), a);
  }
  Node* ChangeTaggedToFloat64(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeTaggedToFloat64(), a);
  }
  Node* ChangeInt32ToTagged(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeInt32ToTagged(), a);
  }
  Node* ChangeUint32ToTagged(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeUint32ToTagged(), a);
  }
  Node* ChangeFloat64ToTagged(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeFloat64ToTagged(), a);
  }
  Node* ChangeBoolToBit(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeBoolToBit(), a);
  }
  Node* ChangeBitToBool(Node* a) {
    return NEW_NODE_1(SIMPLIFIED()->ChangeBitToBool(), a);
  }

  Node* LoadField(const FieldAccess& access, Node* object) {
    return NEW_NODE_1(SIMPLIFIED()->LoadField(access), object);
  }
  Node* StoreField(const FieldAccess& access, Node* object, Node* value) {
    return NEW_NODE_2(SIMPLIFIED()->StoreField(access), object, value);
  }
  Node* LoadElement(const ElementAccess& access, Node* object, Node* index) {
    return NEW_NODE_2(SIMPLIFIED()->LoadElement(access), object, index);
  }
  Node* StoreElement(const ElementAccess& access, Node* object, Node* index,
                     Node* value) {
    return NEW_NODE_3(SIMPLIFIED()->StoreElement(access), object, index, value);
  }
};

#undef NEW_NODE_1
#undef NEW_NODE_2
#undef NEW_NODE_3
#undef SIMPLIFIED

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_NODE_FACTORY_H_
