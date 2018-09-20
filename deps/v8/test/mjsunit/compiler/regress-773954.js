// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

'use strict';
var a = { x:0 };
var b = {};
Object.defineProperty(b, "x", {get: function () {}});

function f(o) {
  return 5 + o.x++;
}

try {
  f(a);
  f(b);
} catch (e) {}
%OptimizeFunctionOnNextCall(f);
f(a);
