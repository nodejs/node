// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo() {
  const arr = __wrapTC(() => []);
  function bar(arr1) {
    const arr2 = [null,,];
    arr1.forEach(Array.prototype.forEach, arr2);
  }
  __wrapTC(() => bar(arr));
}

%PrepareFunctionForOptimization(foo);
foo.apply();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
