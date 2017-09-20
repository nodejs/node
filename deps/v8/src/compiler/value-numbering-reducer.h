// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_VALUE_NUMBERING_REDUCER_H_
#define V8_COMPILER_VALUE_NUMBERING_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

class V8_EXPORT_PRIVATE ValueNumberingReducer final
    : public NON_EXPORTED_BASE(Reducer) {
 public:
  explicit ValueNumberingReducer(Zone* temp_zone, Zone* graph_zone);
  ~ValueNumberingReducer();

  const char* reducer_name() const override { return "ValueNumberingReducer"; }

  Reduction Reduce(Node* node) override;

 private:
  enum { kInitialCapacity = 256u };

  Reduction ReplaceIfTypesMatch(Node* node, Node* replacement);
  void Grow();
  Zone* temp_zone() const { return temp_zone_; }
  Zone* graph_zone() const { return graph_zone_; }

  Node** entries_;
  size_t capacity_;
  size_t size_;
  Zone* temp_zone_;
  Zone* graph_zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_VALUE_NUMBERING_REDUCER_H_
