'use strict';
const common = require('../common');
const assert = require('assert');

const b = Buffer.from('abcdef');

// 3-argument calls: buf.indexOf(val, offset, encoding) where encoding is null
assert.strictEqual(b.indexOf(0x62, 0, null), 1);
assert.strictEqual(b.indexOf(Buffer.from('b'), 0, null), 1);
assert.strictEqual(b.indexOf(new Uint8Array([0x62]), 0, null), 1);
assert.throws(() => b.indexOf('b', 0, null), {
  code: 'ERR_UNKNOWN_ENCODING',
});

// Non-string object encoding in 3-argument calls
assert.strictEqual(b.indexOf(0x62, 0, {}), 1);
assert.strictEqual(b.indexOf(Buffer.from('b'), 0, {}), 1);
assert.throws(() => b.indexOf('b', 0, {}), {
  code: 'ERR_UNKNOWN_ENCODING',
});

// 3-argument calls with false encoding
assert.strictEqual(b.indexOf(0x62, 0, false), 1);
assert.strictEqual(b.indexOf(Buffer.from('b'), 0, false), 1);
assert.throws(() => b.indexOf('b', 0, false), {
  code: 'ERR_UNKNOWN_ENCODING',
});

// lastIndexOf with null and non-string encoding
assert.strictEqual(b.lastIndexOf(0x62, 5, null), 1);
assert.strictEqual(b.lastIndexOf(Buffer.from('b'), 5, null), 1);
assert.strictEqual(b.lastIndexOf(new Uint8Array([0x62]), 5, null), 1);
assert.throws(() => b.lastIndexOf('b', 5, null), {
  code: 'ERR_UNKNOWN_ENCODING',
});
assert.strictEqual(b.lastIndexOf(0x62, 5, {}), 1);
assert.strictEqual(b.lastIndexOf(Buffer.from('b'), 5, {}), 1);
assert.throws(() => b.lastIndexOf('b', 5, {}), {
  code: 'ERR_UNKNOWN_ENCODING',
});

// includes with null and non-string encoding
assert.strictEqual(b.includes(0x62, 0, null), true);
assert.strictEqual(b.includes(Buffer.from('b'), 0, null), true);
assert.strictEqual(b.includes(new Uint8Array([0x62]), 0, null), true);
assert.throws(() => b.includes('b', 0, null), {
  code: 'ERR_UNKNOWN_ENCODING',
});
assert.strictEqual(b.includes(0x62, 0, {}), true);
assert.strictEqual(b.includes(Buffer.from('b'), 0, {}), true);
assert.throws(() => b.includes('b', 0, {}), {
  code: 'ERR_UNKNOWN_ENCODING',
});

// 2-argument calls where byteOffset is null
assert.strictEqual(b.indexOf('b', null), 1);
assert.strictEqual(b.indexOf(0x62, null), 1);
assert.strictEqual(b.indexOf(Buffer.from('b'), null), 1);
assert.strictEqual(b.lastIndexOf('b', null), -1);
assert.strictEqual(b.lastIndexOf(0x62, null), -1);
assert.strictEqual(b.lastIndexOf(Buffer.from('b'), null), -1);
assert.strictEqual(b.includes('b', null), true);
assert.strictEqual(b.includes(0x62, null), true);
assert.strictEqual(b.includes(Buffer.from('b'), null), true);

// 3-argument calls where byteOffset is null and encoding is null
assert.strictEqual(b.indexOf(0x62, null, null), 1);
assert.strictEqual(b.indexOf(Buffer.from('b'), null, null), 1);
assert.throws(() => b.indexOf('b', null, null), {
  code: 'ERR_UNKNOWN_ENCODING',
});
assert.strictEqual(b.includes(0x62, null, null), true);
assert.strictEqual(b.includes(Buffer.from('b'), null, null), true);
assert.throws(() => b.includes('b', null, null), {
  code: 'ERR_UNKNOWN_ENCODING',
});

// 3-argument calls with numeric end bounds still work as expected
assert.strictEqual(b.indexOf('b', 0, 1), -1);
assert.strictEqual(b.indexOf('b', 0, 2), 1);
assert.strictEqual(b.indexOf(0x62, 0, 1), -1);
assert.strictEqual(b.indexOf(0x62, 0, 2), 1);
assert.strictEqual(b.indexOf(Buffer.from('b'), 0, 1), -1);
assert.strictEqual(b.indexOf(Buffer.from('b'), 0, 2), 1);
assert.strictEqual(b.includes('b', 0, 1), false);
assert.strictEqual(b.includes('b', 0, 2), true);
