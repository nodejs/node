'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt32BE()

const buf = Buffer.from([42, 84, 168, 127, 130]);
const result = buf.readInt32BE(1);

assert.strictEqual(result, 1420328834);

assert.throws(
  () => {
    buf.readInt32BE(2);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readInt32BE(2, true);
  },
  'readInt32BE() should not throw if noAssert is true'
);
