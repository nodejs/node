'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readFLoatLE()

const buf = Buffer.from([42, 84, 168, 127, 130]);
const result = buf.readFloatLE(1);

assert.strictEqual(result, -1.8782749018969955e-37);

assert.throws(
  () => buf.readFloatLE(2),
  /^RangeError: Index out of range$/
);
