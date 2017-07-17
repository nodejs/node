'use strict';
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

setTimeout(common.mustCall(function() {
  const a = require(fixtures.path('a'));
  assert.strictEqual(true, 'A' in a);
  assert.strictEqual('A', a.A());
  assert.strictEqual('D', a.D());
}), 50);
