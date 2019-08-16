// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a) {
  a[0] = 0;
  a[1] = 0;
}

var a = new Int32Array(2);
bar([1, 2, 3]);
function foo() {
  bar([1, 2, 3]);
  bar(a);
};
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo();
