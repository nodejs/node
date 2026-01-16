'use strict';
// Addons: test_fatal_exception, test_fatal_exception_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const test_fatal = require(addonPath);

process.on('uncaughtException', common.mustCall(function(err) {
  assert.strictEqual(err.message, 'fatal error');
}));

const err = new Error('fatal error');
test_fatal.Test(err);
