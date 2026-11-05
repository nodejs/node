// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const extension = buffer =>
    Sandbox.getAddressOf(buffer) +
    Sandbox.getFieldOffset(
        Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE'), 'extension');
const resizable = () => new ArrayBuffer(8, {maxByteLength: 1 << 20});
const scavenge = () => gc({type: 'minor'});

let padding = [];
// Work for the sweeper job.
for (let i = 0; i < 20000; i++) padding.push(new ArrayBuffer(8));
const Q = resizable();
scavenge();
scavenge();

let box = [resizable()];
scavenge();
// A fresh object holds S and P: S is visited first.
let pair = (() => {
  const s = new ArrayBuffer(8), p = box.pop();
  // Write 1: S gets the handle of P.
  mem.setUint32(extension(s), mem.getUint32(extension(p), true), true);
  return [s, p];
})();
// Copies S, then promotes P.
scavenge();
const handle = (() => mem.getUint32(extension(pair[1]), true))();
pair = padding = null;
gc();
// Write 2: Q gets the new handle of P.
mem.setUint32(extension(Q), handle, true);
for (let i = 0; i < 20000; i++) Q.resize((i & 1) << 19);
