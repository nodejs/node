// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var g = 0;
g = function() {}

function f() {
  var r = /[abc]/i;  // Optimized out.
  g(r);
}

%PrepareFunctionForOptimization(f);
f(); f(); %OptimizeFunctionOnNextCall(f);  // Warm-up.

var re;
g = function(r) { re = r; }
f();  // Lazy deopt is forced here.

assertNotEquals(undefined, re);
assertEquals("[abc]", re.source);
assertEquals("i", re.flags);
assertEquals(0, re.lastIndex);
assertArrayEquals(["a"], re.exec("a"));
assertArrayEquals(["A"], re.exec("A"));
assertNull(re.exec("d"));
