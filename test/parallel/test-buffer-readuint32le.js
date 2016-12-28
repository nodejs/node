'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt32LE()

const buf = Buffer.from([42, 84, 168, 127, 130]);
const result = buf.readUInt32LE(1);

assert.strictEqual(result, 2189404244);

assert.throws(
  () => {
    buf.readUInt32LE(2);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readUInt32LE(2, true);
  },
  'readUInt32LE() should not throw if noAssert is true'
);
