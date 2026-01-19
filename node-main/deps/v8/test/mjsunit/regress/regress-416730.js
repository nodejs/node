// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var d = {x: undefined, y: undefined};

function Crash(left, right) {
  var c = {x: right.x - left.x, y: right.y - left.y};

  return c.x * c.y;
};
%PrepareFunctionForOptimization(Crash);
var a = {x: 0.5, y: 0};
var b = {x: 1, y: 0};

for (var i = 0; i < 3; i++) Crash(a, b);
%OptimizeFunctionOnNextCall(Crash);
Crash(a, b);

Crash({x: 0, y: 0.5}, b);
