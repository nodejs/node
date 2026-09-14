'use strict';
const common = require('../../common');
const assert = require('assert');

// An add-on that requires a newer Node-API version than this binary supports
// must be rejected with an error that `require()` can catch. The version check
// in `node_napi_env__::New()` already produces that error, but its nullptr
// return used to be dereferenced by `napi_module_register_by_symbol()`, so the
// process segfaulted before the error could surface.
assert.throws(
  () => require(`./build/${common.buildType}/test_module_version_mismatch`),
  /requires Node-API version 2147483646, but this version of Node\.js only supports version \d+ add-ons\./);
