'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt16BE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readUInt16BE(2);

assert.strictEqual(result, 0x48ea);

assert.throws(
  () => buf.readUInt16BE(3),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readUInt16BE(5, true),
  'readUInt16BE() should not throw if noAssert is true'
);
