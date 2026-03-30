// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {};
function bar() {
  o[0] = +o[0];
  o = /\u23a1|__v_4/;
}
bar();
bar();
bar();
function foo() {
  bar();
};
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo();
