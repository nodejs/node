'use strict';
require('../common');
const assert = require('assert');

// testing basic functionality of readFloatBE()

const buf = Buffer.from([42, 84, 168, 127, 130]);
const result = buf.readFloatBE(1);

assert.strictEqual(result, 5789549854720);

assert.throws(
  () => {
    buf.readFloatBE(2);
  },
  /Index out of range/
);
