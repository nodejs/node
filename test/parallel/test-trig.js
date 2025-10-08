'use strict';

require('../common');
const { strictEqual } = require('assert');

// Test to verify that the trig functions work as expected with the
// v8_use_libm_trig_functions flag enabled.
let sum = 0;
for (let i = 0; i < 1_000_000; i++) {
  const result = Math.sin(Math.PI * i) * Math.cos(Math.PI * i);
  sum += result;
};

strictEqual(sum, -0.00006123284579142155);
