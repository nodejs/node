'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readInt16BE()

const buf = Buffer.from([42, 84, 168, 127]);
const result = buf.readInt16BE(1);

assert.strictEqual(result, 21672);

assert.throws(
  () => {
    buf.readInt16BE(5);
  },
  /Index out of range/
);

assert.doesNotThrow(
  () => {
    buf.readInt16BE(5, true);
  },
  'readInt16BE() should not throw if noAssert is true'
);
