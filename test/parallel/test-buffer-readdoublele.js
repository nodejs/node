'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readDoubleLE()

const buf = Buffer.from([42, 84, 168, 127, 130, 34, 23, 200, 13]);
const result = buf.readDoubleLE(1);

assert.strictEqual(result, 2.822519658218711e-242);

assert.throws(
  () => {
    buf.readDoubleLE(2);
  },
  /Index out of range/
);
