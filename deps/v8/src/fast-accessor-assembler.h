// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FAST_ACCESSOR_ASSEMBLER_H_
#define V8_FAST_ACCESSOR_ASSEMBLER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "include/v8-experimental.h"
#include "src/base/macros.h"
#include "src/handles.h"

// For CodeStubAssembler::Label. (We cannot forward-declare inner classes.)
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class Code;
class Isolate;
class Zone;

namespace compiler {
class Node;
}

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

  // Loads internal field and assumes the object is indeed a valid API object
  // with the proper internal fields present.
  // The intended use is to call this on an object whose structure has already
  // been checked previously, e.g. the accessor's receiver, which is map-checked
  // before the fast accessor is called on it. Using this on an arbitrary object
  // will result in unsafe memory accesses.
  ValueId LoadInternalFieldUnchecked(ValueId value_id, int field_no);

  ValueId LoadValue(ValueId value_id, int offset);
  ValueId LoadObject(ValueId value_id, int offset);

  // Converts a machine integer to a SMI.
  ValueId ToSmi(ValueId value_id);

  // Builder / assembler functions for control flow.
  void ReturnValue(ValueId value_id);
  void CheckFlagSetOrReturnNull(ValueId value_id, int mask);
  void CheckNotZeroOrReturnNull(ValueId value_id);
  LabelId MakeLabel();
  void SetLabel(LabelId label_id);
  void Goto(LabelId label_id);
  void CheckNotZeroOrJump(ValueId value_id, LabelId label_id);

  // C++ callback.
  ValueId Call(FunctionCallback callback, ValueId arg);

  // Assemble the code.
  MaybeHandle<Code> Build();

 private:
  ValueId FromRaw(compiler::Node* node);
  LabelId FromRaw(CodeStubAssembler::Label* label);
  compiler::Node* FromId(ValueId value) const;
  CodeStubAssembler::Label* FromId(LabelId value) const;

  void CheckIsJSObjectOrJump(ValueId value, LabelId label_id);

  void Clear();
  Zone* zone() { return &zone_; }
  Isolate* isolate() const { return isolate_; }

  Zone zone_;
  Isolate* isolate_;
  std::unique_ptr<CodeStubAssembler> assembler_;

  // To prevent exposing the RMA internals to the outside world, we'll map
  // Node + Label pointers integers wrapped in ValueId and LabelId instances.
  // These vectors maintain this mapping.
  std::vector<compiler::Node*> nodes_;
  std::vector<CodeStubAssembler::Label*> labels_;

  // Remember the current state for easy error checking. (We prefer to be
  // strict as this class will be exposed at the API.)
  enum { kBuilding, kBuilt, kError } state_;

  DISALLOW_COPY_AND_ASSIGN(FastAccessorAssembler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FAST_ACCESSOR_ASSEMBLER_H_
