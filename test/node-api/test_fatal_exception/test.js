'use strict';
const common = require('../../common');
const assert = require('assert');
const test_fatal = require(`./build/${common.buildType}/test_fatal_exception`);

process.on('uncaughtException', common.mustCall(function(err) {
  assert.strictEqual(err.message, 'fatal error');
}));

const err = new Error('fatal error');
test_fatal.Test(err);
