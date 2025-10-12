// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PAIR_LOAD_STORE_REDUCER_H_
#define V8_COMPILER_PAIR_LOAD_STORE_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class MachineGraph;
class Word32Adapter;
class Word64Adapter;

// Reduces (currently only) store pairs which can be combined on supported
// platforms (currently arm64). Stores are trivially pairable if they are next
// to each other, write to consecutive indices and do not have a write barrier.
// TODO(olivf, v8:13877) Add support for loads, more word sizes, and arm.
class V8_EXPORT_PRIVATE PairLoadStoreReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  PairLoadStoreReducer(Editor* editor, MachineGraph* mcgraph,
                       Isolate* isolate_);

  const char* reducer_name() const override { return "PairLoadStoreReducer"; }

  Reduction Reduce(Node* node) override;

 private:
  MachineGraph* mcgraph_;
  Isolate* isolate_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PAIR_LOAD_STORE_REDUCER_H_
