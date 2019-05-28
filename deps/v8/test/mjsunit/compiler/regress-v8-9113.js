// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let dummy = { x : 0.1 };

let o = { x : 0 };

function f(o, v) {
  o.x = v;
}

f(o, 0);
f(o, 0);
assertEquals(Infinity, 1 / o.x);
%OptimizeFunctionOnNextCall(f);
f(o, -0);
assertEquals(-Infinity, 1 / o.x);
