// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let hf_arr = [1.1, /*hole*/ , 2.2];

function foo(c) {
  let v1 = hf_arr[1];
  let v = c ? v1 : 3.35;
  hf_arr[2] = v;
  return !!v;
}

%PrepareFunctionForOptimization(foo);
assertEquals(false, foo(true));
assertEquals(true, foo(false));

%OptimizeMaglevOnNextCall(foo);
assertEquals(false, foo(true));
assertEquals(true, foo(false));

hf_arr[1] = 2.47; // not hole anymore, should be truthy
assertEquals(true, foo(true));
