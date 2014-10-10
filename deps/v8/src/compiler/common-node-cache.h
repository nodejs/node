// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_NODE_CACHE_H_
#define V8_COMPILER_COMMON_NODE_CACHE_H_

#include "src/assembler.h"
#include "src/compiler/node-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

// Bundles various caches for common nodes.
class CommonNodeCache FINAL : public ZoneObject {
 public:
  explicit CommonNodeCache(Zone* zone) : zone_(zone) {}

  Node** FindInt32Constant(int32_t value) {
    return int32_constants_.Find(zone_, value);
  }

  Node** FindFloat64Constant(double value) {
    // We canonicalize double constants at the bit representation level.
    return float64_constants_.Find(zone_, bit_cast<int64_t>(value));
  }

  Node** FindExternalConstant(ExternalReference reference) {
    return external_constants_.Find(zone_, reference.address());
  }

  Node** FindNumberConstant(double value) {
    // We canonicalize double constants at the bit representation level.
    return number_constants_.Find(zone_, bit_cast<int64_t>(value));
  }

  Zone* zone() const { return zone_; }

 private:
  Int32NodeCache int32_constants_;
  Int64NodeCache float64_constants_;
  PtrNodeCache external_constants_;
  Int64NodeCache number_constants_;
  Zone* zone_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_COMMON_NODE_CACHE_H_
