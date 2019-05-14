// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var dummy = {foo: "true"};

var a = {y:0.5};
a.y = 357;
var b = a.y;

var d;
function f(  )  {
  d = 357;
  return {foo: b};
}
f();
f();
%OptimizeFunctionOnNextCall(f);
var x = f();

// With the bug, x is now an invalid object; the code below
// triggers a crash.

function g(obj) {
  return obj.foo.length;
}

g(dummy);
g(dummy);
%OptimizeFunctionOnNextCall(g);
g(x);
