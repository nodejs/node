'use strict';
require('../common');
const assert = require('assert');
const url = require('url');

// https://github.com/nodejs/node/pull/1036
const throws = [
  undefined,
  null,
  true,
  false,
  0,
  function() {}
];
for (let i = 0; i < throws.length; i++) {
  assert.throws(function() { url.format(throws[i]); }, TypeError);
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
