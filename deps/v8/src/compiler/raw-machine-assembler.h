// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_RAW_MACHINE_ASSEMBLER_H_
#define V8_COMPILER_RAW_MACHINE_ASSEMBLER_H_

#include "src/v8.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-builder.h"
#include "src/compiler/machine-node-factory.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"


namespace v8 {
namespace internal {
namespace compiler {

class BasicBlock;
class Schedule;


class RawMachineAssembler : public GraphBuilder,
                            public MachineNodeFactory<RawMachineAssembler> {
 public:
  class Label {
   public:
    Label() : block_(NULL), used_(false), bound_(false) {}
    ~Label() { DCHECK(bound_ || !used_); }

    BasicBlock* block() { return block_; }

   private:
    // Private constructor for exit label.
    explicit Label(BasicBlock* block)
        : block_(block), used_(false), bound_(false) {}

    BasicBlock* block_;
    bool used_;
    bool bound_;
    friend class RawMachineAssembler;
    DISALLOW_COPY_AND_ASSIGN(Label);
  };

  RawMachineAssembler(Graph* graph,
                      MachineCallDescriptorBuilder* call_descriptor_builder,
                      MachineType word = MachineOperatorBuilder::pointer_rep());
  virtual ~RawMachineAssembler() {}

  Isolate* isolate() const { return zone()->isolate(); }
  Zone* zone() const { return graph()->zone(); }
  MachineOperatorBuilder* machine() { return &machine_; }
  CommonOperatorBuilder* common() { return &common_; }
  CallDescriptor* call_descriptor() const {
    return call_descriptor_builder_->BuildCallDescriptor(zone());
  }
  int parameter_count() const {
    return call_descriptor_builder_->parameter_count();
  }
  const MachineType* parameter_types() const {
    return call_descriptor_builder_->parameter_types();
  }

  // Parameters.
  Node* Parameter(int index);

  // Control flow.
  Label* Exit();
  void Goto(Label* label);
  void Branch(Node* condition, Label* true_val, Label* false_val);
  // Call to a JS function with zero parameters.
  Node* CallJS0(Node* function, Node* receiver, Label* continuation,
                Label* deoptimization);
  // Call to a runtime function with zero parameters.
  Node* CallRuntime1(Runtime::FunctionId function, Node* arg0,
                     Label* continuation, Label* deoptimization);
  void Return(Node* value);
  void Bind(Label* label);
  void Deoptimize(Node* state);

  // Variables.
  Node* Phi(Node* n1, Node* n2) { return NewNode(common()->Phi(2), n1, n2); }
  Node* Phi(Node* n1, Node* n2, Node* n3) {
    return NewNode(common()->Phi(3), n1, n2, n3);
  }
  Node* Phi(Node* n1, Node* n2, Node* n3, Node* n4) {
    return NewNode(common()->Phi(4), n1, n2, n3, n4);
  }

  // MachineAssembler is invalid after export.
  Schedule* Export();

 protected:
  virtual Node* MakeNode(Operator* op, int input_count, Node** inputs);

  Schedule* schedule() {
    DCHECK(ScheduleValid());
    return schedule_;
  }

 private:
  bool ScheduleValid() { return schedule_ != NULL; }

  BasicBlock* Use(Label* label);
  BasicBlock* EnsureBlock(Label* label);
  BasicBlock* CurrentBlock();

  typedef std::vector<MachineType, zone_allocator<MachineType> >
      RepresentationVector;

  Schedule* schedule_;
  MachineOperatorBuilder machine_;
  CommonOperatorBuilder common_;
  MachineCallDescriptorBuilder* call_descriptor_builder_;
  Node** parameters_;
  Label exit_label_;
  BasicBlock* current_block_;

  DISALLOW_COPY_AND_ASSIGN(RawMachineAssembler);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_RAW_MACHINE_ASSEMBLER_H_
