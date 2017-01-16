'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readIntBE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readIntBE(1, 1);

assert.strictEqual(result, -3);

assert.throws(
  () => buf.readIntBE(5, 1),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readIntBE(5, 1, true),
  'readIntBE() should not throw if noAssert is true'
);
