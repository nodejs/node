// Flags: --expose-internals --expose-gc --js-immutable-arraybuffer
'use strict';
const common = require('../common');
const { gcUntil } = require('../common/gc');
const assert = require('node:assert/strict');
const { internalBinding } = require('internal/test/binding');
const { createArrayBufferBatch, trimCopyStorePool } = internalBinding('buffer');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

function releaseBatch() {
  const batch = createArrayBufferBatch(1024, 16, 0);
  for (const buffer of batch) new Uint8Array(buffer).fill(37);
  const frozen = batch[0].transferToImmutable();
  return {
    kept: batch[1].transfer(),
    released: new WeakRef(batch[2]),
    frozen: new WeakRef(frozen),
  };
}

(async () => {
  const { kept, released, frozen } = releaseBatch();
  await gcUntil('release buffered copies', () =>
    released.deref() === undefined && frozen.deref() === undefined);

  for (const buffer of createArrayBufferBatch(1024, 16, 0)) {
    assert.strictEqual(buffer.immutable, false);
    const bytes = new Uint8Array(buffer);
    assert(bytes.every((byte) => byte === 0));
    bytes.fill(11);
    assert(bytes.every((byte) => byte === 11));
  }
  trimCopyStorePool();
  assert(new Uint8Array(kept).every((byte) => byte === 37));
})().then(common.mustCall());
