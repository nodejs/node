// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-turbofan

// A keyed access with monomorphic name feedback and a constant key that is a
// ThinString of the recorded name must not deopt unconditionally.

function makeAccessor(key) {
  return {
    get(o) { return o[key]; },
    set(o, v) { o[key] = v; },
  };
}

// Turns the (computed, non-internalized) string into a ThinString in place.
const key = %ConstructThinString('__props$' + 'abcdefghij0123456789');
const acc = makeAccessor(key);
const o = {};

%PrepareFunctionForOptimization(acc.set);
acc.set(o, 1);
acc.set(o, 2);
%OptimizeMaglevOnNextCall(acc.set);
acc.set(o, 3);
assertTrue(isMaglevved(acc.set));
assertEquals(3, o[key]);

%PrepareFunctionForOptimization(acc.get);
assertEquals(3, acc.get(o));
assertEquals(3, acc.get(o));
%OptimizeMaglevOnNextCall(acc.get);
assertEquals(3, acc.get(o));
assertTrue(isMaglevved(acc.get));
