// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function deopt(f) {
  return { valueOf : function() { %DeoptimizeFunction(f); return 1.1; } };
}

function or_zero(o) {
  return o|0;
}

function multiply_one(o) {
  return +o;
}

function multiply_one_symbol() {
  return +Symbol();
}

assertThrows(multiply_one_symbol, TypeError);
assertEquals(1, or_zero(deopt(or_zero)));
assertEquals(1.1, multiply_one(deopt(multiply_one)));
