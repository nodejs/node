// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Pre-initialize the transition map and EnumCache on Number wrappers so the
// first call to each test function below records fast enum-cache feedback.
{
  const warm = Object(0);
  warm.own = 42;
  for (const k in warm) {
  }
}

function readField(x) {
  const n = Number(x);
  const w = Object(n);
  w.own = 42;
  for (const k in w) return n[k];
}

%PrepareFunctionForOptimization(readField);
assertEquals(undefined, readField(7));
assertEquals(undefined, readField(1.1));
%OptimizeFunctionOnNextCall(readField);
assertEquals(undefined, readField(7));
assertEquals(undefined, readField(1.1));
assertOptimized(readField);

function readFieldCold(x, load) {
  const n = Number(x);
  const w = Object(n);
  w.own = 42;
  for (const k in w) {
    if (load) return n[k];
  }
}

%PrepareFunctionForOptimization(readFieldCold);
assertEquals(undefined, readFieldCold(7, false));
assertEquals(undefined, readFieldCold(1.1, false));
%OptimizeFunctionOnNextCall(readFieldCold);
assertEquals(undefined, readFieldCold(7, true));
%OptimizeFunctionOnNextCall(readFieldCold);
assertEquals(undefined, readFieldCold(1.1, true));

function hasField(x) {
  const n = Number(x);
  const w = Object(n);
  w.own = 42;
  for (const k in w) {
    return Object.prototype.hasOwnProperty.call(n, k);
  }
}

%PrepareFunctionForOptimization(hasField);
assertFalse(hasField(7));
assertFalse(hasField(1.1));
%OptimizeFunctionOnNextCall(hasField);
assertFalse(hasField(7));
assertFalse(hasField(1.1));
assertOptimized(hasField);

function readWrapper(x) {
  const n = Number(x);
  const w = Object(n);
  w.own = 42;
  for (const k in w) {
    if (Object.prototype.hasOwnProperty.call(w, k)) return w[k];
  }
}

%PrepareFunctionForOptimization(readWrapper);
assertEquals(42, readWrapper(7));
assertEquals(42, readWrapper(1.1));
%OptimizeFunctionOnNextCall(readWrapper);
assertEquals(42, readWrapper(7));
assertEquals(42, readWrapper(1.1));
assertOptimized(readWrapper);
