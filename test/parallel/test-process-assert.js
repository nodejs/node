'use strict';
require('../common');
const assert = require('assert');

assert.strictEqual(process.assert(1, 'error'), undefined);
assert.throws(() => {
  process.assert(undefined, 'errorMessage');
}, /^Error: errorMessage$/);
assert.throws(() => {
  process.assert(false);
}, /^Error: assertion error$/);
