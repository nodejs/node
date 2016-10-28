'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(0, Buffer.from('hello').slice(0, 0).length);
assert.strictEqual(0, Buffer('hello').slice(0, 0).length);

const buf = Buffer.from('0123456789');
assert.equal(buf.slice(-10, 10), '0123456789');
assert.equal(buf.slice(-20, 10), '0123456789');
assert.equal(buf.slice(-20, -10), '');
assert.equal(buf.slice(), '0123456789');
assert.equal(buf.slice(0), '0123456789');
assert.equal(buf.slice(0, 0), '');
assert.equal(buf.slice(undefined), '0123456789');
assert.equal(buf.slice('foobar'), '0123456789');
assert.equal(buf.slice(undefined, undefined), '0123456789');

assert.equal(buf.slice(2), '23456789');
assert.equal(buf.slice(5), '56789');
assert.equal(buf.slice(10), '');
assert.equal(buf.slice(5, 8), '567');
assert.equal(buf.slice(8, -1), '8');
assert.equal(buf.slice(-10), '0123456789');
assert.equal(buf.slice(0, -9), '0');
assert.equal(buf.slice(0, -10), '');
assert.equal(buf.slice(0, -1), '012345678');
assert.equal(buf.slice(2, -2), '234567');
assert.equal(buf.slice(0, 65536), '0123456789');
assert.equal(buf.slice(65536, 0), '');
assert.equal(buf.slice(-5, -8), '');
assert.equal(buf.slice(-5, -3), '56');
assert.equal(buf.slice(-10, 10), '0123456789');
for (let i = 0, s = buf.toString(); i < buf.length; ++i) {
  assert.equal(buf.slice(i), s.slice(i));
  assert.equal(buf.slice(0, i), s.slice(0, i));
  assert.equal(buf.slice(-i), s.slice(-i));
  assert.equal(buf.slice(0, -i), s.slice(0, -i));
}

const utf16Buf = Buffer.from('0123456789', 'utf16le');
assert.deepStrictEqual(utf16Buf.slice(0, 6), Buffer.from('012', 'utf16le'));

assert.equal(buf.slice('0', '1'), '0');
assert.equal(buf.slice('-5', '10'), '56789');
assert.equal(buf.slice('-10', '10'), '0123456789');
assert.equal(buf.slice('-10', '-5'), '01234');
assert.equal(buf.slice('-10', '-0'), '');
assert.equal(buf.slice('111'), '');
assert.equal(buf.slice('0', '-111'), '');

// try to slice a zero length Buffer
// see https://github.com/joyent/node/issues/5881
assert.doesNotThrow(() => Buffer.alloc(0).slice(0, 1));
assert.strictEqual(Buffer.alloc(0).slice(0, 1).length, 0);

{
  // Single argument slice
  assert.strictEqual('bcde', Buffer.from('abcde').slice(1).toString());
}

// slice(0,0).length === 0
assert.strictEqual(0, Buffer.from('hello').slice(0, 0).length);

{
  // Regression tests for https://github.com/nodejs/node/issues/9096
  const buf = Buffer.from('abcd');
  assert.strictEqual(buf.slice(buf.length / 3).toString(), 'bcd');
  assert.strictEqual(
    buf.slice(buf.length / 3, buf.length).toString(),
    'bcd'
  );
}

{
  const buf = Buffer.from('abcdefg');
  assert.strictEqual(buf.slice(-(-1 >>> 0) - 1).toString(), buf.toString());
}

{
  const buf = Buffer.from('abc');
  assert.strictEqual(buf.slice(-0.5).toString(), buf.toString());
}

{
  const buf = Buffer.from([
    1, 29, 0, 0, 1, 143, 216, 162, 92, 254, 248, 63, 0,
    0, 0, 18, 184, 6, 0, 175, 29, 0, 8, 11, 1, 0, 0
  ]);
  const chunk1 = Buffer.from([
    1, 29, 0, 0, 1, 143, 216, 162, 92, 254, 248, 63, 0
  ]);
  const chunk2 = Buffer.from([
    0, 0, 18, 184, 6, 0, 175, 29, 0, 8, 11, 1, 0, 0
  ]);
  const middle = buf.length / 2;

  assert.deepStrictEqual(buf.slice(0, middle), chunk1);
  assert.deepStrictEqual(buf.slice(middle), chunk2);
}
