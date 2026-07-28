// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
// Flags: --max-maglev-inlined-bytecode-size-small=0
// Flags: --maglev-non-eager-inlining

// --max-maglev-inlined-bytecode-size-small=0 prevents these from getting eagerly inlined.
function getDouble() {
  return 5.5;
}
%PrepareFunctionForOptimization(getDouble);

let o = {valueOf: () => { return 3.1; }};

function getObject() {
  return o;
}
%PrepareFunctionForOptimization(getObject);

function test(f, optimizedAtEnd = true) {
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  f();
  assertEquals(optimizedAtEnd, isMaglevved(f));
}

function testDouble() {
  assertEquals(5.5, Math.min(getDouble(), getDouble()));
}

test(testDouble);

function testObject() {
  assertEquals(3.1, Math.min(getObject(), getObject()));
}

test(testObject, false);
