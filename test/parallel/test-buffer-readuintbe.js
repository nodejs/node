'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUIntBE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readUIntBE(2);

assert.strictEqual(result, 0xfd);

assert.throws(
  () => buf.readUIntBE(5),
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => buf.readUIntBE(5, 0, true),
  'readUIntBE() should not throw if noAssert is true'
);
