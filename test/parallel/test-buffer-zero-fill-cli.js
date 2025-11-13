'use strict';
// Flags: --zero-fill-buffers

// when using --zero-fill-buffers, every Buffer
// instance must be zero filled upon creation

require('../common');
const { Buffer } = require('buffer');
const assert = require('assert');

function isZeroFilled(buf) {
  for (const n of buf)
    if (n > 0) return false;
  return true;
}

// We have to consume the data from the pool as otherwise
// we would be testing what's in snapshot, which is zero-filled
// regardless of the flag presence, and we want to test the flag
for (let i = 0; i < 8; i++) {
  assert(isZeroFilled(Buffer.allocUnsafe(1024)));
}

// This can be somewhat unreliable because the
// allocated memory might just already happen to
// contain all zeroes. The test is run multiple
// times to improve the reliability.
for (let i = 0; i < 50; i++) {
  const bufs = [
    Buffer.alloc(20),
    Buffer.allocUnsafe(20),
    Buffer.allocUnsafeSlow(20), // Heap
    Buffer.allocUnsafeSlow(128), // Alloc
    Buffer(20),
  ];
  for (const buf of bufs) {
    assert(isZeroFilled(buf));
  }
}
