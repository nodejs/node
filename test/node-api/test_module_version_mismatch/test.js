'use strict';
const common = require('../../common');
const assert = require('assert');

// An add-on that requires a newer Node-API version than this binary supports
// must be rejected with an error that `require()` can catch, rather than
// crashing the process.
assert.throws(
  () => require(`./build/${common.buildType}/test_module_version_mismatch`),
  /requires Node-API version 2147483646, but this version of Node\.js only supports version \d+ add-ons\./);
