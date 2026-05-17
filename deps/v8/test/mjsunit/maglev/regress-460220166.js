// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function bar(val) {
  val+val+val+val+val+val+val+val+val; // blocking eager inlining
  return val;
}

function foo(x) {
  let v = false;
  if (x) {
    let r = bar(true);
    let b = !!r; // Inserting a ToBoolean, so that Maglev know that {b} is a
                 // boolean.
    return !b;
  }  else {
    let r = bar(false);
    let b = !!r; // Inserting a ToBoolean, so that Maglev know that {b} is a
                 // boolean.
    return !b;
  }
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
assertEquals(false, foo(true));
assertEquals(true, foo(false));

%OptimizeMaglevOnNextCall(foo);
assertEquals(false, foo(true));
assertEquals(true, foo(false));
