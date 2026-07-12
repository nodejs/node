// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const n = 10;
const a = [undefined];

function foo() {
  return a[0];
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(test);

function test() {
    var c  = 0;
    for (var i = 0; i < n; i++) {
      if(undefined == foo()) c++;
      if(i == 2) %OptimizeOsr();
    }
    return c;
}

assertEquals(n, test());
assertEquals(n, test());
