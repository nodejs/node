'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readIntBE()

const buf = Buffer.from([42, 84, 167, 127]);
const result = buf.readIntBE(1);

assert.strictEqual(result, 42);

assert.throws(
  () => {
    buf.readIntBE(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readIntBE(5, 0, true);
  },
  'readIntBE() should not throw if noAssert is true'
);
