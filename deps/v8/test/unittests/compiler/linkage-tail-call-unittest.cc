// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

MachineType kMachineTypes[] = {
    MachineType::AnyTagged(), MachineType::AnyTagged(),
    MachineType::AnyTagged(), MachineType::AnyTagged(),
    MachineType::AnyTagged(), MachineType::AnyTagged(),
    MachineType::AnyTagged(), MachineType::AnyTagged()};
}

class LinkageTailCall : public TestWithZone {
 protected:
  CallDescriptor* NewStandardCallDescriptor(LocationSignature* locations) {
    DCHECK(arraysize(kMachineTypes) >=
           locations->return_count() + locations->parameter_count());
    MachineSignature* types = new (zone()) MachineSignature(
        locations->return_count(), locations->parameter_count(), kMachineTypes);
    return new (zone()) CallDescriptor(CallDescriptor::kCallCodeObject,
                                       MachineType::AnyTagged(),
                                       LinkageLocation::ForAnyRegister(),
                                       types,      // machine_sig
                                       locations,  // location_sig
                                       0,          // js_parameter_count
                                       Operator::kNoProperties,  // properties
                                       0,                        // callee-saved
                                       0,  // callee-saved fp
                                       CallDescriptor::kNoFlags,  // flags,
                                       "");
  }

  LinkageLocation StackLocation(int loc) {
    return LinkageLocation::ForCallerFrameSlot(-loc);
  }

  LinkageLocation RegisterLocation(int loc) {
    return LinkageLocation::ForRegister(loc);
  }
};


TEST_F(LinkageTailCall, EmptyToEmpty) {
  LocationSignature locations(0, 0, nullptr);
  CallDescriptor* desc = NewStandardCallDescriptor(&locations);
  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, SameReturn) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0)};
  LocationSignature locations1(1, 0, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Callee
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations1);

  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc2);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, DifferingReturn) {
  // Caller
  LinkageLocation location_array1[] = {RegisterLocation(0)};
  LocationSignature locations1(1, 0, location_array1);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Callee
  LinkageLocation location_array2[] = {RegisterLocation(1)};
  LocationSignature locations2(1, 0, location_array2);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc2);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_FALSE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, MoreRegisterParametersCallee) {
  // Caller
  LinkageLocation location_array1[] = {RegisterLocation(0)};
  LocationSignature locations1(1, 0, location_array1);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Callee
  LinkageLocation location_array2[] = {RegisterLocation(0),
                                       RegisterLocation(0)};
  LocationSignature locations2(1, 1, location_array2);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc2);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, MoreRegisterParametersCaller) {
  // Caller
  LinkageLocation location_array1[] = {RegisterLocation(0),
                                       RegisterLocation(0)};
  LocationSignature locations1(1, 1, location_array1);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Callee
  LinkageLocation location_array2[] = {RegisterLocation(0)};
  LocationSignature locations2(1, 0, location_array2);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc2);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, MoreRegisterAndStackParametersCallee) {
  // Caller
  LinkageLocation location_array1[] = {RegisterLocation(0)};
  LocationSignature locations1(1, 0, location_array1);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Callee
  LinkageLocation location_array2[] = {RegisterLocation(0), RegisterLocation(0),
                                       RegisterLocation(1), StackLocation(1)};
  LocationSignature locations2(1, 3, location_array2);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc2);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(-1, stack_param_delta);
}


TEST_F(LinkageTailCall, MoreRegisterAndStackParametersCaller) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), RegisterLocation(0),
                                      RegisterLocation(1), StackLocation(1)};
  LocationSignature locations1(1, 3, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Callee
  LinkageLocation location_array2[] = {RegisterLocation(0)};
  LocationSignature locations2(1, 0, location_array2);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  const Operator* op = common.Call(desc2);
  Node* const node = Node::New(zone(), 1, op, 0, nullptr, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(1, stack_param_delta);
}


TEST_F(LinkageTailCall, MatchingStackParameters) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), StackLocation(3),
                                      StackLocation(2), StackLocation(1)};
  LocationSignature locations1(1, 3, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Caller
  LocationSignature locations2(1, 3, location_array);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations1);

  CommonOperatorBuilder common(zone());
  Node* p0 = Node::New(zone(), 0, nullptr, 0, nullptr, false);
  Node* p1 = Node::New(zone(), 0, common.Parameter(0), 0, nullptr, false);
  Node* p2 = Node::New(zone(), 0, common.Parameter(1), 0, nullptr, false);
  Node* p3 = Node::New(zone(), 0, common.Parameter(2), 0, nullptr, false);
  Node* parameters[] = {p0, p1, p2, p3};
  const Operator* op = common.Call(desc2);
  Node* const node =
      Node::New(zone(), 1, op, arraysize(parameters), parameters, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, NonMatchingStackParameters) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), StackLocation(3),
                                      StackLocation(2), StackLocation(1)};
  LocationSignature locations1(1, 3, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Caller
  LocationSignature locations2(1, 3, location_array);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations1);

  CommonOperatorBuilder common(zone());
  Node* p0 = Node::New(zone(), 0, nullptr, 0, nullptr, false);
  Node* p1 = Node::New(zone(), 0, common.Parameter(0), 0, nullptr, false);
  Node* p2 = Node::New(zone(), 0, common.Parameter(2), 0, nullptr, false);
  Node* p3 = Node::New(zone(), 0, common.Parameter(1), 0, nullptr, false);
  Node* parameters[] = {p0, p1, p2, p3};
  const Operator* op = common.Call(desc2);
  Node* const node =
      Node::New(zone(), 1, op, arraysize(parameters), parameters, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, MatchingStackParametersExtraCallerRegisters) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), StackLocation(3),
                                      StackLocation(2),    StackLocation(1),
                                      RegisterLocation(0), RegisterLocation(1)};
  LocationSignature locations1(1, 5, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Caller
  LocationSignature locations2(1, 3, location_array);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations1);

  CommonOperatorBuilder common(zone());
  Node* p0 = Node::New(zone(), 0, nullptr, 0, nullptr, false);
  Node* p1 = Node::New(zone(), 0, common.Parameter(0), 0, nullptr, false);
  Node* p2 = Node::New(zone(), 0, common.Parameter(1), 0, nullptr, false);
  Node* p3 = Node::New(zone(), 0, common.Parameter(2), 0, nullptr, false);
  Node* parameters[] = {p0, p1, p2, p3};
  const Operator* op = common.Call(desc2);
  Node* const node =
      Node::New(zone(), 1, op, arraysize(parameters), parameters, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, MatchingStackParametersExtraCalleeRegisters) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), StackLocation(3),
                                      StackLocation(2),    StackLocation(1),
                                      RegisterLocation(0), RegisterLocation(1)};
  LocationSignature locations1(1, 3, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Caller
  LocationSignature locations2(1, 5, location_array);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations1);

  CommonOperatorBuilder common(zone());
  Node* p0 = Node::New(zone(), 0, nullptr, 0, nullptr, false);
  Node* p1 = Node::New(zone(), 0, common.Parameter(0), 0, nullptr, false);
  Node* p2 = Node::New(zone(), 0, common.Parameter(1), 0, nullptr, false);
  Node* p3 = Node::New(zone(), 0, common.Parameter(2), 0, nullptr, false);
  Node* p4 = Node::New(zone(), 0, common.Parameter(3), 0, nullptr, false);
  Node* parameters[] = {p0, p1, p2, p3, p4};
  const Operator* op = common.Call(desc2);
  Node* const node =
      Node::New(zone(), 1, op, arraysize(parameters), parameters, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(0, stack_param_delta);
}


TEST_F(LinkageTailCall, MatchingStackParametersExtraCallerRegistersAndStack) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), StackLocation(3),
                                      StackLocation(2),    StackLocation(1),
                                      RegisterLocation(0), StackLocation(4)};
  LocationSignature locations1(1, 5, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Caller
  LocationSignature locations2(1, 3, location_array);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  Node* p0 = Node::New(zone(), 0, nullptr, 0, nullptr, false);
  Node* p1 = Node::New(zone(), 0, common.Parameter(0), 0, nullptr, false);
  Node* p2 = Node::New(zone(), 0, common.Parameter(1), 0, nullptr, false);
  Node* p3 = Node::New(zone(), 0, common.Parameter(2), 0, nullptr, false);
  Node* p4 = Node::New(zone(), 0, common.Parameter(3), 0, nullptr, false);
  Node* parameters[] = {p0, p1, p2, p3, p4};
  const Operator* op = common.Call(desc2);
  Node* const node =
      Node::New(zone(), 1, op, arraysize(parameters), parameters, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(1, stack_param_delta);
}


TEST_F(LinkageTailCall, MatchingStackParametersExtraCalleeRegistersAndStack) {
  // Caller
  LinkageLocation location_array[] = {RegisterLocation(0), StackLocation(3),
                                      StackLocation(2),    RegisterLocation(0),
                                      RegisterLocation(1), StackLocation(4)};
  LocationSignature locations1(1, 3, location_array);
  CallDescriptor* desc1 = NewStandardCallDescriptor(&locations1);

  // Caller
  LocationSignature locations2(1, 5, location_array);
  CallDescriptor* desc2 = NewStandardCallDescriptor(&locations2);

  CommonOperatorBuilder common(zone());
  Node* p0 = Node::New(zone(), 0, nullptr, 0, nullptr, false);
  Node* p1 = Node::New(zone(), 0, common.Parameter(0), 0, nullptr, false);
  Node* p2 = Node::New(zone(), 0, common.Parameter(1), 0, nullptr, false);
  Node* p3 = Node::New(zone(), 0, common.Parameter(2), 0, nullptr, false);
  Node* p4 = Node::New(zone(), 0, common.Parameter(3), 0, nullptr, false);
  Node* parameters[] = {p0, p1, p2, p3, p4};
  const Operator* op = common.Call(desc2);
  Node* const node =
      Node::New(zone(), 1, op, arraysize(parameters), parameters, false);
  int stack_param_delta = 0;
  EXPECT_TRUE(desc1->CanTailCall(node, &stack_param_delta));
  EXPECT_EQ(-1, stack_param_delta);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
