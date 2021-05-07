// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboprop --allow-natives-syntax

let last_value;
let throwFunc;

function foo(count) {
  let val = 1;
  for (let i = 16; i < count; ++i) {
    try {
      throwFunc();
    } catch (e) {
    }
    val *= 2;
    last_value = val;
  }
}

%PrepareFunctionForOptimization(foo);
foo(20);
foo(21);
%OptimizeFunctionOnNextCall(foo);
foo(47);
assertEquals(2147483648, last_value);
