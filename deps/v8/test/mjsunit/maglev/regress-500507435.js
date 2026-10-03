// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev
let arr = new Uint8ClampedArray(4);

function store(a, v) { a[0] = v; return a[0]; }

%PrepareFunctionForOptimization(store);

for (let i = 0; i < 100; i++) store(arr, 42.7);

%OptimizeMaglevOnNextCall(store);

assertEquals(100, store(arr, 100.5));
assertEquals(128, store(arr, 128.0));
assertEquals(0,   store(arr, 0.4));
assertEquals(0,   store(arr, 0.5));
assertEquals(2,   store(arr, 1.5));
assertEquals(2,   store(arr, 2.5));
assertEquals(1,   store(arr, 1.0));
assertEquals(128, store(arr, 128.0));
assertEquals(43,  store(arr, 42.7));
assertEquals(100, store(arr, 100.5));
assertEquals(255, store(arr, 255.0));
assertEquals(0,   store(arr, -0.3));
assertEquals(0,   store(arr, NaN));

assertTrue(isMaglevved(store));
