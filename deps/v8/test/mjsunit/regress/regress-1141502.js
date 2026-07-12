// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --turbo-dynamic-map-checks

function bar(a) {
  return a.x;
}

function foo(a) {
  return 1 * bar(a);
}

var obj = {x: 2};

%PrepareFunctionForOptimization(foo);
foo(obj, obj);
%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo());
