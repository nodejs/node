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
assert.strictEqual(typeof navigator.userAgent, 'string');
assert.match(navigator.userAgent, /^Node\.js\/\d+$/);

assert.strictEqual(navigator.appCodeName, 'Mozilla');
assert.strictEqual(navigator.appName, 'Netscape');
assert.match(navigator.appVersion, /^Node\.js\/\d+$/);
assert.strictEqual(navigator.product, 'Gecko');
assert.strictEqual(navigator.productSub, '20030107');
assert.strictEqual(navigator.vendor, '');
assert.strictEqual(navigator.vendorSub, '');
