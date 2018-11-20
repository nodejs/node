// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Ensure that we properly check for elements on the receiver.
function foo(o) {
  var s = "";
  for (var i in o) s += i;
  return s;
}

var a = [];
assertEquals("", foo(a));
assertEquals("", foo(a));
%OptimizeFunctionOnNextCall(foo);
assertEquals("", foo(a));
a[0] = 1;
assertEquals("0", foo(a));
