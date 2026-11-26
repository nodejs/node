// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

// Regression test for Runtime_StringReplaceNonGlobalRegExpWithFunction:
// Toggling JSRegExp::data between a RegExp with many named capture groups and a
// RegExp with a single capture group must not cause an out-of-bounds read on
// the replacement-callback arguments vector.

const kJSRegExpType = Sandbox.getInstanceTypeIdFor('JS_REG_EXP_TYPE');
const kDataOffset = Sandbox.getFieldOffset(kJSRegExpType, 'data');

// Put the first named capture at index 8 (past DirectHandleSmallVector's
// 8-slot inline buffer) so an out-of-bounds read on the stack-allocated
// arguments vector hits the ASAN stack redzone instead of a null slot.
const kUnnamedCaptures = 7;
const namedPattern = '()'.repeat(kUnnamedCaptures) + '(?<g>a)';
const reNamed = new RegExp(namedPattern);
const reShort = /(a)/;
const reTarget = /(a)/;
const subject = 'a';

// Compile both RegExps so capture_name_map is populated on reNamed's
// IrRegExpData.
assertNotNull(reNamed.exec(subject));
assertNotNull(reShort.exec(subject));
assertNotNull(reTarget.exec(subject));

// Promote RegExps to old space so minor GC does not relocate reTarget while the
// worker is writing to its data slot.
gc();
gc();

const namedHandle = Sandbox.readObjectField(reNamed, 'data');
const shortHandle = Sandbox.readObjectField(reShort, 'data');
assertNotEquals(namedHandle, shortHandle);

const shared = new SharedArrayBuffer(16);
const control = new Int32Array(shared);
// control[0]: stop flag (0 = run, 1 = stop)
// control[1]: target JSRegExp::data slot address
// control[2]: namedHandle
// control[3]: shortHandle
Atomics.store(control, 0, 0);
Atomics.store(control, 1, (Sandbox.getAddressOf(reTarget) + kDataOffset) | 0);
Atomics.store(control, 2, namedHandle | 0);
Atomics.store(control, 3, shortHandle | 0);

const worker = new Worker(`
  onmessage = function(e) {
    const control = new Int32Array(e.data);
    const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    const handleA = Atomics.load(control, 2) >>> 0;
    const handleB = Atomics.load(control, 3) >>> 0;
    postMessage('ready');
    while (Atomics.load(control, 0) === 0) {
      const addr = Atomics.load(control, 1) >>> 0;
      for (let i = 0; i < 1000; i++) {
        mem.setUint32(addr, handleA, true);
        mem.setUint32(addr, handleB, true);
      }
    }
  };
`, {type: 'string'});

worker.postMessage(shared);
assertEquals('ready', worker.getMessage());

let mismatch = false;
try {
  for (let i = 0; i < 5000; i++) {
    subject.replace(reTarget, (...args) => {
      if (args.length !== 4 && !(args.length === 12 && args[11].g === 'a')) {
        mismatch = true;
      }
      return '';
    });
  }
} finally {
  Atomics.store(control, 0, 1);
  worker.terminate();
  Sandbox.corruptObjectField(reTarget, 'data', shortHandle);
}
assertFalse(mismatch);
