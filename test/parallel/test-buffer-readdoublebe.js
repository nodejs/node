'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readDoubleBE()

const buf = Buffer.from([42, 84, 168, 127, 130, 34, 23, 200, 13]);
const result = buf.readDoubleBE(1);

assert.strictEqual(result, 6.697930247176003e+99);
assert.throws(
  () => buf.readDoubleBE(2),
  /^RangeError: Index out of range$/
);
