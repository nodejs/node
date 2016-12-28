'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt32LE()

const buf = Buffer.from([42, 84, 168, 127, 130]);
const result = buf.readInt32LE(1);

assert.strictEqual(result, -2105563052);

assert.throws(
  () => {
    buf.readInt32LE(2);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readInt32LE(2, true);
  },
  'readInt16LE() should not throw if noAssert is true'
);
