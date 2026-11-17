'use strict';

require('../common');
const zlib = require('zlib');
const assert = require('assert');
const { Buffer } = require('buffer');

// Test vectors from RFC 3720, Appendix B.4.
{
  const ascending = Buffer.alloc(32);
  const descending = Buffer.alloc(32);
  for (let i = 0; i < 32; i++) {
    ascending[i] = i;
    descending[i] = 31 - i;
  }
  const iscsiReadPdu = Buffer.from(
    '01c00000000000000000000000000000' +
    '14000000000004000000001400000018' +
    '28000000000000000200000000000000', 'hex');

  const tests = [
    [Buffer.alloc(32, 0x00), 0x8a9136aa],
    [Buffer.alloc(32, 0xff), 0x62a8ab43],
    [ascending, 0x46dd794e],
    [descending, 0x113fdb5c],
    [iscsiReadPdu, 0xd9963a56],
  ];
  for (const [data, expected] of tests) {
    assert.strictEqual(zlib.crc32c(data), expected);
    assert.strictEqual(zlib.crc32c(data, 0), expected);
  }
}

assert.strictEqual(zlib.crc32c('123456789'), 0xe3069283);
assert.strictEqual(zlib.crc32c(Buffer.from('123456789')), 0xe3069283);

assert.strictEqual(zlib.crc32c(''), 0);
assert.strictEqual(zlib.crc32c(Buffer.alloc(0)), 0);
assert.strictEqual(zlib.crc32c('', 0xffffffff), 0xffffffff);
assert.strictEqual(zlib.crc32c(Buffer.alloc(0), 0x12345678), 0x12345678);
assert.strictEqual(zlib.crc32c('123456789', -0), 0xe3069283);

// Strings are encoded as UTF-8.
{
  const str = 'héllo wörld ✓ 🚀';
  assert.strictEqual(zlib.crc32c(str), zlib.crc32c(Buffer.from(str, 'utf8')));
}

// Only the bytes covered by the view are used.
{
  const backing = Buffer.from('xx123456789yy');
  const view = backing.subarray(2, 11);
  assert.strictEqual(zlib.crc32c(view), 0xe3069283);
  assert.strictEqual(
    zlib.crc32c(new Uint8Array(backing.buffer, backing.byteOffset + 2, 9)),
    0xe3069283);
  assert.strictEqual(
    zlib.crc32c(new DataView(backing.buffer, backing.byteOffset + 2, 9)),
    0xe3069283);
}

// Chaining with `value` matches a single pass, across sizes and split points
// large enough to reach every code path of the implementation.
{
  const data = Buffer.allocUnsafe(1 << 20);
  for (let i = 0; i < data.length; i++) data[i] = (i * 1103515245 + 12345) & 0xff;
  for (const len of [1, 7, 15, 16, 17, 63, 64, 65, 255, 256, 4095, 4096,
                     65537, data.length]) {
    const chunk = data.subarray(0, len);
    const whole = zlib.crc32c(chunk);
    for (const split of new Set([0, 1, len >> 1, len - 1, len])) {
      const crc = zlib.crc32c(chunk.subarray(split),
                              zlib.crc32c(chunk.subarray(0, split)));
      assert.strictEqual(crc, whole, `len=${len} split=${split}`);
    }
  }
}

// Different from CRC-32.
assert.notStrictEqual(zlib.crc32c('123456789'), zlib.crc32('123456789'));

[undefined, null, true, 1, () => {}, {}].forEach((invalid) => {
  assert.throws(() => { zlib.crc32c(invalid); }, { code: 'ERR_INVALID_ARG_TYPE' });
});

[null, true, () => {}, {}].forEach((invalid) => {
  assert.throws(() => { zlib.crc32c('test', invalid); }, { code: 'ERR_INVALID_ARG_TYPE' });
});

[-1, 2 ** 32, 1.5, NaN].forEach((invalid) => {
  assert.throws(() => { zlib.crc32c('test', invalid); }, { code: 'ERR_OUT_OF_RANGE' });
});
