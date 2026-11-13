// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = (addr) => memory.getUint32(addr >>> 0, true);
const write32 = (addr, val) => memory.setUint32(addr >>> 0, val >>> 0, true);
const addressOf = (obj) => Sandbox.getAddressOf(obj) >>> 0;
const tagged = (obj) => (addressOf(obj) + 1) >>> 0;

function findPropertySlot(obj, key) {
  const map = (read32(addressOf(obj)) - 1) >>> 0;
  assertTrue((read32(map + 12) & (1 << 21)) !== 0);
  const dictionary = (read32(addressOf(obj) + 4) - 1) >>> 0;
  const capacity = read32(dictionary + 16) >>> 1;
  for (let i = 0; i < capacity; ++i) {
    const entry = dictionary + 32 + 12 * i;
    if (read32(entry) === tagged(key)) return entry + 4;
  }
  throw new Error('Property slot not found: ' + key);
}

let hits = 0;
const victim = [];
Object.defineProperty(victim, 'stackTraceLimit', {
  get() {
    ++hits;
    return 10;
  },
  configurable: true,
});
Object.defineProperty(victim, 'keepAlive', {
  get() {},
  configurable: true,
});

const originalPrototype = Object.getPrototypeOf(Error);
const originalLimit = Object.getOwnPropertyDescriptor(Error, 'stackTraceLimit');
delete Error.stackTraceLimit;
Object.setPrototypeOf(Error, victim);

const descriptor = findPropertySlot(victim, 'stackTraceLimit');
const pair = read32(descriptor);
// Keep `pair` strongly reachable from `victim`'s NameDictionary while the
// worker temporarily overwrites `descriptor` with `native`.
write32(findPropertySlot(victim, 'keepAlive'), pair);
const native = read32(findPropertySlot(victim, 'length'));

// Tenure `victim` and its property dictionary in OldSpace before racing.
gc();
gc();

const control = new Int32Array(new SharedArrayBuffer(8));
const worker = new Worker(function() {
  onmessage = function(event) {
    const {buffer, descriptor, native, pair} = event.data;
    const control = new Int32Array(buffer);
    const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    Atomics.store(control, 0, 1);
    while (Atomics.load(control, 1) === 0) {
      memory.setUint32(descriptor, native, true);
      memory.setUint32(descriptor, pair, true);
    }
    memory.setUint32(descriptor, pair, true);
    postMessage('done');
  };
}, {type: 'function'});

worker.postMessage({buffer: control.buffer, descriptor, native, pair});
while (Atomics.load(control, 0) === 0) {}

try {
  for (let i = 0; i < 10 && hits === 0; ++i) {
    new Error();
    Object.getOwnPropertyDescriptor(victim, 'stackTraceLimit');
  }
} finally {
  Atomics.store(control, 1, 1);
  worker.getMessage();
  worker.terminate();
  write32(descriptor, pair);
  Object.setPrototypeOf(Error, originalPrototype);
  Object.defineProperty(Error, 'stackTraceLimit', originalLimit);
}

assertEquals(0, hits);
