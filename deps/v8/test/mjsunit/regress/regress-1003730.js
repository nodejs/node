// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --concurrent-inlining

function bar(error) {
  try {
    throw "didn't throw TypeError";
  } catch (err) {
    error instanceof error, "didn't throw " + error.prototype.name;
  }
}
function foo(param) {
  bar(TypeError);
}
try {
  bar();
} catch (e) {}
%PrepareFunctionForOptimization(foo);
try {
  foo();
} catch (e) {}
%OptimizeFunctionOnNextCall(foo);
foo();
