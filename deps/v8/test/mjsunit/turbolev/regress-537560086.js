// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

let obj = {};
let count = 0;
let result;

function foo() {
  var o = [];
  for (var i = 0; i < 2; ++i) {
    result = o.x;
    obj["p" + (count++)] = [];
    result = o.x;
    o = i;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
