// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(x, y) {
  return x === y;
}

function foo(x) {
  bar("0", x);
}

foo("0");
foo("0");
%BaselineFunctionOnNextCall(bar);
foo("0");
foo("0");
bar(1, 1);
%OptimizeFunctionOnNextCall(foo);
foo("0");
assertOptimized(foo);
