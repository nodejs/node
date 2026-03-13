'use strict';
const common = require('../../common');
const assert = require('assert');
const test_async = require(`./build/${common.buildType}/test_async`);

process.on('uncaughtException', common.mustCall(function(err) {
  try {
    throw new Error('should not fail');
  } catch (err) {
    assert.strictEqual(err.message, 'should not fail');
  }
  assert.strictEqual(err.message, 'uncaught');
}));

// Successful async execution and completion callback.
test_async.Test(5, {}, common.mustCall(function() {
  throw new Error('uncaught');
}));
