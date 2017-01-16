'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt8()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readUInt8(2);

assert.strictEqual(result, 0x48);

assert.throws(
  () => buf.readUInt8(5),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readUInt8(5, true),
  'readUInt8() should not throw if noAssert is true'
);
