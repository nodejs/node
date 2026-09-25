// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function prettyPrint(str) {
  print(str);
}

function foo(f) {
  try {
    __getRandomProperty(); // Throws
  } catch (e) {
    prettyPrint();
  }

  try {
    f();
    f();
  } catch (e) {
  }
  prettyPrint(prettyPrint());
}

function bar() {
  foo();
}

foo(bar);

let arr = [0.5];

function test(undf) {
  undf + 0.5;
  arr[0] = undf;
  prettyPrint(arr);
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeMaglevOnNextCall(test);
test();
