// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo --turbo-escape

// Warm up {g} with arrays and strings.
function g(v) { return v.length; }
assertEquals(1, g("x"));
assertEquals(2, g("xy"));
assertEquals(1, g([1]));
assertEquals(2, g([1,2]));

// Inline into {f}, where we see only an array.
function f() { assertEquals(0, g([])); }
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
