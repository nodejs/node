// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

// ES #sec-isfinite-number
TF_BUILTIN(GlobalIsFinite, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);

  Label return_true(this), return_false(this);

  // We might need to loop once for ToNumber conversion.
  VARIABLE(var_num, MachineRepresentation::kTagged);
  Label loop(this, &var_num);
  var_num.Bind(Parameter(Descriptor::kNumber));
  Goto(&loop);
  BIND(&loop);
  {
    Node* num = var_num.value();

    // Check if {num} is a Smi or a HeapObject.
    GotoIf(TaggedIsSmi(num), &return_true);

    // Check if {num} is a HeapNumber.
    Label if_numisheapnumber(this),
        if_numisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(num), &if_numisheapnumber, &if_numisnotheapnumber);

    BIND(&if_numisheapnumber);
    {
      // Check if {num} contains a finite, non-NaN value.
      Node* num_value = LoadHeapNumberValue(num);
      BranchIfFloat64IsNaN(Float64Sub(num_value, num_value), &return_false,
                           &return_true);
    }

    BIND(&if_numisnotheapnumber);
    {
      // Need to convert {num} to a Number first.
      var_num.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, num));
      Goto(&loop);
    }
  }

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// ES6 #sec-isnan-number
TF_BUILTIN(GlobalIsNaN, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);

  Label return_true(this), return_false(this);

  // We might need to loop once for ToNumber conversion.
  VARIABLE(var_num, MachineRepresentation::kTagged);
  Label loop(this, &var_num);
  var_num.Bind(Parameter(Descriptor::kNumber));
  Goto(&loop);
  BIND(&loop);
  {
    Node* num = var_num.value();

    // Check if {num} is a Smi or a HeapObject.
    GotoIf(TaggedIsSmi(num), &return_false);

    // Check if {num} is a HeapNumber.
    Label if_numisheapnumber(this),
        if_numisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(num), &if_numisheapnumber, &if_numisnotheapnumber);

    BIND(&if_numisheapnumber);
    {
      // Check if {num} contains a NaN.
      Node* num_value = LoadHeapNumberValue(num);
      BranchIfFloat64IsNaN(num_value, &return_true, &return_false);
    }

    BIND(&if_numisnotheapnumber);
    {
      // Need to convert {num} to a Number first.
      var_num.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, num));
      Goto(&loop);
    }
  }

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

}  // namespace internal
}  // namespace v8
