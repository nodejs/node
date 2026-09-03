// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing

Int32Array.prototype.my_values = Array.prototype.values;

function foo(ta) {
  const iter = ta.my_values();
  return iter.next();
}

%PrepareFunctionForOptimization(foo);
const ta0 = new Int32Array([1, 2, 3]);
foo(ta0);
foo(ta0);

const ta_det = new Int32Array([10, 20, 30]);
%ArrayBufferDetach(ta_det.buffer);
try { foo(ta_det); } catch (e) {}
try { foo(ta_det); } catch (e) {}

%OptimizeFunctionOnNextCall(foo);
const ta2 = new Int32Array([100, 200, 300]);
%ArrayBufferDetach(ta2.buffer);

// ECMA-262 §23.1.5.2 step 10: if buffer is detached, throw TypeError.
// Without the fix, because length is zeroed in-place to 0 and protector remains intact under --fuzzing,
// optimized iter.next() returns {value: undefined, done: true} instead of throwing TypeError.
assertThrows(() => foo(ta2), TypeError);
