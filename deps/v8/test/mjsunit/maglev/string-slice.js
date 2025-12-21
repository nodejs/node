// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(s) {
  return s.slice(-1);
}
%PrepareFunctionForOptimization(foo);

foo("lol");

%OptimizeMaglevOnNextCall(foo);

assertEquals("d", foo("hello world"));
assertEquals("", foo(""));

assertTrue(isMaglevved(foo));

// Passing something else than a string deopts.
assertEquals("x", foo({slice: function() { return "x";}}));
assertFalse(isMaglevved(foo));
