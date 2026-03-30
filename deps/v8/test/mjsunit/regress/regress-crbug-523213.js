// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v1 = [];
var v2 = [];
v1.__proto__ = v2;

function f() {
  var a = [];
  for (var i = 0; i < 2; i++) {
    a.push([]);
    a = v2;
  }
};
%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
