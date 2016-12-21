'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUIntBE()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readUIntBE(2);

assert.strictEqual(result, 84);

assert.throws(
  () => {
    buf.readUIntBE(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readUIntBE(5, 0, true);
  },
  'readUIntBE() should not throw if noAssert is true'
);
