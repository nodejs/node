// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f(r) {
  let pos;
  while ('a'.startsWith('b', pos)) { do { pos = 1; } while (pos); }
  r.push(pos);
}

%PrepareFunctionForOptimization(f);
let a = [{}];
assertEquals(undefined, f(a));
assertEquals([{}, undefined], a);
let b = [1];
assertEquals(undefined, f(b));
assertEquals([1, undefined], b);

%OptimizeMaglevOnNextCall(f);
let c = [1];
assertEquals(undefined, f(c));
assertEquals([1, undefined], c);
