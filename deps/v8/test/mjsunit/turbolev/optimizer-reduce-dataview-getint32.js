// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const buffer = new ArrayBuffer(64);
const dv = new DataView(buffer);
dv.setInt32(8, 0x12345678);
dv.setInt32(16, -1);

// `load` is lazily inlined (eager-inline bytecode size is 0), so the
// DataView.prototype.getInt32 call is reduced by the optimizer-side reducer
// post-inline rather than at build time.
function load(dv, offset) { return dv.getInt32(offset); }

function foo(offset) { return load(dv, offset); }

%PrepareFunctionForOptimization(load);
%PrepareFunctionForOptimization(foo);

assertEquals(0x12345678, foo(8));
assertEquals(-1, foo(16));

%OptimizeFunctionOnNextCall(foo);
assertEquals(0x12345678, foo(8));
assertEquals(-1, foo(16));
assertEquals(0, foo(0));
assertOptimized(foo);

// Out-of-bounds offset deopts through the reduced bounds-check path.
assertThrows(() => foo(62), RangeError);
assertUnoptimized(foo);
