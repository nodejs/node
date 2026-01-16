'use strict';
// Addons: test_uncaught_exception, test_uncaught_exception_vtable

const { addonPath } = require('../../common/addon-test');
const binding = require(addonPath);
const { testUncaughtException } = require('./uncaught_exception');

testUncaughtException(binding);
