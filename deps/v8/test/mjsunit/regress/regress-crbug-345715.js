// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

a = {y:1.5};
a.y = 0;
b = a.y;
c = {y:{}};

function f() {
  return 1;
}

function g() {
  var e = {y: b};
  var d = {x:f()};
  var d = {x:f()};
  return [e, d];
}

g();
g();
%OptimizeFunctionOnNextCall(g);
assertEquals(1, g()[1].x);
