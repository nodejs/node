'use strict';
const common = require('../../common');
const assert = require('assert');

assert.throws(
  () => require(`./build/${common.buildType}/test_null_init`),
  /Module has no declared entry point[.]/);
