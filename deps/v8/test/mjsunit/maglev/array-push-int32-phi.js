// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --verify-heap

function foo(arr, c) {
  let phi = c ? 0x7ffffff1 : 42;
  // Int32 use to enable untagging.
  phi + 2;

  // Polymorphic ELEMENTS/SMI_ELEMENTS push. The storing code will be shared,
  // but the Smi case should first do a Smi check on {phi}, which should fail
  // when {c} is `true` since 0x7ffffff1 is out of Smi range.
  arr.push(phi);
}

let smi_arr = [ 1, 2, 3, 4 ];
let obj_arr = [ "a", 1, 42.5 ];

%PrepareFunctionForOptimization(foo);
foo(smi_arr, false);
foo(obj_arr, false);
foo(smi_arr, false);
foo(obj_arr, false);

%OptimizeMaglevOnNextCall(foo);
foo(smi_arr, false);
foo(obj_arr, false);

foo(smi_arr, true);

assertEquals([1, 2, 3, 4, 42, 42, 42, 0x7ffffff1], smi_arr);
