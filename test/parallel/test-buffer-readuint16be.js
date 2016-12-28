'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readUInt16BE()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readUInt16BE(2);

assert.strictEqual(result, 43135);

assert.throws(
  () => {
    buf.readUInt16BE(3);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readUInt16BE(5, true);
  },
  'readUInt16BE() should not throw if noAssert is true'
);
