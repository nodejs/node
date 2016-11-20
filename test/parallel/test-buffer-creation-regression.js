'use strict';

require('../common');
const assert = require('assert');

function test(size, offset, length) {
  const arrayBuffer = new ArrayBuffer(size);

  const uint8Array = new Uint8Array(arrayBuffer, offset, length);
  for (let i = 0; i < length; i += 1) {
    uint8Array[i] = 1;
  }

  const buffer = Buffer.from(arrayBuffer, offset, length);
  for (let i = 0; i < length; i += 1) {
    assert.strictEqual(buffer[i], 1);
  }
}

test(200, 50, 100);
test(8589934592 /* 1 << 40 */, 4294967296 /* 1 << 39 */, 1000);
