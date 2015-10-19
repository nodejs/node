// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPE_FEEDBACK_LOWERING_H_
#define V8_COMPILER_JS_TYPE_FEEDBACK_LOWERING_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class MachineOperatorBuilder;


// Lowers JS-level operators to simplified operators based on type feedback.
class JSTypeFeedbackLowering final : public AdvancedReducer {
 public:
  // Various configuration flags to control the operation of this lowering.
  enum Flag {
    kNoFlags = 0,
    kDeoptimizationEnabled = 1 << 0,
  };
  typedef base::Flags<Flag> Flags;

  JSTypeFeedbackLowering(Editor* editor, Flags flags, JSGraph* jsgraph);
  ~JSTypeFeedbackLowering() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSLoadNamed(Node* node);

  Factory* factory() const;
  Flags flags() const { return flags_; }
  Graph* graph() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  Flags const flags_;
  JSGraph* const jsgraph_;
  SimplifiedOperatorBuilder simplified_;

  DISALLOW_COPY_AND_ASSIGN(JSTypeFeedbackLowering);
};

DEFINE_OPERATORS_FOR_FLAGS(JSTypeFeedbackLowering::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_TYPE_FEEDBACK_LOWERING_H_
