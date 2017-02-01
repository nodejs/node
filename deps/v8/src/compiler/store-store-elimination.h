// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_STORE_STORE_ELIMINATION_H_
#define V8_COMPILER_STORE_STORE_ELIMINATION_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class StoreStoreElimination final {
 public:
  static void Run(JSGraph* js_graph, Zone* temp_zone);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_STORE_STORE_ELIMINATION_H_
