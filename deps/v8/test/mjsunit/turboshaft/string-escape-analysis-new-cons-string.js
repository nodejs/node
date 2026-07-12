// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-string-concat-escape-analysis
// Flags: --turbofan

function foo(s, c1, c2) {
  c1 += 0; // just so that `if (c1)` below doesn't do a ToBoolean
  c2 += 0; // just so that `if (c2)` below doesn't do a ToBoolean

  let str = "More than 13 chars " + s; // Creates NewConsString.
  if (c1) {
    return str + " no feedback here.";
  }
  let str2 = str + "def";
  if (c2) {
    return str2 + " no feedback as well.";
  }
  return str2.length;
}

%PrepareFunctionForOptimization(foo);
assertEquals(25, foo("abc", 0, 0));
assertEquals(25, foo("abc", 0, 0));

%OptimizeFunctionOnNextCall(foo);
assertEquals(25, foo("abc", 0, 0));
assertOptimized(foo);

assertEquals("More than 13 chars abc no feedback here.", foo("abc", 1, 0));
assertUnoptimized(foo);

%OptimizeFunctionOnNextCall(foo);
assertEquals(25, foo("abc", 0, 0));
assertEquals("More than 13 chars abc no feedback here.", foo("abc", 1, 0));
assertOptimized(foo);

assertEquals("More than 13 chars abcdef no feedback as well.", foo("abc", 0, 1));
assertUnoptimized(foo);

%OptimizeFunctionOnNextCall(foo);
assertEquals(25, foo("abc", 0, 0));
assertEquals("More than 13 chars abc no feedback here.", foo("abc", 1, 0));
assertEquals("More than 13 chars abcdef no feedback as well.", foo("abc", 0, 1));
assertOptimized(foo);
