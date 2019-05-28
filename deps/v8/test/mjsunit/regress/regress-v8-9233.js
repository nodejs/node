// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let o1 = { x: 999 };
o1.y = 999;

// o2 will share map with o1 in its initial state
var o2 = { x: 1 };

function f() { return o2.x; }

assertEquals(1, f());
assertEquals(1, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f());

delete o2.x;
o2.x = 2;
assertEquals(2, f());
