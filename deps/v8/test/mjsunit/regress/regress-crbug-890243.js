// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// We need a SpeculativeNumberAdd with Number feedback.
function bar(x) { return x + x; }
bar(0.1);

// We also need an indirection via an object field such
// that only after escape analysis TurboFan can figure
// out that the value `y` is actually a Number in the
// safe integer range.
function baz(y) { return {y}; }
baz(null); baz(0);

// Now we can put all of that together to get a kRepBit
// use of a kWord64 value (on 64-bit architectures).
function foo(o) {
  return !baz(bar(o.x)).y;
}

assertFalse(foo({x:1}));
assertFalse(foo({x:1}));
%OptimizeFunctionOnNextCall(foo);
assertFalse(foo({x:1}));
