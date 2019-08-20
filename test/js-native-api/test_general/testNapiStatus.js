'use strict';

const common = require('../../common');
const addon = require(`./build/${common.buildType}/test_general`);
const assert = require('assert');

addon.createNapiError();
assert(addon.testNapiErrorCleanup(), 'napi_status cleaned up for second call');
