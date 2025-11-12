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

// This can be somewhat unreliable because the
// allocated memory might just already happen to
// contain all zeroes. The test is run multiple
// times to improve the reliability.
for (let i = 0; i < 50; i++) {
  const bufs = [
    Buffer.alloc(20),
    Buffer.allocUnsafe(20),
    Buffer.allocUnsafeSlow(20),
    Buffer(20),
  ];
  for (const buf of bufs) {
    assert(isZeroFilled(buf));
  }
}
