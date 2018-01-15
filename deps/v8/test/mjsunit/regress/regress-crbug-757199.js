/// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var obj1 = {};
var obj2 = {};

function h() {}

h.prototype = obj2;

function g(v) {
  v.constructor;
}
function f() {
  g(obj1);
}

obj1.x = 0;
f();

obj1.__defineGetter__("x", function() {});

g(obj2);

obj2.y = 0;

%OptimizeFunctionOnNextCall(f);
f();
