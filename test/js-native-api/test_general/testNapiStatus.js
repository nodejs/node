'use strict';
// Addons: test_general, test_general_vtable

const { addonPath } = require('../../common/addon-test');
const addon = require(addonPath);
const assert = require('assert');

addon.createNapiError();
assert(addon.testNapiErrorCleanup(), 'napi_status cleaned up for second call');
