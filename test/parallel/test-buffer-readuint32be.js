'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt32BE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf]);
const result = buf.readUInt32BE(1);

assert.strictEqual(result, 0xfd48eacf);

assert.throws(
  () => buf.readUInt32BE(2),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readUInt32BE(2, true),
  'readUInt32BE() should not throw if noAssert is true'
);
