'use strict';
// Flags: --force-node-api-uncaught-exceptions-policy

const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_uncaught_exception_v9`);
const { testUncaughtException } = require('./uncaught_exception');

testUncaughtException(binding);
