'use strict';
const common = require('../common');
var assert = require('assert');

setTimeout(common.mustCall(function() {
  const a = require('../fixtures/a');
  assert.strictEqual(true, 'A' in a);
  assert.strictEqual('A', a.A());
  assert.strictEqual('D', a.D());
}), 50);
