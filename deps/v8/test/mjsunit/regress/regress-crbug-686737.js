// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Object.prototype.__defineGetter__(0, () => {
  throw Error();
});
var a = [, 0.1];
function foo(i) {
  a[i];
};
%PrepareFunctionForOptimization(foo);
foo(1);
foo(1);
%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo(0), Error);
