// Flags: --expose-internals --max-semi-space-size=1 --no-inline-new
'use strict';
require('../common');
const assert = require('node:assert/strict');
const { internalBinding } = require('internal/test/binding');
const { createArrayBufferBatch } = internalBinding('buffer');

const retained = [];
for (let i = 0; i < 2048; i++) {
  const batch = createArrayBufferBatch(128, 64, 0);
  assert.strictEqual(batch.length, 64);
  const view = new Uint8Array(batch[i % 64]);
  assert.strictEqual(view.byteLength, 128);
  if (i % 31 === 0) {
    view[0] = retained.length + 1;
    retained.push(view);
  }
}
for (const [i, view] of retained.entries())
  assert.strictEqual(view[0], i + 1);
