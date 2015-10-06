// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals(0, "".length);
assertEquals(1, "a".length);
assertEquals(2, ("a" + "b").length);

function id(x) { return x; }

function f1(x) {
  return x.length;
}
assertEquals(0, f1(""));
assertEquals(1, f1("a"));
%OptimizeFunctionOnNextCall(f1);
assertEquals(2, f1("a" + "b"));
assertEquals(3, f1(id("a") + id("b" + id("c"))))

function f2(x, y, z) {
  x = x ? "" + y : "" + z;
  return x.length;
}
assertEquals(0, f2(true, "", "a"));
assertEquals(1, f2(false, "", "a"));
%OptimizeFunctionOnNextCall(f2);
assertEquals(0, f2(true, "", "a"));
assertEquals(1, f2(false, "", "a"));
assertEquals(3, f2(true, id("a") + id("b" + id("c")), ""));
