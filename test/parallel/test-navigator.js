'use strict';

require('../common');
const assert = require('assert');

const is = {
  number: (value, key) => {
    assert(!Number.isNaN(value), `${key} should not be NaN`);
    assert.strictEqual(typeof value, 'number');
  },
};

is.number(+navigator.hardwareConcurrency, 'hardwareConcurrency');
is.number(navigator.hardwareConcurrency, 'hardwareConcurrency');
assert.ok(navigator.hardwareConcurrency > 0);
