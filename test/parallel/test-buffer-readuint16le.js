'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt16LE()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readUInt16LE(2);

assert.strictEqual(result, 32680);

assert.throws(
  () => {
    buf.readUInt16LE(3);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readUInt16LE(5, true);
  },
  'readUInt16LE() should not throw if noAssert is true'
);
