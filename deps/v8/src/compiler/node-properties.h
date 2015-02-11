// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_PROPERTIES_H_
#define V8_COMPILER_NODE_PROPERTIES_H_

#include "src/v8.h"

#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

class Node;
class Operator;

// A facade that simplifies access to the different kinds of inputs to a node.
class NodeProperties {
 public:
  static inline Node* GetValueInput(Node* node, int index);
  static inline Node* GetContextInput(Node* node);
  static inline Node* GetEffectInput(Node* node, int index = 0);
  static inline Node* GetControlInput(Node* node, int index = 0);

  static inline bool IsValueEdge(Node::Edge edge);
  static inline bool IsContextEdge(Node::Edge edge);
  static inline bool IsEffectEdge(Node::Edge edge);
  static inline bool IsControlEdge(Node::Edge edge);

  static inline bool IsControl(Node* node);

  static inline void ReplaceEffectInput(Node* node, Node* effect,
                                        int index = 0);
  static inline void RemoveNonValueInputs(Node* node);

  static inline Bounds GetBounds(Node* node);
  static inline void SetBounds(Node* node, Bounds bounds);

  static inline int GetContextIndex(Node* node);

  static inline int FirstValueIndex(Node* node);
  static inline int FirstEffectIndex(Node* node);
  static inline int FirstControlIndex(Node* node);
  static inline int PastValueIndex(Node* node);
  static inline int PastContextIndex(Node* node);
  static inline int PastEffectIndex(Node* node);
  static inline int PastControlIndex(Node* node);

  static inline bool IsInputRange(Node::Edge edge, int first, int count);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_NODE_PROPERTIES_H_
