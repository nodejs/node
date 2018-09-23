// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Ensure that we properly check for elements on the prototypes.
function foo(o) {
  var s = "";
  for (var i in o) s += i;
  return s;
}

var o = {};
assertEquals("", foo(o));
assertEquals("", foo(o));
%OptimizeFunctionOnNextCall(foo);
assertEquals("", foo(o));
Object.prototype[0] = 1;
assertEquals("0", foo(o));
