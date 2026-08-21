// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

// Pseudo-side-effecting function.
function bar() {}
%NeverOptimizeFunction(bar);

function foo(i) {
  // First load checks for HeapNumber map, allowing through Smis.
  i['oh'];
  // Cause side-effects to clear known maps of i.
  bar(i);
  // Second load should not crash for Smis.
  i['no'];
}

%PrepareFunctionForOptimization(foo);
// Give the two loads polymorphic feedback in HeapNumber and {some object}.
foo({});
foo(1);
%OptimizeMaglevOnNextCall(foo);
// Pass a Smi to loads with a HeapNumber map-check.
foo(2);
