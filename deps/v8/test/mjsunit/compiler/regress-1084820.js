// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Create a map where 'my_property' has HeapObject representation.
const dummy_obj = {};
dummy_obj.my_property = 'some HeapObject';
dummy_obj.my_property = 'some other HeapObject';

function gaga() {
  const obj = {};
  // Store a HeapNumber and then a Smi.
  // This must happen in a loop, even if it's only 2 iterations:
  for (let j = -3_000_000_000; j <= -1_000_000_000; j += 2_000_000_000) {
    obj.my_property = j;
  }
  // Trigger (soft) deopt.
  if (!%IsBeingInterpreted()) obj + obj;
}

%PrepareFunctionForOptimization(gaga);
gaga();
gaga();
%OptimizeFunctionOnNextCall(gaga);
gaga();
