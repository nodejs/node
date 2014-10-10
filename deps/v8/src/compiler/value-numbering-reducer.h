// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_VALUE_NUMBERING_REDUCER_H_
#define V8_COMPILER_VALUE_NUMBERING_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class ValueNumberingReducer FINAL : public Reducer {
 public:
  explicit ValueNumberingReducer(Zone* zone);
  ~ValueNumberingReducer();

  virtual Reduction Reduce(Node* node) OVERRIDE;

 private:
  Zone* zone() const { return zone_; }

  // TODO(turbofan): We currently use separate chaining with linked lists here,
  // we may want to replace that with a more sophisticated data structure at
  // some point in the future.
  class Entry;
  Entry* buckets_[117u];
  Zone* zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_VALUE_NUMBERING_REDUCER_H_
