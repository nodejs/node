'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt16BE()

const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea]);
const result = buf.readInt16BE(1);

assert.strictEqual(result, -696);

assert.throws(
  () => {
    buf.readInt16BE(5);
  },
  /^RangeError: Index out of range$/
);

assert.doesNotThrow(
  () => {
    buf.readInt16BE(5, true);
  },
  'readInt16BE() should not throw if noAssert is true'
);
