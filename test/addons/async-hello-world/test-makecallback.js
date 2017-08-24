'use strict';
const common = require('../../common');
const assert = require('assert');
const { runMakeCallback } = require(`./build/${common.buildType}/binding`);

runMakeCallback(5, common.mustCall(function(err, val) {
  assert.strictEqual(err, null);
  assert.strictEqual(val, 10);
  process.nextTick(common.mustCall());
}));
