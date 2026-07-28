// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const buffer = new ArrayBuffer(64);
const dv = new DataView(buffer);

// `store` is lazily inlined (eager-inline bytecode size is 0), so the
// DataView.prototype.setInt32 call is reduced by the optimizer-side reducer
// post-inline rather than at build time.
function store(dv, offset, value) { dv.setInt32(offset, value); }

function foo(offset, value) { store(dv, offset, value); }

%PrepareFunctionForOptimization(store);
%PrepareFunctionForOptimization(foo);

foo(8, 0x12345678);
assertEquals(0x12345678, dv.getInt32(8));

%OptimizeFunctionOnNextCall(foo);
foo(12, 0x01234567);
assertEquals(0x01234567, dv.getInt32(12));
foo(16, -1);
assertEquals(-1, dv.getInt32(16));
assertOptimized(foo);

// Out-of-bounds offset deopts through the reduced bounds-check path.
assertThrows(() => foo(62, 1), RangeError);
assertUnoptimized(foo);
