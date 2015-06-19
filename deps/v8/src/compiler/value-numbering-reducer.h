// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_VALUE_NUMBERING_REDUCER_H_
#define V8_COMPILER_VALUE_NUMBERING_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class ValueNumberingReducer final : public Reducer {
 public:
  explicit ValueNumberingReducer(Zone* zone);
  ~ValueNumberingReducer();

  Reduction Reduce(Node* node) override;

 private:
  enum { kInitialCapacity = 256u, kCapacityToSizeRatio = 2u };

  void Grow();
  Zone* zone() const { return zone_; }

  Node** entries_;
  size_t capacity_;
  size_t size_;
  Zone* zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_VALUE_NUMBERING_REDUCER_H_
