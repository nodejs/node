// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a, b) {
  a == b;
}

f({}, {});

var a = { y: 1.5 };
a.y = 777;
var b = a.y;

function h() {
  var d = 1;
  var e = 777;
  while (d-- > 0) e++;
  f(1, e);
}

var global;
function g() {
  global = b;
  return h(b);
}

g();
g();
%OptimizeFunctionOnNextCall(g);
g();
