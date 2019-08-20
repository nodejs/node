// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { arrayBufferViewHasBuffer } = internalBinding('util');

const tests = [
  { length: 0, expectOnHeap: true },
  { length: 48, expectOnHeap: true },
  { length: 96, expectOnHeap: false },
  { length: 1024, expectOnHeap: false },
];

for (const { length, expectOnHeap } of tests) {
  const arrays = [
    new Uint8Array(length),
    new Uint16Array(length / 2),
    new Uint32Array(length / 4),
    new Float32Array(length / 4),
    new Float64Array(length / 8),
    Buffer.alloc(length),
    Buffer.allocUnsafeSlow(length)
    // Buffer.allocUnsafe() is missing because it may use pooled allocations.
  ];

  for (const array of arrays) {
    const isOnHeap = !arrayBufferViewHasBuffer(array);
    assert.strictEqual(isOnHeap, expectOnHeap,
                       `mismatch: ${isOnHeap} vs ${expectOnHeap} ` +
                       `for ${array.constructor.name}, length = ${length}`);

    // Consistency check: Accessing .buffer should create it.
    array.buffer;
    assert(arrayBufferViewHasBuffer(array));
  }
}
