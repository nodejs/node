// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_AUX_DATA_INL_H_
#define V8_COMPILER_NODE_AUX_DATA_INL_H_

#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-aux-data.h"

namespace v8 {
namespace internal {
namespace compiler {

template <class T>
NodeAuxData<T>::NodeAuxData(Zone* zone)
    : aux_data_(zone) {}


template <class T>
void NodeAuxData<T>::Set(Node* node, const T& data) {
  int id = node->id();
  if (id >= static_cast<int>(aux_data_.size())) {
    aux_data_.resize(id + 1);
  }
  aux_data_[id] = data;
}


template <class T>
T NodeAuxData<T>::Get(Node* node) const {
  int id = node->id();
  if (id >= static_cast<int>(aux_data_.size())) {
    return T();
  }
  return aux_data_[id];
}
}
}
}  // namespace v8::internal::compiler

#endif
