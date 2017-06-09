// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-do-expressions

function f(x) {
  switch (x) {
    case 1: return "one";
    case 2: return "two";
    case do { for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); } }:
    case 3: return "WAT";
  }
}

assertEquals("one", f(1));
assertEquals("two", f(2));
assertEquals("WAT", f(3));
