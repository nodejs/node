// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

'use strict'
function g() {
  var x = 1;
  try { undefined.x } catch (e) { x = e; }
  (function() { x });
  return x;
}
function f(a) {
  var args = arguments;
  assertInstanceof(g(), TypeError);
  return args.length;
}
assertEquals(1, f(0));
assertEquals(1, f(0));
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f(0));
