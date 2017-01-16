'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt32BE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf]);
const result = buf.readInt32BE(1);

assert.strictEqual(result, -45552945);

assert.throws(
  () => buf.readInt32BE(2),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readInt32BE(2, true),
  'readInt32BE() should not throw if noAssert is true'
);
