'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(0.6840442338072671 ** 2.4, 0.4019777798321958);

const constants = {
  '0': 1,
  '-1': 0.1,
  '-2': 0.01,
  '-3': 0.001,
  '-4': 0.0001,
  '-5': 0.00001,
  '-6': 0.000001,
  '-7': 0.0000001,
  '-8': 0.00000001,
  '-9': 0.000000001,
};

for (let i = 0; i > -10; i -= 1) {
  assert.strictEqual(10 ** i, constants[i]);
  assert.strictEqual(10 ** i, 1 / (10 ** -i));
}
