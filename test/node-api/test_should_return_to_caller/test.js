'use strict';

const common = require('../../common');
const assert = require('assert');
const util = require('util');
const { test } = require(
  `./build/${common.buildType}/test_should_return_to_caller`);

assert.throws(() => {
  util.callWithTimeout(10, test);
}, {
  type: EvalError,
});
