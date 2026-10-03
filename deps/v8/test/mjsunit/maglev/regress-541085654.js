// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining

class A extends Array {
  constructor() {
    super();
    this.slot = 0;
  }
}

class Holder {
  constructor(v) {
    this.s = v;
  }
}

function inlinee(x) {
  x += 1; x ^= 3; x += 5; x ^= 7; x += 9;
  x ^= 11; x += 13; x ^= 15; x += 17; x ^= 19;
  return x;
}

let sink = 0;

function f(flag, a, holder) {
  sink = inlinee(1);
  const seed = a.slot;
  const r = flag ? a : holder.s;
  const marker = r.missing_property;
  const is_array = Array.isArray(r);
  return marker === undefined ? is_array : seed;
}

const a = new A();
const holder = new Holder(1);

%PrepareFunctionForOptimization(f);
for (let i = 0; i < 1000; ++i) {
  holder.s = (i & 7) + 1;
  f((i & 1) === 0, a, holder);
}

%OptimizeMaglevOnNextCall(f);
assertTrue(f(true, a, holder));
assertFalse(f(false, a, holder));
