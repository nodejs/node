// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

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
  SimplifiedOperatorReducer(Editor* editor, JSGraph* jsgraph);
  ~SimplifiedOperatorReducer() final;

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceReferenceEqual(Node* node);

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
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  MachineOperatorBuilder* machine() const;
  SimplifiedOperatorBuilder* simplified() const;

  JSGraph* const jsgraph_;

  DISALLOW_COPY_AND_ASSIGN(SimplifiedOperatorReducer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_OPERATOR_REDUCER_H_
