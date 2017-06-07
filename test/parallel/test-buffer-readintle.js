'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readIntLE()

const buf = Buffer.from([42, 84, 167, 127]);
const result = buf.readIntLE(0);

assert.strictEqual(result, 42);

assert.throws(
  () => {
    buf.readIntLE(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readIntLE(5, 0, true);
  },
  'readIntLE() should not throw if noAssert is true'
);
