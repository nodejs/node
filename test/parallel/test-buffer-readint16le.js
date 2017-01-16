'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt16LE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readInt16LE(1);

assert.strictEqual(result, 0x48fd);

assert.throws(
  () => buf.readInt16LE(5),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readInt16LE(5, true),
  'readInt16LE() should not throw if noAssert is true'
);
