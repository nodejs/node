// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const buffer = new ArrayBuffer(64);
const dv = new DataView(buffer);

// `store` is lazily inlined (eager-inline bytecode size is 0), so the
// DataView.prototype.setFloat64 call is reduced by the optimizer-side reducer
// post-inline rather than at build time.
function store(dv, offset, value, little_endian) {
  dv.setFloat64(offset, value, little_endian);
}

function foo(offset, value, little_endian) {
  store(dv, offset, value, little_endian);
}

%PrepareFunctionForOptimization(store);
%PrepareFunctionForOptimization(foo);

foo(8, 12.2536, false);
assertEquals(12.2536, dv.getFloat64(8));

%OptimizeFunctionOnNextCall(foo);
foo(16, -0.5, true);
assertEquals(-0.5, dv.getFloat64(16, true));
foo(24, NaN, false);
assertEquals(NaN, dv.getFloat64(24));
assertOptimized(foo);

// Out-of-bounds offset deopts through the reduced bounds-check path.
assertThrows(() => foo(60, 1.5, false), RangeError);
assertUnoptimized(foo);
