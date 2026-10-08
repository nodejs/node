// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const buffer = new ArrayBuffer(64);
const dv = new DataView(buffer);
dv.setFloat64(8, 12.2536);
dv.setFloat64(16, -0.5, true);

// `load` is lazily inlined (eager-inline bytecode size is 0), so the
// DataView.prototype.getFloat64 call is reduced by the optimizer-side reducer
// post-inline rather than at build time.
function load(dv, offset, little_endian) {
  return dv.getFloat64(offset, little_endian);
}

function foo(offset, little_endian) { return load(dv, offset, little_endian); }

%PrepareFunctionForOptimization(load);
%PrepareFunctionForOptimization(foo);

assertEquals(12.2536, foo(8, false));
assertEquals(-0.5, foo(16, true));

%OptimizeFunctionOnNextCall(foo);
assertEquals(12.2536, foo(8, false));
assertEquals(-0.5, foo(16, true));
assertOptimized(foo);

// Out-of-bounds offset deopts through the reduced bounds-check path.
assertThrows(() => foo(60, false), RangeError);
assertUnoptimized(foo);
