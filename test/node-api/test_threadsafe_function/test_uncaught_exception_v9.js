'use strict';
// Flags: --force-node-api-uncaught-exceptions-policy
// Addons: test_uncaught_exception_v9, test_uncaught_exception_v9_vtable

const { addonPath } = require('../../common/addon-test');
const binding = require(addonPath);
const { testUncaughtException } = require('./uncaught_exception');

testUncaughtException(binding);
