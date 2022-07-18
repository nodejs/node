'use strict';
const common = require('../common');

// Test Buffer.from(ArrayBufferView)

const assert = require('assert');

// The buffer contains numbers from 27 * 0 to 27 * 127, modulo 256
const full = Uint8Array.from({ length: 128 }, (_, i) => 27 * i);

{
  const expected = Buffer.from(full.buffer);

  assert.deepStrictEqual(
    Buffer.from(Buffer.from(full.buffer)),
    expected);

  for (const abv of common.getArrayBufferViews(full)) {
    assert.deepStrictEqual(
      Buffer.from(abv),
      expected);
  }
}

{
  const expected = Buffer.from(full.buffer.slice(48, 96));

  assert.deepStrictEqual(
    Buffer.from(Buffer.from(full.buffer, 48, 48)),
    expected);

  assert.deepStrictEqual(
    Buffer.from(new Int8Array(full.buffer, 48, 48)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new Uint8Array(full.buffer, 48, 48)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new Uint8ClampedArray(full.buffer, 48, 48)),
    expected);

  assert.deepStrictEqual(
    Buffer.from(new Int16Array(full.buffer, 48, 24)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new Uint16Array(full.buffer, 48, 24)),
    expected);

  assert.deepStrictEqual(
    Buffer.from(new Int32Array(full.buffer, 48, 12)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new Uint32Array(full.buffer, 48, 12)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new Float32Array(full.buffer, 48, 12)),
    expected);

  assert.deepStrictEqual(
    Buffer.from(new Float64Array(full.buffer, 48, 6)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new BigInt64Array(full.buffer, 48, 6)),
    expected);
  assert.deepStrictEqual(
    Buffer.from(new BigUint64Array(full.buffer, 48, 6)),
    expected);

  assert.deepStrictEqual(
    Buffer.from(new DataView(full.buffer, 48, 48)),
    expected);
}
