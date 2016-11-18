'use strict';

require('../common');
const assert = require('assert');

const b = Buffer.allocUnsafe(1024);
const c = Buffer.allocUnsafe(512);
var cntr = 0;

{
  // copy 512 bytes, from 0 to 512.
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, 0, 512);
  assert.strictEqual(512, copied);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

{
  // copy c into b, without specifying sourceEnd
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = c.copy(b, 0, 0);
  assert.strictEqual(c.length, copied);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // copy c into b, without specifying sourceStart
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = c.copy(b, 0);
  assert.strictEqual(c.length, copied);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(c[i], b[i]);
  }
}

{
  // copy longer buffer b to shorter c without targetStart
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c);
  assert.strictEqual(c.length, copied);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

{
  // copy starting near end of b to c
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, b.length - Math.floor(c.length / 2));
  assert.strictEqual(Math.floor(c.length / 2), copied);
  for (let i = 0; i < Math.floor(c.length / 2); i++) {
    assert.strictEqual(b[b.length - Math.floor(c.length / 2) + i], c[i]);
  }
  for (let i = Math.floor(c.length / 2) + 1; i < c.length; i++) {
    assert.strictEqual(c[c.length - 1], c[i]);
  }
}

{
  // try to copy 513 bytes, and check we don't overrun c
  b.fill(++cntr);
  c.fill(++cntr);
  const copied = b.copy(c, 0, 0, 513);
  assert.strictEqual(c.length, copied);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

{
  // copy 768 bytes from b into b
  b.fill(++cntr);
  b.fill(++cntr, 256);
  const copied = b.copy(b, 0, 256, 1024);
  assert.strictEqual(768, copied);
  for (let i = 0; i < b.length; i++) {
    assert.strictEqual(cntr, b[i]);
  }
}

// copy string longer than buffer length (failure will segfault)
const bb = Buffer.allocUnsafe(10);
bb.fill('hello crazy world');


// try to copy from before the beginning of b
assert.doesNotThrow(() => { b.copy(c, 0, 100, 10); });

// copy throws at negative sourceStart
assert.throws(function() {
  Buffer.allocUnsafe(5).copy(Buffer.allocUnsafe(5), 0, -1);
}, RangeError);

{
  // check sourceEnd resets to targetEnd if former is greater than the latter
  b.fill(++cntr);
  c.fill(++cntr);
  b.copy(c, 0, 0, 1025);
  for (let i = 0; i < c.length; i++) {
    assert.strictEqual(b[i], c[i]);
  }
}

// throw with negative sourceEnd
assert.throws(() => b.copy(c, 0, 0, -1), RangeError);

// when sourceStart is greater than sourceEnd, zero copied
assert.strictEqual(b.copy(c, 0, 100, 10), 0);

// when targetStart > targetLength, zero copied
assert.strictEqual(b.copy(c, 512, 0, 10), 0);
