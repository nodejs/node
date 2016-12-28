'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readIntLE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readIntLE(2, 1);

assert.strictEqual(result, 0x48);

assert.throws(
  () => buf.readIntLE(5, 1),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readIntLE(5, 1, true),
  'readIntLE() should not throw if noAssert is true'
);
