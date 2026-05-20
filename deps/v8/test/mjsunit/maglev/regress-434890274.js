// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function empty() {}

function bar(a, b, c) {
  if (empty()) {
    empty(a, b, c);
  }
}

function foo(a) {
  var x = 1;
  bar();
  x + a;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(1);
%OptimizeFunctionOnNextCall(foo);
foo();
