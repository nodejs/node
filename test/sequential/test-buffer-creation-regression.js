'use strict';

const common = require('../common');
const assert = require('assert');

function test(arrayBuffer, offset, length) {
  const uint8Array = new Uint8Array(arrayBuffer, offset, length);
  for (let i = 0; i < length; i += 1) {
    uint8Array[i] = 1;
  }

  const buffer = Buffer.from(arrayBuffer, offset, length);
  for (let i = 0; i < length; i += 1) {
    assert.strictEqual(buffer[i], 1);
  }
}

const acceptableOOMErrors = [
  'Array buffer allocation failed',
  'Invalid array buffer length'
];

const length = 1000;
const offset = 4294967296; /* 1 << 32 */
const size = offset + length;
let arrayBuffer;

try {
  arrayBuffer = new ArrayBuffer(size);
} catch (e) {
  if (e instanceof RangeError && acceptableOOMErrors.includes(e.message))
    common.skip(`Unable to allocate ${size} bytes for ArrayBuffer`);
  throw e;
}

test(arrayBuffer, offset, length);
