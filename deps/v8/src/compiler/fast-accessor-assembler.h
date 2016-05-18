// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FAST_ACCESSOR_ASSEMBLER_H_
#define V8_COMPILER_FAST_ACCESSOR_ASSEMBLER_H_

#include <stdint.h>
#include <vector>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "include/v8-experimental.h"
#include "src/base/macros.h"
#include "src/base/smart-pointers.h"
#include "src/handles.h"


namespace v8 {
namespace internal {

class Code;
class Isolate;
class Zone;

namespace compiler {

class Node;
class RawMachineAssembler;
class RawMachineLabel;


// This interface "exports" an aggregated subset of RawMachineAssembler, for
// use by the API to implement Fast Dom Accessors.
//
// This interface is made for this single purpose only and does not attempt
// to implement a general purpose solution. If you need one, please look at
// RawMachineAssembler instead.
//
// The life cycle of a FastAccessorAssembler has two phases:
// - After creating the instance, you can call an arbitrary sequence of
//   builder functions to build the desired function.
// - When done, you can Build() the accessor and query for the build results.
//
// You cannot call any result getters before Build() was called & successful;
// and you cannot call any builder functions after Build() was called.
class FastAccessorAssembler {
 public:
  typedef v8::experimental::FastAccessorBuilder::ValueId ValueId;
  typedef v8::experimental::FastAccessorBuilder::LabelId LabelId;
  typedef v8::FunctionCallback FunctionCallback;

  explicit FastAccessorAssembler(Isolate* isolate);
  ~FastAccessorAssembler();

  // Builder / assembler functions:
  ValueId IntegerConstant(int int_constant);
  ValueId GetReceiver();
  ValueId LoadInternalField(ValueId value_id, int field_no);
  ValueId LoadValue(ValueId value_id, int offset);
  ValueId LoadObject(ValueId value_id, int offset);

  // Builder / assembler functions for control flow.
  void ReturnValue(ValueId value_id);
  void CheckFlagSetOrReturnNull(ValueId value_id, int mask);
  void CheckNotZeroOrReturnNull(ValueId value_id);
  LabelId MakeLabel();
  void SetLabel(LabelId label_id);
  void CheckNotZeroOrJump(ValueId value_id, LabelId label_id);

  // C++ callback.
  ValueId Call(FunctionCallback callback, ValueId arg);

  // Assemble the code.
  MaybeHandle<Code> Build();

 private:
  ValueId FromRaw(Node* node);
  LabelId FromRaw(RawMachineLabel* label);
  Node* FromId(ValueId value) const;
  RawMachineLabel* FromId(LabelId value) const;

  Zone* zone() { return &zone_; }

  Zone zone_;
  base::SmartPointer<RawMachineAssembler> assembler_;

  // To prevent exposing the RMA internals to the outside world, we'll map
  // Node + Label pointers integers wrapped in ValueId and LabelId instances.
  // These vectors maintain this mapping.
  std::vector<Node*> nodes_;
  std::vector<RawMachineLabel*> labels_;

  // Remember the current state for easy error checking. (We prefer to be
  // strict as this class will be exposed at the API.)
  enum { kBuilding, kBuilt, kError } state_;

  DISALLOW_COPY_AND_ASSIGN(FastAccessorAssembler);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FAST_ACCESSOR_ASSEMBLER_H_
