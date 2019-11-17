// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_STORE_STORE_ELIMINATION_H_
#define V8_COMPILER_STORE_STORE_ELIMINATION_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

class TickCounter;
class Zone;

namespace compiler {

class JSGraph;

// Store-store elimination.
//
// The aim of this optimization is to detect the following pattern in the
// effect graph:
//
// - StoreField[+24, kRepTagged](263, ...)
//
//   ... lots of nodes from which the field at offset 24 of the object
//       returned by node #263 cannot be observed ...
//
// - StoreField[+24, kRepTagged](263, ...)
//
// In such situations, the earlier StoreField cannot be observed, and can be
// eliminated. This optimization should work for any offset and input node, of
// course.
//
// The optimization also works across splits. It currently does not work for
// loops, because we tend to put a stack check in loops, and like deopts,
// stack checks can observe anything.

// Assumption: every byte of a JS object is only ever accessed through one
// offset. For instance, byte 15 of a given object may be accessed using a
// two-byte read at offset 14, or a four-byte read at offset 12, but never
// both in the same program.
//
// This implementation needs all dead nodes removed from the graph, and the
// graph should be trimmed.
class StoreStoreElimination final : public AllStatic {
 public:
  static void Run(JSGraph* js_graph, TickCounter* tick_counter,
                  Zone* temp_zone);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_STORE_STORE_ELIMINATION_H_
