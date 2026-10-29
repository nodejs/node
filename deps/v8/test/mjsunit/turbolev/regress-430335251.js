// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing --turbolev-future

function get(value = 4) {
  switch (typeof value) {
    case "number":
      return value;
  }
}

function bar (value = false) {
  return get(value);
}

function foo(a) {
  var x = 0;
  var y = 1;
  x = y + a;
  assertEquals(2, bar(x));
}

%PrepareFunctionForOptimization(foo);
foo(1);
%OptimizeFunctionOnNextCall(foo);
foo(1);
