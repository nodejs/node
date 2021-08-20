// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --stack-size=100

function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  }
  return t();
}

function baz(f) {
  return [[f(1)], 1, 2, 3, 4, 5, 6, 7, 8];
}

function foo(__v_3) {
  try {
    var arr = baz(__v_3);
  } catch (e) {}
  try {
    for (var i = 0; i < arr.length; i++) {
      function bar() {
        return arr[i];
      }
      try {
        throw e;
      } catch (e) {}
    }
  } catch (e) {}
}

%PrepareFunctionForOptimization(foo);
foo(a => a);
foo(a => a);
%OptimizeFunctionOnNextCall(foo);
runNearStackLimit(() => { foo(a => a); });
