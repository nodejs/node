'use strict';

require('../common');
const assert = require('assert');

const b = Buffer.allocUnsafe(1024);
const c = Buffer.allocUnsafe(512);

let cntr = 0;

{
  // copy 512 bytes, from 0 to 512.
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, 0, 512);
  assert.strictEqual(copied, 512);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // Current behavior is to coerce values to integers.
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, '0', '0', '512');
  assert.strictEqual(copied, 512);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // Floats will be converted to integers via `Math.floor`
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, 0, 512.5);
  assert.strictEqual(copied, 512);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // Copy c into b, without specifying sourceEnd
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = c.copy(b, 0, 0);
  assert.strictEqual(copied, c.length);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

{
  // Copy c into b, without specifying sourceStart
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = c.copy(b, 0);
  assert.strictEqual(copied, c.length);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

{
  // Copied source range greater than source length
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = c.copy(b, 0, 0, c.length + 1);
  assert.strictEqual(copied, c.length);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

{
  // Copy longer buffer b to shorter c without targetStart
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c);
  assert.strictEqual(copied, c.length);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // Copy starting near end of b to c
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, b.length - Math.floor(c.length / 2));
  assert.strictEqual(copied, Math.floor(c.length / 2));
  for (let i = 0; i < Math.floor(c.length / 2); i++) {
    assert.strictEqual(c[i], b[b.length - Math.floor(c.length / 2) + i]);
  }
  for (let i = Math.floor(c.length / 2) + 1; i < c.length; i++) {
    assert.strictEqual(c[c.length - 1], c[i]);
  }
}

{
  // Try to copy 513 bytes, and check we don't overrun c
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, 0, 513);
  assert.strictEqual(copied, c.length);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // copy 768 bytes from b into b
  b.fill(++cntr);
  b.fill(++cntr, 256);
  const copied = b.copy(b, 0, 256, 1024);
  assert.strictEqual(copied, 768);
  for (let i = 0; i < b.length; i++) {
    assert.strictEqual(b[i], cntr);
  }
}

// Copy string longer than buffer length (failure will segfault)
const bb = Buffer.allocUnsafe(10);
bb.fill('hello crazy world');


// Try to copy from before the beginning of b. Should not throw.
b.copy(c, 0, 100, 10);

// Throw with invalid source type
assert.throws(
  () => Buffer.prototype.copy.call(0),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

// Copy throws at negative targetStart
assert.throws(
  () => Buffer.allocUnsafe(5).copy(Buffer.allocUnsafe(5), -1, 0),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "targetStart" is out of range. ' +
             'It must be >= 0. Received -1'
  }
);

// Copy throws at negative sourceStart
assert.throws(
  () => Buffer.allocUnsafe(5).copy(Buffer.allocUnsafe(5), 0, -1),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

// Copy throws if sourceStart is greater than length of source
assert.throws(
  () => Buffer.allocUnsafe(5).copy(Buffer.allocUnsafe(5), 0, 100),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

{
  // Check sourceEnd resets to targetEnd if former is greater than the latter
  b.fill(++cntr);
  c.fill(++cntr);
  b.copy(c, 0, 0, 1025);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

// Throw with negative sourceEnd
assert.throws(
  () => b.copy(c, 0, 0, -1),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "sourceEnd" is out of range. ' +
             'It must be >= 0. Received -1'
  }
);

// When sourceStart is greater than sourceEnd, zero copied
assert.strictEqual(b.copy(c, 0, 100, 10), 0);

// When targetStart > targetLength, zero copied
assert.strictEqual(b.copy(c, 512, 0, 10), 0);

// Test that the `target` can be a Uint8Array.
{
  const d = new Uint8Array(c);
  // copy 512 bytes, from 0 to 512.
  b.fill(++cntr);
  d.fill(++cntr);
  const copied = b.copy(d, 0, 0, 512);
  assert.strictEqual(copied, 512);
  for (let i = 0; i < d.length; i++) {
    assert.strictEqual(d[i], b[i]);
  }
}

// Test that the source can be a Uint8Array, too.
{
  const e = new Uint8Array(b);
  // copy 512 bytes, from 0 to 512.
  e.fill(++cntr);
  c.fill(++cntr);
  const copied = Buffer.prototype.copy.call(e, c, 0, 0, 512);
  assert.strictEqual(copied, 512);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], e[i]);
  }
}

// https://github.com/nodejs/node/issues/23668: Do not crash for invalid input.
c.fill('c');
b.copy(c, 'not a valid offset');
// Make sure this acted like a regular copy with `0` offset.
assert.deepStrictEqual(c, b.slice(0, c.length));

// Copy into a Uint16Array target; bytes are packed into 16-bit elements.
{
  const x = new Uint16Array(4);
  const buf = Buffer.of(1, 2, 3, 4);
  const copied = buf.copy(x);
  assert.strictEqual(copied, 4);
  assert.ok(x instanceof Uint16Array);
  const bytes = new Uint8Array(x.buffer, x.byteOffset, 4);
  assert.deepStrictEqual(Array.from(bytes), [1, 2, 3, 4]);
  const remaining = new Uint8Array(
    x.buffer,
    x.byteOffset + 4,
    x.byteLength - 4
  );
  assert.ok(remaining.every((b) => b === 0));
}

{
  c.fill('C');
  assert.throws(() => {
    b.copy(c, { [Symbol.toPrimitive]() { throw new Error('foo'); } });
  }, /foo/);
  // No copying took place:
  assert.deepStrictEqual(c.toString(), 'C'.repeat(c.length));
}

// Copying to/from SharedArrayBuffer-backed buffers. The relaxed-atomic copy
// path is used only when both sides are backed by a SharedArrayBuffer; mixed
// copies (one shared, one not) go through the regular non-atomic overload.
{
  // SharedArrayBuffer -> SharedArrayBuffer.
  const src = Buffer.from(new SharedArrayBuffer(512)).fill(0x61);
  const dst = Buffer.from(new SharedArrayBuffer(512)).fill(0x62);
  const copied = src.copy(dst, 0, 0, 512);
  assert.strictEqual(copied, 512);
  assert.deepStrictEqual(Buffer.from(dst), Buffer.from(src));
}

{
  // SharedArrayBuffer source -> regular Buffer target (mixed).
  const src = Buffer.from(new SharedArrayBuffer(256)).fill(0x63);
  const dst = Buffer.allocUnsafe(256).fill(0x64);
  assert.strictEqual(src.copy(dst), 256);
  assert.deepStrictEqual(dst, Buffer.from(src));
}

{
  // Regular Buffer source -> SharedArrayBuffer target (mixed).
  const src = Buffer.allocUnsafe(256).fill(0x65);
  const dst = Buffer.from(new SharedArrayBuffer(256)).fill(0x66);
  assert.strictEqual(src.copy(dst), 256);
  assert.deepStrictEqual(Buffer.from(dst), src);
}

{
  // Views with a non-zero byteOffset over a SharedArrayBuffer. The native copy
  // is relative to the underlying ArrayBuffer, so the view's byteOffset must be
  // accounted for.
  const sab = new SharedArrayBuffer(16);
  const whole = Buffer.from(sab);
  for (let i = 0; i < 16; i++) whole[i] = i;
  const src = Buffer.from(sab, 4, 8);   // sab bytes 4..11
  const dst = Buffer.from(sab, 12, 4);  // sab bytes 12..15
  assert.strictEqual(src.copy(dst, 0, 0, 4), 4);
  assert.deepStrictEqual(
    [...whole],
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 4, 5, 6, 7]);
}

{
  // Overlapping copy within a single SharedArrayBuffer uses memmove semantics.
  const buf = Buffer.from(new SharedArrayBuffer(8));
  for (let i = 0; i < 8; i++) buf[i] = i + 1; // [1,2,3,4,5,6,7,8]
  assert.strictEqual(buf.copy(buf, 2, 0, 6), 6);
  assert.deepStrictEqual([...buf], [1, 2, 1, 2, 3, 4, 5, 6]);
}
