// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function MyWrapper(v) {
  return { valueOf: function() { return v } };
}

function f() {
  assertEquals("truex", true + "x");
  assertEquals("truey", true + new String("y"));
  assertEquals("truez", true + new MyWrapper("z"));

  assertEquals("xtrue", "x" + true);
  assertEquals("ytrue", new String("y") + true);
  assertEquals("ztrue", new MyWrapper("z") + true);
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
