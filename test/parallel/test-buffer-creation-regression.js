'use strict';

const common = require('../common');
const assert = require('assert');
const os = require('os');

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

const testCases = [
  [200, 50, 100],
  [4294967296 /* 1 << 32 */, 2147483648 /* 1 << 31 */, 1000],
  [8589934592 /* 1 << 33 */, 4294967296 /* 1 << 32 */, 1000]
];

for (let caseIndex = 0; caseIndex < testCases.length; caseIndex += 1) {
  const [size, offset, length] = testCases[caseIndex];
  if (os.freemem() < size) {
    return common.skip('Not enough memory to run the test');
  }
  test(size, offset, length);
}
