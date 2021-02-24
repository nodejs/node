// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-dynamic-map-checks --opt --no-always-opt

function bar(obj) {
  // Add two dummy loads to make sure obj.b is in the same slot index as obj.a
  // in foo.
  obj.y;
  obj.x;
  return obj.b
}

function foo(obj) {
  bar(obj);
  return obj.a;
}

var obj = { a: 10, b: 20};
%PrepareFunctionForOptimization(foo);
%EnsureFeedbackVectorForFunction(bar);
foo(obj);
%OptimizeFunctionOnNextCall(foo);
assertEquals(10, foo(obj));
