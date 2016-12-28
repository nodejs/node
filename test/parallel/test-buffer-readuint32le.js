'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt32LE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf]);
const result = buf.readUInt32LE(1);

assert.strictEqual(result, 0xcfea48fd);

assert.throws(
  () => buf.readUInt32LE(2),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readUInt32LE(2, true),
  'readUInt32LE() should not throw if noAssert is true'
);
