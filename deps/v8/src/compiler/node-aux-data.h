// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_AUX_DATA_H_
#define V8_COMPILER_NODE_AUX_DATA_H_

#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;
class Node;

template <class T>
class NodeAuxData {
 public:
  inline explicit NodeAuxData(Zone* zone);

  inline void Set(Node* node, const T& data);
  inline T Get(Node* node);

 private:
  ZoneVector<T> aux_data_;
};
}
}
}  // namespace v8::internal::compiler

#endif
