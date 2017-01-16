'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt16LE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readUInt16LE(2);

assert.strictEqual(result, 0xea48);

assert.throws(
  () => buf.readUInt16LE(3),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readUInt16LE(5, true),
  'readUInt16LE() should not throw if noAssert is true'
);
