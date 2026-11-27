'use strict';
const common = require('../../common');
const assert = require('assert');

// An exception thrown by the init function must reach the caller of
// require(), even when the init function also returns new exports.
assert.throws(
  () => require(`./build/${common.buildType}/test_init_exception`),
  { message: 'Error during Init' },
);
