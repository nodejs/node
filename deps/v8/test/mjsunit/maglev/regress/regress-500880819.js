// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --expose-gc

let f = new Float64Array(1); f[0] = 5;
let HN5 = f[0];
globalThis.G = HN5;

let obj = { smiField: 1 };
obj.smiField = 2;
obj.smiField = 3;

function sh(o, x, c) { if (c) o.smiField = x; }
function corrupt(o, x, c) { G = x; sh(o, x, c); }

%PrepareFunctionForOptimization(sh);
%PrepareFunctionForOptimization(corrupt);
sh(obj, 5, true);
corrupt(obj, HN5, false);
corrupt(obj, HN5, false);
%OptimizeMaglevOnNextCall(sh);
%OptimizeMaglevOnNextCall(corrupt);

// Trigger: HeapNumber(5.0) ends up in a kSmi-typed field without a Smi check.
corrupt(obj, HN5, true);

// Force a write-barrier verification path by allocating.
gc();
assertEquals(5, obj.smiField);
