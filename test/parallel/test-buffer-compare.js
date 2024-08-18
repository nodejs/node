'use strict';

require('../common');
const assert = require('assert');

const b = Buffer.alloc(1, 'a');
const c = Buffer.alloc(1, 'c');
const d = Buffer.alloc(2, 'aa');
const e = new Uint8Array([ 0x61, 0x61 ]); // ASCII 'aa', same as d

assert.strictEqual(b.compare(c), -1);
assert.strictEqual(c.compare(d), 1);
assert.strictEqual(d.compare(b), 1);
assert.strictEqual(d.compare(e), 0);
assert.strictEqual(b.compare(d), -1);
assert.strictEqual(b.compare(b), 0);

assert.strictEqual(Buffer.compare(b, c), -1);
assert.strictEqual(Buffer.compare(c, d), 1);
assert.strictEqual(Buffer.compare(d, b), 1);
assert.strictEqual(Buffer.compare(b, d), -1);
assert.strictEqual(Buffer.compare(c, c), 0);
assert.strictEqual(Buffer.compare(e, e), 0);
assert.strictEqual(Buffer.compare(d, e), 0);
assert.strictEqual(Buffer.compare(d, b), 1);

assert.strictEqual(Buffer.compare(Buffer.alloc(0), Buffer.alloc(0)), 0);
assert.strictEqual(Buffer.compare(Buffer.alloc(0), Buffer.alloc(1)), -1);
assert.strictEqual(Buffer.compare(Buffer.alloc(1), Buffer.alloc(0)), 1);

assert.throws(() => Buffer.compare(Buffer.alloc(1), 'abc'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => Buffer.compare('abc', Buffer.alloc(1)), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => Buffer.alloc(1).compare('abc'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// compare works with ArrayBuffer
{
  const buf = Buffer.from([1, 2, 3]);
  const ab = new ArrayBuffer(3);
  new Uint8Array(ab).set([1, 2, 3]);
  assert.strictEqual(buf.compare(ab), 0);

  const ab2 = new ArrayBuffer(3);
  new Uint8Array(ab2).set([4, 5, 6]);
  assert.strictEqual(buf.compare(ab2), -1);

  const ab3 = new ArrayBuffer(3);
  new Uint8Array(ab3).set([0, 0, 0]);
  assert.strictEqual(buf.compare(ab3), 1);
}

// compare works with DataView
{
  const buf = Buffer.from([1, 2, 3]);
  const dv = new DataView(new ArrayBuffer(3));
  new Uint8Array(dv.buffer).set([1, 2, 3]);
  assert.strictEqual(buf.compare(dv), 0);

  const dv2 = new DataView(new ArrayBuffer(3));
  new Uint8Array(dv2.buffer).set([4, 5, 6]);
  assert.strictEqual(buf.compare(dv2), -1);
}

// compare works with SharedArrayBuffer
{
  const buf = Buffer.from([1, 2, 3]);
  const sab = new SharedArrayBuffer(3);
  new Uint8Array(sab).set([1, 2, 3]);
  assert.strictEqual(buf.compare(sab), 0);
}

// compare works with other TypedArrays
{
  const buf = Buffer.from([0x01, 0x00, 0x02, 0x00]);
  const u16 = new Uint16Array([1, 2]);
  assert.strictEqual(buf.compare(u16), 0);
}
