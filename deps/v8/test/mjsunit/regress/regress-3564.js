// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function MyWrapper(v) {
  return { valueOf: function() { return v } };
}

function f() {
  assertTrue("a" < "x");
  assertTrue("a" < new String("y"));
  assertTrue("a" < new MyWrapper("z"));

  assertFalse("a" > "x");
  assertFalse("a" > new String("y"));
  assertFalse("a" > new MyWrapper("z"));
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
