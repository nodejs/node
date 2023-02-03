// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Test that ObjectIsArrayBufferView lowering works correctly
// in EffectControlLinearizer in the case that the input is
// known to be a HeapObject by TurboFan. For this we use the
// simple trick with an object literal whose field `x` will
// only ever contain HeapObjects and so the representation
// tracking is going to pick it up.
(function() {
  function foo(x) {
    return ArrayBuffer.isView({x}.x);
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(Symbol()));
  assertFalse(foo("some string"));
  assertFalse(foo(new Object()));
  assertFalse(foo(new Array()));
  assertFalse(foo(new ArrayBuffer(1)));
  assertTrue(foo(new Int32Array(1)));
  assertTrue(foo(new DataView(new ArrayBuffer(1))));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(Symbol()));
  assertFalse(foo("some string"));
  assertFalse(foo(new Object()));
  assertFalse(foo(new Array()));
  assertFalse(foo(new ArrayBuffer(1)));
  assertTrue(foo(new Int32Array(1)));
  assertTrue(foo(new DataView(new ArrayBuffer(1))));
  assertOptimized(foo);
})();

// Test that ObjectIsArrayBufferView lowering works correctly
// in EffectControlLinearizer in the case that the input is
// some arbitrary tagged value.
(function() {
  function foo(x) {
    return ArrayBuffer.isView(x);
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo(1));
  assertFalse(foo(1.1));
  assertFalse(foo(Symbol()));
  assertFalse(foo("some string"));
  assertFalse(foo(new Object()));
  assertFalse(foo(new Array()));
  assertFalse(foo(new ArrayBuffer(1)));
  assertTrue(foo(new Int32Array(1)));
  assertTrue(foo(new DataView(new ArrayBuffer(1))));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(1));
  assertFalse(foo(1.1));
  assertFalse(foo(Symbol()));
  assertFalse(foo("some string"));
  assertFalse(foo(new Object()));
  assertFalse(foo(new Array()));
  assertFalse(foo(new ArrayBuffer(1)));
  assertTrue(foo(new Int32Array(1)));
  assertTrue(foo(new DataView(new ArrayBuffer(1))));
  assertOptimized(foo);
})();
