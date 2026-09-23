// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --maglev

const getInt32 = DataView.prototype.getInt32;

function read(holder) {
  return getInt32.call(holder.x, 0, true);
}

function Holder(x) {
  this.x = x;
}

const a = new DataView(new ArrayBuffer(16));
const b = new DataView(new ArrayBuffer(16));
a.p = 1;
b.p = 2;

// `Holder.x` learns the DataView map of `a` and `b` as its class field type.
const holder = new Holder(a);
holder.x = b;

// Generalizing `p` to double makes that map unstable.
a.p = 1.5;

%PrepareFunctionForOptimization(read);
read(holder);
read(holder);
%OptimizeMaglevOnNextCall(read);
read(holder);

// The last instance migrates away, so the class map dies and the GC widens the
// field type to Any without invalidating the optimized code.
b.p = 2.5;
gc();
gc();

holder.x = {a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8, i: 9};
assertThrows(() => read(holder), TypeError);
