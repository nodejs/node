// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

var t = 0;
function foo() {
  var o = {x: 1};
  var p = {y: 2.5, x: 0};
  o = [];
  for (var i = 0; i < 2; ++i) {
    t = o.x;
    o = p;
  }
};
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
