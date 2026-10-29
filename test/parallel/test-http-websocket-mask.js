'use strict';

require('../common');
const assert = require('assert');
const { websocketMask, websocketUnmask } = require('http');

// Reference implementation (RFC 6455, Section 5.3).
function referenceMask(source, key, length = source.length) {
  const out = Buffer.alloc(length);
  for (let i = 0; i < length; i++) out[i] = source[i] ^ key[i & 3];
  return out;
}

function randomBytes(length) {
  const buf = Buffer.alloc(length);
  for (let i = 0; i < length; i++) buf[i] = (Math.random() * 256) | 0;
  return buf;
}

const key = Buffer.from([0x12, 0x34, 0x56, 0x78]);

// Sizes around the 8- and 32-byte chunk boundaries, and a large buffer.
const sizes = [];
for (let i = 0; i <= 80; i++) sizes.push(i);
sizes.push(125, 126, 127, 1023, 1024, 1025, 65535, 65536, 65537, 1024 * 1024 + 3);

// websocketMask() and websocketUnmask() match the reference, for source and
// output views at every alignment.
for (const size of sizes) {
  const data = randomBytes(size);
  const expected = referenceMask(data, key);
  for (const align of size > 4096 ? [0, 3] : [0, 1, 2, 3, 4, 5, 6, 7]) {
    const source = Buffer.alloc(size + align).subarray(align);
    data.copy(source);
    const output = Buffer.alloc(size + 8).subarray(7 - align, 7 - align + size);
    websocketMask(source, key, output, 0, size);
    assert.deepStrictEqual(output, expected, `mask size=${size} align=${align}`);
    // The source must not be modified.
    assert.deepStrictEqual(source, data);

    websocketUnmask(output, key);
    assert.deepStrictEqual(output, data, `unmask size=${size} align=${align}`);
  }
}

// Masking twice with the same key is the identity; websocketMask() in place works.
{
  const data = randomBytes(1000);
  const buf = Buffer.from(data);
  websocketMask(buf, key, buf, 0, buf.length);
  assert.deepStrictEqual(buf, referenceMask(data, key));
  websocketMask(buf, key, buf, 0, buf.length);
  assert.deepStrictEqual(buf, data);
}

// The offset and length arguments.
{
  const data = randomBytes(100);
  const output = Buffer.alloc(110, 0xaa);
  websocketMask(data, key, output, 6, 90);
  assert.deepStrictEqual(output.subarray(0, 6), Buffer.alloc(6, 0xaa));
  assert.deepStrictEqual(output.subarray(6, 96), referenceMask(data, key, 90));
  assert.deepStrictEqual(output.subarray(96), Buffer.alloc(14, 0xaa));
}

// Overlapping source and output: the result is as if the source had been
// copied before masking.
for (const size of [1, 7, 8, 31, 32, 33, 100, 5000]) {
  for (const shift of [-33, -9, -8, -1, 1, 3, 8, 9, 33]) {
    const base = size + 40;
    const backing = randomBytes(base * 2);
    const srcStart = 40;
    const dstStart = srcStart + shift;
    const expected = referenceMask(backing.subarray(srcStart, srcStart + size), key);

    // Views over the same memory.
    const ab = Buffer.from(backing);
    const source = ab.subarray(srcStart, srcStart + size);
    const output = ab.subarray(dstStart, dstStart + size);
    websocketMask(source, key, output, 0, size);
    assert.deepStrictEqual(output, expected, `views size=${size} shift=${shift}`);

    // Same object, using offset to shift the output.
    if (shift > 0) {
      const buf = Buffer.from(backing.subarray(srcStart, srcStart + size + shift));
      websocketMask(buf, key, buf, shift, size);
      assert.deepStrictEqual(buf.subarray(shift), expected,
                             `same object size=${size} shift=${shift}`);
    }
  }
}

// The mask may alias the output.
{
  const buf = Buffer.from([1, 2, 3, 4, 10, 20, 30, 40, 50, 60]);
  const aliasKey = buf.subarray(0, 4);
  const expected = referenceMask(buf, Buffer.from([1, 2, 3, 4]));
  websocketMask(buf, aliasKey, buf, 0, buf.length);
  assert.deepStrictEqual(buf, expected);
}

// Any ArrayBufferView works, and is treated as bytes.
{
  const data = randomBytes(64);
  const expected = referenceMask(data, key);
  const u16 = new Uint16Array(data.buffer.slice(data.byteOffset, data.byteOffset + 64));
  const dv = new DataView(new ArrayBuffer(64));
  websocketMask(u16, key, dv, 0, 64);
  assert.deepStrictEqual(Buffer.from(dv.buffer), expected);

  const keyViews = [
    new Uint8Array([0x12, 0x34, 0x56, 0x78]),
    new Uint32Array(new Uint8Array([0x12, 0x34, 0x56, 0x78]).buffer),
    new DataView(new Uint8Array([0, 0x12, 0x34, 0x56, 0x78]).buffer, 1, 4),
    Buffer.from([0, 0, 0x12, 0x34, 0x56, 0x78, 0]).subarray(2, 6),
    new Float32Array(new Uint8Array([0x12, 0x34, 0x56, 0x78]).buffer),
  ];
  for (const k of keyViews) {
    const out = Buffer.alloc(64);
    websocketMask(data, k, out, 0, 64);
    assert.deepStrictEqual(out, expected, k.constructor.name);

    const copy = new Uint8Array(data);
    websocketUnmask(copy, k);
    assert.deepStrictEqual(Buffer.from(copy), expected, k.constructor.name);
  }
}

// Views backed by a SharedArrayBuffer.
{
  const data = randomBytes(100);
  const shared = new Uint8Array(new SharedArrayBuffer(100));
  shared.set(data);
  websocketUnmask(shared, key);
  assert.deepStrictEqual(Buffer.from(shared), referenceMask(data, key));
}

// Empty and detached inputs.
{
  websocketMask(Buffer.alloc(0), key, Buffer.alloc(0), 0, 0);
  websocketUnmask(Buffer.alloc(0), key);

  const ab = new ArrayBuffer(16);
  const detached = new Uint8Array(ab);
  structuredClone(ab, { transfer: [ab] });
  assert.strictEqual(detached.byteLength, 0);
  websocketUnmask(detached, key);
  websocketMask(detached, key, Buffer.alloc(4), 0, 0);
  assert.throws(() => websocketMask(detached, key, Buffer.alloc(4), 0, 4), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

// Argument validation.
{
  const buf = Buffer.alloc(8);
  const invalidViews = [undefined, null, 1, 'abcd', [1, 2, 3, 4], {}, new ArrayBuffer(8)];

  for (const value of invalidViews) {
    assert.throws(() => websocketMask(value, key, buf, 0, 8), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => websocketMask(buf, key, value, 0, 8), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => websocketMask(buf, value, buf, 0, 8), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => websocketUnmask(value, key), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => websocketUnmask(buf, value), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  for (const badKey of [Buffer.alloc(0), Buffer.alloc(3), Buffer.alloc(5), new Uint16Array(4)]) {
    assert.throws(() => websocketMask(buf, badKey, buf, 0, 8), { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => websocketUnmask(buf, badKey), { code: 'ERR_INVALID_ARG_VALUE' });
  }

  for (const offset of [-1, 9, 1.5, NaN, Infinity]) {
    assert.throws(() => websocketMask(buf, key, buf, offset, 0), { code: 'ERR_OUT_OF_RANGE' });
  }
  // The offset and length arguments are required.
  assert.throws(() => websocketMask(buf, key, buf), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => websocketMask(buf, key, buf, 0), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => websocketMask(buf, key, buf, undefined, 8),
                { code: 'ERR_INVALID_ARG_TYPE' });

  for (const offset of ['1', null, 1n]) {
    assert.throws(() => websocketMask(buf, key, buf, offset, 0), { code: 'ERR_INVALID_ARG_TYPE' });
  }
  for (const length of [-1, 9, 1.5, NaN, Infinity]) {
    assert.throws(() => websocketMask(buf, key, buf, 0, length), { code: 'ERR_OUT_OF_RANGE' });
  }
  for (const length of ['1', null, 1n]) {
    assert.throws(() => websocketMask(buf, key, buf, 0, length), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  // The sum of offset and length must fit in the output.
  assert.throws(() => websocketMask(buf, key, Buffer.alloc(8), 1, 8), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => websocketMask(buf, key, Buffer.alloc(4), 0, 8), { code: 'ERR_OUT_OF_RANGE' });
  // The length must fit in the source.
  assert.throws(() => websocketMask(Buffer.alloc(4), key, buf, 0, 5), { code: 'ERR_OUT_OF_RANGE' });

  // Nothing is written when validation fails.
  const out = Buffer.alloc(8, 0xaa);
  assert.throws(() => websocketMask(buf, key, out, 1, 8), { code: 'ERR_OUT_OF_RANGE' });
  assert.deepStrictEqual(out, Buffer.alloc(8, 0xaa));
}

// Return values.
assert.strictEqual(websocketMask(Buffer.alloc(4), key, Buffer.alloc(4), 0, 4), undefined);
assert.strictEqual(websocketUnmask(Buffer.alloc(4), key), undefined);
