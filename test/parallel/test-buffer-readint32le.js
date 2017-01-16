'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt32LE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf]);
const result = buf.readInt32LE(1);

assert.strictEqual(result, -806729475);

assert.throws(
  () => buf.readInt32LE(2),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readInt32LE(2, true),
  'readInt16LE() should not throw if noAssert is true'
);
