'use strict';
var common = require('../common');
var path = require('path');
var assert = require('assert');

setTimeout(common.mustCall(function() {
  const a = require(path.join(common.fixturesDir, 'a'));
  assert.strictEqual(true, 'A' in a);
  assert.strictEqual('A', a.A());
  assert.strictEqual('D', a.D());
}), 50);
