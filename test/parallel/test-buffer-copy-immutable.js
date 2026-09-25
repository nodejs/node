// Flags: --js-immutable-arraybuffer
'use strict';
const common = require('../common');
const assert = require('assert');

// transferToImmutable is gated behind --js-immutable-arraybuffer (set above).
// Skip if this V8 build does not expose the API even with the flag set.
if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

// Copying *into* a buffer backed by an immutable ArrayBuffer must not write to
// the read-only backing store. The copy is a no-op and reports 0 bytes copied.
{
  const ab = new ArrayBuffer(8);
  new Uint8Array(ab).set([1, 2, 3, 4, 5, 6, 7, 8]);
  const target = Buffer.from(ab.transferToImmutable());
  const source = Buffer.from([9, 9, 9, 9, 9, 9, 9, 9]);

  assert.strictEqual(source.copy(target), 0);
  assert.deepStrictEqual([...target], [1, 2, 3, 4, 5, 6, 7, 8]);

  // A partial / offset copy is also a no-op.
  assert.strictEqual(source.copy(target, 2, 0, 4), 0);
  assert.deepStrictEqual([...target], [1, 2, 3, 4, 5, 6, 7, 8]);
}

// Copying *from* a buffer backed by an immutable ArrayBuffer is allowed (reads
// do not require a writable backing store) and reports the bytes copied.
{
  const ab = new ArrayBuffer(8);
  new Uint8Array(ab).set([10, 20, 30, 40, 50, 60, 70, 80]);
  const source = Buffer.from(ab.transferToImmutable());
  const target = Buffer.alloc(8);

  assert.strictEqual(source.copy(target), 8);
  assert.deepStrictEqual([...target], [10, 20, 30, 40, 50, 60, 70, 80]);
}

// A mutable Uint8Array view onto an immutable ArrayBuffer is still a read-only
// target through Buffer.prototype.copy.
{
  const ab = new ArrayBuffer(4);
  new Uint8Array(ab).set([100, 101, 102, 103]);
  const target = new Uint8Array(ab.transferToImmutable());

  assert.strictEqual(Buffer.from([1, 2, 3, 4]).copy(target), 0);
  assert.deepStrictEqual([...target], [100, 101, 102, 103]);
}
