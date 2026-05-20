// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function inlined(a, i) {
  return a[i + 1];
}

function foo(index) {
  var a = [0, 1, 2, 3];
  var result = 0;
  result += a[index];
  result += inlined(a, index);
  return result;
};
%PrepareFunctionForOptimization(foo);
foo(0);
foo(0);
%OptimizeFunctionOnNextCall(foo);
foo(0);
