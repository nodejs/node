// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var a = [,];
  a[0] = {};
  a[0].toString = FAIL;
};
%PrepareFunctionForOptimization(foo);
try {
  foo();
} catch (e) {
}
try {
  foo();
} catch (e) {
}
%OptimizeFunctionOnNextCall(foo);
try {
  foo();
} catch (e) {
}
