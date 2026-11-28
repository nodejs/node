// Flags: --expose-internals --expose-gc --no-concurrent-array-buffer-sweeping
'use strict';

const common = require('../../common');
const assert = require('node:assert/strict');
const { internalBinding } = require('internal/test/binding');
const { createArrayBufferBatch, trimCopyStorePool } = internalBinding('buffer');
const { checkOwnership } = require(`./build/${common.buildType}/binding`);

for (const [count, index] of [[15, 0], [16, 15], [17, 0], [17, 16], [64, 32]]) {
  const buffers = createArrayBufferBatch(1024, count, 0);
  const source = buffers[index];
  new Uint8Array(source).fill(37);

  const independent = checkOwnership(source, common.mustCall((alias) => {
    for (const buffer of buffers) buffer.transfer(0);
    assert(new Uint8Array(alias).every((byte) => byte === 37));
    alias.transfer(0);
    trimCopyStorePool();
    global.gc();
  }), () => createArrayBufferBatch(1024, count, 0)[index]);
  assert.strictEqual(independent, true);
}
