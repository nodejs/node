// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;
class Isolate;

namespace compiler {

// Forward declarations.
class JSGraph;
class MachineOperatorBuilder;
class SimplifiedOperatorBuilder;

class V8_EXPORT_PRIVATE SimplifiedOperatorReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  SimplifiedOperatorReducer(Editor* editor, JSGraph* jsgraph,
                            JSHeapBroker* broker,
                            BranchSemantics branch_semantics);
  ~SimplifiedOperatorReducer() final;
  SimplifiedOperatorReducer(const SimplifiedOperatorReducer&) = delete;
  SimplifiedOperatorReducer& operator=(const SimplifiedOperatorReducer&) =
      delete;

  const char* reducer_name() const override {
    return "SimplifiedOperatorReducer";
  }

  Reduction Reduce(Node* node) final;

 private:
  Reduction Change(Node* node, const Operator* op, Node* a);
  Reduction ReplaceBoolean(bool value);
  Reduction ReplaceFloat64(double value);
  Reduction ReplaceInt32(int32_t value);
  Reduction ReplaceUint32(uint32_t value) {
    return ReplaceInt32(bit_cast<int32_t>(value));
  }
  Reduction ReplaceNumber(double value);
  Reduction ReplaceNumber(int32_t value);

  Factory* factory() const;
  Graph* graph() const;
  MachineOperatorBuilder* machine() const;
  SimplifiedOperatorBuilder* simplified() const;

  JSGraph* jsgraph() const { return jsgraph_; }
  JSHeapBroker* broker() const { return broker_; }

  JSGraph* const jsgraph_;
  JSHeapBroker* const broker_;
  BranchSemantics branch_semantics_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_
