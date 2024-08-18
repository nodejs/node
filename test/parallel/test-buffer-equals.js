'use strict';

require('../common');
const assert = require('assert');

const b = Buffer.from('abcdf');
const c = Buffer.from('abcdf');
const d = Buffer.from('abcde');
const e = Buffer.from('abcdef');

assert.ok(b.equals(c));
assert.ok(!c.equals(d));
assert.ok(!d.equals(e));
assert.ok(d.equals(d));
assert.ok(d.equals(new Uint8Array([0x61, 0x62, 0x63, 0x64, 0x65])));

assert.throws(
  () => Buffer.alloc(1).equals('abc'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

// equals works with ArrayBuffer
{
  const buf = Buffer.from([1, 2, 3]);
  const ab = new ArrayBuffer(3);
  new Uint8Array(ab).set([1, 2, 3]);
  assert.ok(buf.equals(ab));

  const ab2 = new ArrayBuffer(3);
  new Uint8Array(ab2).set([4, 5, 6]);
  assert.ok(!buf.equals(ab2));

  // Different length
  assert.ok(!buf.equals(new ArrayBuffer(4)));
}

// equals works with DataView
{
  const buf = Buffer.from([1, 2, 3]);
  const dv = new DataView(new ArrayBuffer(3));
  new Uint8Array(dv.buffer).set([1, 2, 3]);
  assert.ok(buf.equals(dv));

  const dv2 = new DataView(new ArrayBuffer(3));
  new Uint8Array(dv2.buffer).set([4, 5, 6]);
  assert.ok(!buf.equals(dv2));
}

// equals works with SharedArrayBuffer
{
  const buf = Buffer.from([1, 2, 3]);
  const sab = new SharedArrayBuffer(3);
  new Uint8Array(sab).set([1, 2, 3]);
  assert.ok(buf.equals(sab));

  const sab2 = new SharedArrayBuffer(3);
  new Uint8Array(sab2).set([4, 5, 6]);
  assert.ok(!buf.equals(sab2));
}

// equals works with other TypedArrays
{
  const buf = Buffer.from([0x01, 0x00, 0x02, 0x00]);
  const u16 = new Uint16Array([1, 2]);
  assert.ok(buf.equals(u16));
}

// equals with empty buffers of different types
{
  const buf = Buffer.alloc(0);
  assert.ok(buf.equals(new ArrayBuffer(0)));
  assert.ok(buf.equals(new DataView(new ArrayBuffer(0))));
  assert.ok(buf.equals(new SharedArrayBuffer(0)));
}
