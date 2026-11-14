// Flags: --js-immutable-arraybuffer
'use strict';

require('../common');
const assert = require('assert');

function immutable(bytes) {
  const buffer = Uint8Array.from(bytes).buffer.transferToImmutable();
  assert.strictEqual(buffer.immutable, true);
  return new Uint8Array(buffer);
}

function check(actual, expected) {
  assert(Buffer.isBuffer(actual));
  assert.deepStrictEqual([...actual], expected);
  assert.strictEqual(actual.buffer.immutable, false);
  actual.fill(0);
}

{
  const source = immutable([1, 2, 3, 4]);
  check(Buffer.from(source), [1, 2, 3, 4]);
  check(Buffer.from(source.subarray(1, 3)), [2, 3]);
  check(Buffer.from(Buffer.from(source.buffer)), [1, 2, 3, 4]);
  assert.deepStrictEqual([...source], [1, 2, 3, 4]);
}

{
  // Larger than half the pool size, so the copy is not pooled.
  const bytes = Array.from({ length: Buffer.poolSize }, (_, i) => i & 0xff);
  const source = immutable(bytes);
  check(Buffer.from(source), bytes);
  check(Buffer.from(source.subarray(1)), bytes.slice(1));
}

{
  const source = immutable([1, 2, 3, 4]);
  check(Buffer.copyBytesFrom(source), [1, 2, 3, 4]);
  check(Buffer.copyBytesFrom(source, 1, 2), [2, 3]);
  const wide = new Uint16Array(Uint16Array.from([0x0102, 0x0304]).buffer.transferToImmutable());
  check(Buffer.copyBytesFrom(wide, 1), [...new Uint8Array(Uint16Array.from([0x0304]).buffer)]);
}

{
  const a = immutable([1, 2]);
  const b = Buffer.from([3, 4]);
  const c = immutable([5, 6]);
  check(Buffer.concat([a, b, c]), [1, 2, 3, 4, 5, 6]);
  check(Buffer.concat([a, b, c], 6), [1, 2, 3, 4, 5, 6]);
  // The last element is cut short.
  check(Buffer.concat([a, b, c], 5), [1, 2, 3, 4, 5]);
  // The result is longer than the sum of the elements and gets zero-filled.
  check(Buffer.concat([a, c], 5), [1, 2, 5, 6, 0]);
  check(Buffer.concat([a.subarray(1), c.subarray(0, 1)]), [2, 5]);
  assert.deepStrictEqual([...a], [1, 2]);
  assert.deepStrictEqual([...c], [5, 6]);
}

{
  // The other direction is unchanged: the copy must not write into a view
  // backed by an immutable ArrayBuffer.
  const target = immutable([9, 9, 9, 9]);
  assert.strictEqual(Buffer.from([1, 2, 3, 4]).copy(target), 0);
  assert.deepStrictEqual([...target], [9, 9, 9, 9]);
}
