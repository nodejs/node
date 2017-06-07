'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt8()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readInt8(1);

assert.strictEqual(result, 84);

assert.throws(
  () => {
    buf.readInt8(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readInt8(5, true);
  },
  'readInt8() should not throw if noAssert is true'
);
