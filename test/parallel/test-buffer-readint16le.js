'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt16LE()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readInt16LE(1);

assert.strictEqual(result, -22444);

assert.throws(
  () => {
    buf.readInt16LE(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readInt16LE(5, true);
  },
  'readInt16LE() should not throw if noAssert is true'
);
