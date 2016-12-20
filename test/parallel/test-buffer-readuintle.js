'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUIntLE()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readUIntLE(2);

assert.strictEqual(result, 168);

assert.throws(
  () => {
    buf.readUIntLE(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readUIntLE(5, 0, true);
  },
  'readUIntLE() should not throw if noAssert is true'
);
