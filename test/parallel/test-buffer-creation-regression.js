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

const testCases = [
  [200, 50, 100],
  [4294967296 /* 1 << 32 */, 2147483648 /* 1 << 31 */, 1000],
  [8589934592 /* 1 << 33 */, 4294967296 /* 1 << 32 */, 1000]
];

for (let index = 0, arrayBuffer; index < testCases.length; index += 1) {
  const [size, offset, length] = testCases[index];

  try {
    arrayBuffer = new ArrayBuffer(size);
  } catch (e) {
    if (e instanceof RangeError && acceptableOOMErrors.includes(e.message))
      return common.skip(`Unable to allocate ${size} bytes for ArrayBuffer`);
    throw e;
  }

  test(arrayBuffer, offset, length);
}
