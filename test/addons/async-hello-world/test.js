'use strict';
const common = require('../../common');
const assert = require('assert');
const { runCall } = require(`./build/${common.buildType}/binding`);

runCall(5, common.mustCall((err, val) => {
  assert.strictEqual(err, null);
  assert.strictEqual(val, 10);
  process.nextTick(common.mustCall());
}));
