// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_AUX_DATA_H_
#define V8_COMPILER_NODE_AUX_DATA_H_

#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Node;

template <class T>
class NodeAuxData {
 public:
  explicit NodeAuxData(Zone* zone) : aux_data_(zone) {}

  void Set(Node* node, T const& data) {
    size_t const id = node->id();
    if (id >= aux_data_.size()) aux_data_.resize(id + 1);
    aux_data_[id] = data;
  }

  T Get(Node* node) const {
    size_t const id = node->id();
    return (id < aux_data_.size()) ? aux_data_[id] : T();
  }

 private:
  ZoneVector<T> aux_data_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_AUX_DATA_H_
