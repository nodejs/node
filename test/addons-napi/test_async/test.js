'use strict';
const common = require('../../common');
const assert = require('assert');
const test_async = require(`./build/${common.buildType}/test_async`);

test_async(5, common.mustCall(function(err, val) {
  assert.strictEqual(err, null);
  assert.strictEqual(val, 10);
  process.nextTick(common.mustCall(function() {}));
}));
