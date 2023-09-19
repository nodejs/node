// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function foo() {
  String.prototype.startsWith.call(undefined, "");
}
%PrepareFunctionForOptimization(foo);
assertThrows(foo);
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo);

function bar() {
  "bla".startsWith("", Symbol(''));
}
%PrepareFunctionForOptimization(bar);
assertThrows(bar);
%OptimizeFunctionOnNextCall(bar);
assertThrows(bar);
